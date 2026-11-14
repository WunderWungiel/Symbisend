// SPDX-FileCopyrightText: 2026 Wunder Wungiel
// SPDX-License-Identifier: GPL-3.0-or-later

#include "gallerythumbnails.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QImageReader>
#include <QMutex>
#include <QMutexLocker>
#include <QSet>
#include <QTransform>
#include <QUrl>

#ifndef Q_OS_SYMBIAN
#include <QDesktopServices>
#endif

// Cache limit (about 5 KB a thumbnail, so ~2000 photos). Above it, only
// the newest photos' thumbnails are kept, up to kPruneToBytes, so pruning
// isn't needed on every visit. The prefetcher stops at kPruneToBytes, so on a
// larger gallery the newest photos are cached and older ones only when
// scrolled to.
static const qint64 kMaxCacheBytes = 10 * 1024 * 1024;
static const qint64 kPruneToBytes = 8 * 1024 * 1024;

// ------------------------------------------------------------------ EXIF

static quint32 rd16(const uchar *p, bool le)
{
    return le ? quint32(p[0]) | (quint32(p[1]) << 8)
              : (quint32(p[0]) << 8) | quint32(p[1]);
}

static quint32 rd32(const uchar *p, bool le)
{
    return le ? rd16(p, true) | (rd16(p + 2, true) << 16)
              : (rd16(p, false) << 16) | rd16(p + 2, false);
}

// TIFF structure inside the EXIF segment: IFD0 has the orientation, IFD1
// the thumbnail's offset and length. Every offset is checked against len.
static QByteArray parseTiff(const uchar *t, qint64 len, int *orientation)
{
    if (len < 8)
        return QByteArray();
    const bool le = t[0] == 'I' && t[1] == 'I';
    if (!le && !(t[0] == 'M' && t[1] == 'M'))
        return QByteArray();

    const qint64 ifd0 = rd32(t + 4, le);
    if (ifd0 + 2 > len)
        return QByteArray();
    const qint64 n0 = rd16(t + ifd0, le);
    if (ifd0 + 2 + n0 * 12 + 4 > len)
        return QByteArray();
    for (qint64 i = 0; i < n0; ++i) {
        const uchar *e = t + ifd0 + 2 + i * 12;
        if (rd16(e, le) == 0x0112 && rd16(e + 2, le) == 3) // Orientation, SHORT
            *orientation = int(rd16(e + 8, le));
    }

    const qint64 ifd1 = rd32(t + ifd0 + 2 + n0 * 12, le);
    if (ifd1 == 0 || ifd1 + 2 > len)
        return QByteArray();
    const qint64 n1 = rd16(t + ifd1, le);
    if (ifd1 + 2 + n1 * 12 > len)
        return QByteArray();
    qint64 offset = 0, length = 0;
    for (qint64 i = 0; i < n1; ++i) {
        const uchar *e = t + ifd1 + 2 + i * 12;
        const quint32 tag = rd16(e, le);
        if (tag == 0x0201)      // JPEGInterchangeFormat
            offset = rd32(e + 8, le);
        else if (tag == 0x0202) // JPEGInterchangeFormatLength
            length = rd32(e + 8, le);
    }
    if (offset <= 0 || length <= 0 || offset + length > len)
        return QByteArray();
    return QByteArray(reinterpret_cast<const char *>(t + offset), int(length));
}

// The TIFF data of a JPEG's EXIF segment (APP1), read by walking the
// segment headers: only that segment is read. Empty if there is none.
static QByteArray readExif(QFile &f)
{
    uchar h[4];
    if (f.read(reinterpret_cast<char *>(h), 2) != 2 || h[0] != 0xFF || h[1] != 0xD8)
        return QByteArray();
    for (int segments = 0; segments < 32; ++segments) {
        if (f.read(reinterpret_cast<char *>(h), 4) != 4 || h[0] != 0xFF)
            return QByteArray();
        const uchar marker = h[1];
        if (marker == 0xDA || marker == 0xD9) // image data / end: no EXIF
            return QByteArray();
        const int len = int(rd16(h + 2, false)) - 2;
        if (len < 0)
            return QByteArray();
        if (marker == 0xE1) {
            const QByteArray seg = f.read(len);
            if (seg.size() != len)
                return QByteArray();
            if (len >= 14 && seg.startsWith(QByteArray("Exif\0\0", 6)))
                return seg.mid(6);
            // another APP1 (XMP): go on
        } else if (!f.seek(f.pos() + len)) {
            return QByteArray();
        }
    }
    return QByteArray();
}

static QImage oriented(const QImage &img, int orientation)
{
    QTransform t;
    switch (orientation) {
    case 2: t.scale(-1, 1); break;
    case 3: t.rotate(180); break;
    case 4: t.scale(1, -1); break;
    case 5: t.rotate(90); t.scale(-1, 1); break;
    case 6: t.rotate(90); break;
    case 7: t.rotate(-90); t.scale(-1, 1); break;
    case 8: t.rotate(-90); break;
    default: return img;
    }
    return img.transformed(t);
}

// ------------------------------------------------------------------ timings

enum Method { FromCache, FromExif, FromDecode, MethodCount };

// Per user (QML's loader or the prefetcher): count and time per method,
// logged every `every` thumbnails.
struct Stats {
    const char *who;
    int every;
    int count[MethodCount];
    qint64 ms[MethodCount];
    int total;
};

static QMutex g_statsMutex;
static Stats g_viewStats = { "shown", 25, { 0, 0, 0 }, { 0, 0, 0 }, 0 };
static Stats g_prefetchStats = { "prefetched", 50, { 0, 0, 0 }, { 0, 0, 0 }, 0 };

static void record(Stats &s, Method m, qint64 ms)
{
    QMutexLocker lock(&g_statsMutex);
    s.count[m] += 1;
    s.ms[m] += ms;
    if (++s.total % s.every != 0)
        return;
    static const char *const names[MethodCount] = { "cache", "exif", "decode" };
    QString line = QString::fromLatin1("Gallery: thumbnails %1: %2 so far").arg(QLatin1String(s.who)).arg(s.total);
    for (int i = 0; i < MethodCount; ++i) {
        if (s.count[i] > 0)
            line += QString::fromLatin1(", %1 %2 (avg %3 ms)").arg(QLatin1String(names[i]))
                    .arg(s.count[i]).arg(s.ms[i] / s.count[i]);
    }
    qDebug() << qPrintable(line);
}

// ------------------------------------------------------------------ cache

// File-scope rather than function-local statics: those aren't guaranteed to
// be initialised thread-safely before C++11, and QML's loader thread and the
// prefetcher make their first call at about the same time.
static QMutex g_cacheDirMutex;
static QString g_cacheDir;

static QString cacheDir()
{
    QMutexLocker lock(&g_cacheDirMutex);
    QString &dir = g_cacheDir;
    if (dir.isEmpty()) {
#ifdef Q_OS_SYMBIAN
        // The private folder (applicationDirPath() is that), no capabilities needed.
        QString base = QCoreApplication::applicationDirPath();
#else
        QString base = QDesktopServices::storageLocation(QDesktopServices::DataLocation);
        if (base.isEmpty())
            base = QDir::homePath() + QLatin1String("/.symbisend");
#endif
        dir = QDir::fromNativeSeparators(base) + QLatin1String("/thumbs");
        if (!QDir().mkpath(dir))
            qWarning() << "Gallery: cannot create the thumbnail cache" << dir;
    }
    return dir;
}

// The cache file's name for a photo.
static QString cacheName(const QFileInfo &fi)
{
    const QString key = fi.absoluteFilePath().toLower() + QLatin1Char('|')
                        + QString::number(fi.size()) + QLatin1Char('|')
                        + QString::number(fi.lastModified().toTime_t());
    return QString::fromLatin1(QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Md5).toHex())
           + QLatin1String(".jpg");
}

static QString cachePath(const QFileInfo &fi)
{
    return cacheDir() + QLatin1Char('/') + cacheName(fi);
}

QSize GalleryThumbs::cacheSize()
{
    return QSize(128, 128);
}

// Makes a thumbnail covering cacheSize(); null if the file can't be read.
static QImage make(const QString &path, Method *method)
{
    const QSize target = GalleryThumbs::cacheSize();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Gallery: cannot open" << path;
        return QImage();
    }

    int orientation = 1;
    QImage img;
    const QByteArray exif = readExif(file);
    if (!exif.isEmpty()) {
        const QByteArray thumb = parseTiff(reinterpret_cast<const uchar *>(exif.constData()),
                                           exif.size(), &orientation);
        if (!thumb.isEmpty() && img.loadFromData(thumb, "JPG"))
            *method = FromExif;
    }

    if (img.isNull()) {
        *method = FromDecode;
        file.seek(0);
        QImageReader reader(&file);
        const QSize full = reader.size();
        if (full.isValid()) {
            QSize cover = full; // QSize::scaled() is Qt 4.8
            cover.scale(target, Qt::KeepAspectRatioByExpanding);
            if (cover.width() < full.width())
                reader.setScaledSize(cover);
        }
        img = reader.read();
        if (img.isNull()) {
            qWarning() << "Gallery: no thumbnail for" << path << reader.errorString();
            return QImage();
        }
    }

    img = oriented(img, orientation);
    if (img.width() > target.width() && img.height() > target.height())
        img = img.scaled(target, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    return img;
}

// From the cache, or made and cached. With onlyEnsure, a cached thumbnail
// isn't read (returns a null image then; *cached says so). *bytes gets the
// size of its cache file (0 if it couldn't be cached); onlyEnsure only.
static QImage loadThumb(const QString &path, Stats &stats, bool onlyEnsure, bool *cached,
                        qint64 *bytes = 0)
{
    if (bytes)
        *bytes = 0;
    QElapsedTimer timer;
    timer.start();
    *cached = false;
    const QFileInfo fi(path);
    if (!fi.isFile())
        return QImage();
    const QString cp = cachePath(fi);

    if (onlyEnsure) {
        const QFileInfo cfi(cp);
        if (cfi.exists()) {
            *cached = true;
            if (bytes)
                *bytes = cfi.size();
            return QImage();
        }
    }
    QImage img;
    if (!onlyEnsure && img.load(cp, "JPG")) {
        *cached = true;
        record(stats, FromCache, timer.elapsed());
        return img;
    }

    Method method = FromDecode;
    img = make(path, &method);
    if (img.isNull())
        return img;
    // Written under another name first: QML's loader and the prefetcher may
    // make the same one at once, and a half-written file must not be read.
    const QString temp = cp + QLatin1String(".") + QString::number(quintptr(QThread::currentThreadId())) + QLatin1String(".tmp");
    if (img.save(temp, "JPG", 85)) {
        const qint64 size = QFileInfo(temp).size();
        if (QFile::rename(temp, cp)) {
            if (bytes)
                *bytes = size;
        } else {
            QFile::remove(temp); // made by the other thread meanwhile
            if (bytes)
                *bytes = QFileInfo(cp).size();
        }
    } else {
        qWarning() << "Gallery: cannot write" << temp;
        QFile::remove(temp);
    }
    record(stats, method, timer.elapsed());
    return img;
}

QImage GalleryThumbs::load(const QString &path)
{
    bool cached;
    return loadThumb(path, g_viewStats, false, &cached);
}

qint64 GalleryThumbs::prune(const QStringList &newestFirst)
{
    const QDir dir(cacheDir());
    const QFileInfoList files = dir.entryInfoList(QStringList() << QLatin1String("*.jpg") << QLatin1String("*.tmp"),
                                                  QDir::Files);
    // A .tmp file is being written by QML's loader thread, unless it was
    // left behind (the app died while writing): only those old ones go.
    const QDateTime staleBefore = QDateTime::currentDateTime().addSecs(-60);
    QHash<QString, qint64> sizes; // cache file name -> size
    qint64 total = 0;
    int staleTemps = 0;
    foreach (const QFileInfo &fi, files) {
        if (fi.suffix() == QLatin1String("tmp")) {
            if (fi.lastModified() < staleBefore && QFile::remove(fi.absoluteFilePath()))
                ++staleTemps;
            continue;
        }
        sizes.insert(fi.fileName(), fi.size());
        total += fi.size();
    }
    if (staleTemps > 0)
        qDebug() << "Gallery: removed" << staleTemps << "unfinished thumbnails";
    if (total <= kMaxCacheBytes)
        return total;

    // Keep the newest photos' thumbnails up to kPruneToBytes: those are the
    // grid's first rows and what the prefetcher makes first. (The files'
    // own dates would say which were made last, not which are wanted.)
    // Everything else goes, also thumbnails of photos no longer listed.
    QSet<QString> keep;
    qint64 kept = 0;
    foreach (const QString &path, newestFirst) {
        const QString name = cacheName(QFileInfo(path));
        const QHash<QString, qint64>::const_iterator it = sizes.constFind(name);
        if (it == sizes.constEnd())
            continue;
        if (kept + it.value() > kPruneToBytes)
            break;
        kept += it.value();
        keep.insert(name);
    }
    int removed = 0;
    qint64 left = kept;
    for (QHash<QString, qint64>::const_iterator it = sizes.constBegin(); it != sizes.constEnd(); ++it) {
        if (keep.contains(it.key()))
            continue;
        if (QFile::remove(dir.filePath(it.key())))
            ++removed;
        else
            left += it.value(); // open in the loader thread
    }
    qDebug() << "Gallery: thumbnail cache had" << sizes.size() << "files," << total / 1024
             << "KB; kept" << keep.size() << "of the newest photos, removed" << removed
             << ", now" << left / 1024 << "KB";
    return left;
}

// ------------------------------------------------------------------ provider

GalleryThumbnailProvider::GalleryThumbnailProvider()
    : QDeclarativeImageProvider(QDeclarativeImageProvider::Image)
{
}

QImage GalleryThumbnailProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    QImage img = GalleryThumbs::load(QUrl::fromPercentEncoding(id.toUtf8()));
    if (!img.isNull() && requestedSize.width() > 0 && requestedSize.height() > 0
            && img.width() > requestedSize.width() && img.height() > requestedSize.height())
        img = img.scaled(requestedSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    if (size)
        *size = img.size();
    return img;
}

// ------------------------------------------------------------------ prefetcher

GalleryPrefetcher::GalleryPrefetcher(QObject *parent)
    : QThread(parent), m_hasPending(false), m_running(false), m_stop(0)
{
}

GalleryPrefetcher::~GalleryPrefetcher()
{
    stop();
}

void GalleryPrefetcher::start(const QStringList &paths)
{
    QMutexLocker lock(&m_mutex);
    m_pending = paths;
    m_hasPending = true;
    m_stop = 1; // a running pass ends, run() then takes this one
    if (m_running)
        return;
    m_running = true;
    lock.unlock();
    // run() may be between its last unlock and returning: start() does
    // nothing on a running thread, so wait for that (microseconds).
    wait();
    QThread::start(QThread::LowestPriority);
}

void GalleryPrefetcher::requestStop()
{
    QMutexLocker lock(&m_mutex);
    m_pending.clear();
    m_hasPending = false;
    m_stop = 1;
}

void GalleryPrefetcher::stop()
{
    requestStop();
    wait();
}

void GalleryPrefetcher::run()
{
    forever {
        QStringList paths;
        {
            QMutexLocker lock(&m_mutex);
            if (!m_hasPending) {
                m_running = false;
                return;
            }
            paths = m_pending;
            m_pending.clear();
            m_hasPending = false;
            m_stop = 0;
        }
        runPass(paths);
    }
}

void GalleryPrefetcher::runPass(const QStringList &paths)
{
    QElapsedTimer timer;
    timer.start();
    GalleryThumbs::prune(paths);
    // Counts the newest photos' thumbnails, cached or made, the same way
    // prune() keeps them: stops at kPruneToBytes, so a pass never makes
    // what the next prune would remove.
    qint64 bytes = 0;
    int made = 0, cached = 0, done = 0;
    bool full = false;
    foreach (const QString &path, paths) {
        if (int(m_stop))
            break;
        if (bytes >= kPruneToBytes) {
            full = true;
            break;
        }
        bool wasCached = false;
        qint64 size = 0;
        const QImage img = loadThumb(path, g_prefetchStats, true, &wasCached, &size);
        if (wasCached)
            ++cached;
        else if (!img.isNull())
            ++made;
        bytes += size;
        ++done;
    }
    qDebug() << "Gallery: prefetch" << (full ? "stopped, cache full," : done < paths.size() ? "stopped" : "done")
             << "after"
             << done << "of" << paths.size() << "photos:" << made << "made," << cached
             << "already cached, their thumbnails" << bytes / 1024 << "KB, in" << timer.elapsed() << "ms";
}
