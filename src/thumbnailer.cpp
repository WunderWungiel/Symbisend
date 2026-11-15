// SPDX-FileCopyrightText: 2026 The Symbigram authors (https://github.com/Symnok/Symbigram)
// SPDX-FileCopyrightText: 2026 Wunder Wungiel
// SPDX-License-Identifier: GPL-3.0-or-later

#include "thumbnailer.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QTime>
#include <QTransform>

#ifndef Q_OS_SYMBIAN
#include <QDesktopServices>
#endif

namespace
{
    // Big enough for one grid cell, with a little to spare.
    const int KMaxEdge = 160;
    const int KJpegQuality = 82;

    // A JPEG segment carries a 16-bit length, so an APP1/EXIF block is at
    // most this big. APP1 can sit behind other segments (an ICC profile in
    // APP2 is itself up to 64 KB), so the markers are walked with seeks and
    // only the EXIF segment itself is read.
    const int KMaxSegment = 65533;
    const int KMaxMarkers = 64;

    // Written once, by the first Thumbnailer's constructor on the UI thread,
    // before its worker starts; only read after that. File scope, as
    // function-local statics aren't initialised thread-safely before C++11.
    QString g_cacheDir;

    // Readers over a TIFF block, honouring its endianness.
    quint16 tiffU16(const QByteArray &d, int base, bool big, int off)
    {
        if (base + off + 2 > d.size() || base + off < 0) return 0;
        const quint8 a = quint8(d.at(base + off));
        const quint8 b = quint8(d.at(base + off + 1));
        return big ? quint16((a << 8) | b) : quint16((b << 8) | a);
    }

    quint32 tiffU32(const QByteArray &d, int base, bool big, int off)
    {
        if (base + off + 4 > d.size() || base + off < 0) return 0;
        const quint8 a = quint8(d.at(base + off));
        const quint8 b = quint8(d.at(base + off + 1));
        const quint8 c = quint8(d.at(base + off + 2));
        const quint8 e = quint8(d.at(base + off + 3));
        return big ? ((quint32(a) << 24) | (quint32(b) << 16) | (quint32(c) << 8) | quint32(e))
                   : ((quint32(e) << 24) | (quint32(c) << 16) | (quint32(b) << 8) | quint32(a));
    }

    QString privateFolder()
    {
#ifdef Q_OS_SYMBIAN
        // applicationDirPath() is the private folder: no capabilities needed,
        // and the gallery's folder scan never goes there.
        return QDir::fromNativeSeparators(QCoreApplication::applicationDirPath());
#else
        QString base = QDesktopServices::storageLocation(QDesktopServices::DataLocation);
        if (base.isEmpty())
            base = QDir::homePath() + QLatin1String("/.symbisend");
        return QDir::fromNativeSeparators(base);
#endif
    }
}

Thumbnailer::Thumbnailer(QObject *parent)
    : QThread(parent)
    , m_stop(false)
{
    cacheDir(); // settled here, on the UI thread, before run() can read it
}

Thumbnailer::~Thumbnailer()
{
    stop();
    wait();
}

QString Thumbnailer::cacheDir()
{
    if (g_cacheDir.isEmpty()) {
        g_cacheDir = privateFolder() + QLatin1String("/thumbnails");
        if (!QDir().mkpath(g_cacheDir))
            qWarning() << "Gallery: cannot create the thumbnail cache" << g_cacheDir;
    }
    return g_cacheDir;
}

void Thumbnailer::discardLegacyCache()
{
    QDir dir(privateFolder() + QLatin1String("/thumbs"));
    if (!dir.exists())
        return;
    const QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::Hidden | QDir::System);
    for (int i = 0; i < files.size(); ++i)
        QFile::remove(files.at(i).absoluteFilePath());
    dir.cdUp();
    dir.rmdir(QLatin1String("thumbs"));
    qDebug() << "Gallery: removed the old thumbnail cache," << files.size() << "files";
}

QString Thumbnailer::thumbPathFor(const QString &imagePath)
{
    const QFileInfo fi(imagePath);
    QByteArray key = imagePath.toUtf8();
    key += '\0';
    key += QByteArray::number(qint64(fi.size()));
    key += '\0';
    key += QByteArray::number(qint64(fi.lastModified().toTime_t()));
    const QByteArray hash = QCryptographicHash::hash(key, QCryptographicHash::Md5).toHex();
    return cacheDir() + QLatin1Char('/') + QString::fromLatin1(hash) + QLatin1String(".jpg");
}

void Thumbnailer::request(const QString &imagePath, int edge)
{
    QMutexLocker lock(&m_mutex);
    for (int i = 0; i < m_queue.size(); ++i)
        if (m_queue.at(i).path == imagePath) return; // already waiting
    Job job;
    job.path = imagePath;
    job.edge = edge > 0 ? qMin(edge, KMaxEdge) : KMaxEdge;
    // At the front: the cells revealed last are the ones being looked at.
    m_queue.prepend(job);
    m_wake.wakeAll();
}

void Thumbnailer::clearQueue()
{
    QMutexLocker lock(&m_mutex);
    m_queue.clear();
}

void Thumbnailer::stop()
{
    QMutexLocker lock(&m_mutex);
    m_stop = true;
    m_queue.clear();
    m_wake.wakeAll();
}

QByteArray Thumbnailer::exifThumbnail(const QString &path, int *orientation)
{
    if (orientation) *orientation = 1;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return QByteArray();

    char soi[2];
    if (f.read(soi, 2) != 2) return QByteArray();
    if (quint8(soi[0]) != 0xFF || quint8(soi[1]) != 0xD8) return QByteArray(); // not a JPEG

    const QByteArray exifMagic("Exif\0\0", 6);

    // Walk the markers, reading only each 4-byte header and skipping the
    // payloads, until APP1 with an EXIF payload turns up.
    for (int seen = 0; seen < KMaxMarkers; ++seen) {
        char hdr[4];
        if (f.read(hdr, 2) != 2) break;
        if (quint8(hdr[0]) != 0xFF) break;
        const quint8 marker = quint8(hdr[1]);
        if (marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) continue; // no payload
        if (marker == 0xDA || marker == 0xD9) break;                         // pixels start
        if (f.read(hdr + 2, 2) != 2) break;
        const int len = (int(quint8(hdr[2])) << 8) | int(quint8(hdr[3]));
        if (len < 2 || len > KMaxSegment + 2) break;
        const int payloadLen = len - 2;

        if (marker != 0xE1) {
            if (!f.seek(f.pos() + payloadLen)) break;
            continue;
        }

        const QByteArray seg = f.read(payloadLen);
        if (seg.size() != payloadLen) break;
        if (seg.size() < 6 || seg.left(6) != exifMagic) continue; // APP1, but not EXIF (XMP)

        // EXIF offsets are relative to the start of the TIFF header.
        const QByteArray tiff = seg.mid(6);
        if (tiff.size() < 8) return QByteArray();

        const bool big = tiff.left(2) == QByteArray("MM", 2);
        if (!big && tiff.left(2) != QByteArray("II", 2)) return QByteArray();

        const quint32 ifd0 = tiffU32(tiff, 0, big, 4);
        if (ifd0 < 8 || ifd0 > quint32(tiff.size())) return QByteArray();
        const quint16 n0 = tiffU16(tiff, 0, big, int(ifd0));
        if (n0 == 0 || n0 > 512) return QByteArray();

        // Orientation lives in IFD0.
        if (orientation) {
            for (int i = 0; i < int(n0); ++i) {
                const int e = int(ifd0) + 2 + 12 * i;
                if (tiffU16(tiff, 0, big, e) != 0x0112) continue;
                const int v = int(tiffU16(tiff, 0, big, e + 8));
                if (v >= 1 && v <= 8) *orientation = v;
                break;
            }
        }

        // IFD1, the thumbnail directory, is linked right after IFD0's entries.
        const quint32 ifd1 = tiffU32(tiff, 0, big, int(ifd0) + 2 + 12 * int(n0));
        if (ifd1 < 8 || ifd1 > quint32(tiff.size())) return QByteArray();
        const quint16 n1 = tiffU16(tiff, 0, big, int(ifd1));
        if (n1 == 0 || n1 > 512) return QByteArray();

        quint32 thumbOff = 0;
        quint32 thumbLen = 0;
        for (int i = 0; i < int(n1); ++i) {
            const int e = int(ifd1) + 2 + 12 * i;
            const quint16 tag = tiffU16(tiff, 0, big, e);
            if (tag == 0x0201) thumbOff = tiffU32(tiff, 0, big, e + 8);      // offset
            else if (tag == 0x0202) thumbLen = tiffU32(tiff, 0, big, e + 8); // byte count
        }
        if (thumbOff == 0 || thumbLen == 0) return QByteArray();
        if (qint64(thumbOff) + qint64(thumbLen) > qint64(tiff.size())) return QByteArray();
        return tiff.mid(int(thumbOff), int(thumbLen));
    }
    return QByteArray();
}

QImage Thumbnailer::applyOrientation(const QImage &image, int orientation)
{
    if (orientation <= 1 || orientation > 8 || image.isNull()) return image;

    QTransform t;
    switch (orientation) {
    case 2: return image.mirrored(true, false);
    case 3: t.rotate(180); break;
    case 4: return image.mirrored(false, true);
    case 5: t.rotate(90); return image.transformed(t).mirrored(true, false);
    case 6: t.rotate(90); break;
    case 7: t.rotate(270); return image.transformed(t).mirrored(true, false);
    case 8: t.rotate(270); break;
    default: return image;
    }
    return image.transformed(t);
}

void Thumbnailer::run()
{
    // For the log: how many were made each way, and how long they took.
    int exifCount = 0, decodeCount = 0, failed = 0;
    qint64 exifMs = 0, decodeMs = 0;

    for (;;) {
        Job job;
        {
            QMutexLocker lock(&m_mutex);
            while (!m_stop && m_queue.isEmpty()) m_wake.wait(&m_mutex);
            if (m_stop) return;
            job = m_queue.takeFirst();
        }

        const QString out = thumbPathFor(job.path);
        if (QFile::exists(out)) {
            emit ready(job.path, out);
            continue;
        }

        QTime timer;
        timer.start();
        QImage image;
        int orientation = 1;
        bool fromExif = false;

        // The stored (un-rotated) size of the picture, from the header alone.
        const QSize storedSize = QImageReader(job.path).size();

        // Fast path: the picture's own EXIF thumbnail.
        const QByteArray embedded = exifThumbnail(job.path, &orientation);
        if (!embedded.isEmpty()) {
            // Scaled even here: cameras embed up to 512x384.
            QBuffer buffer;
            buffer.setData(embedded);
            buffer.open(QIODevice::ReadOnly);
            QImageReader reader(&buffer, "JPEG");
            const QSize full = reader.size();
            if (full.isValid() && (full.width() > job.edge || full.height() > job.edge)) {
                QSize target = full;
                target.scale(job.edge, job.edge, Qt::KeepAspectRatio);
                reader.setScaledSize(target);
            }
            image = reader.read();

            // An embedded thumbnail can be stale: editors crop or rotate the
            // picture and leave the old one. Aspect is the cheap tell.
            if (!image.isNull() && storedSize.isValid() && image.height() > 0 && storedSize.height() > 0) {
                const qreal a = qreal(image.width()) / qreal(image.height());
                const qreal b = qreal(storedSize.width()) / qreal(storedSize.height());
                if (qAbs(a - b) > 0.05 * b) image = QImage();
            }
            fromExif = !image.isNull();
        }

        // Otherwise a scaled read (the JPEG reader decodes at 1/2 to 1/8).
        if (image.isNull()) {
            QImageReader reader(job.path);
            const QSize full = reader.size();
            if (full.isValid() && (full.width() > job.edge || full.height() > job.edge)) {
                QSize target = full;
                target.scale(job.edge, job.edge, Qt::KeepAspectRatio);
                reader.setScaledSize(target);
            }
            image = reader.read();
        }

        if (image.isNull()) {
            ++failed;
            qWarning() << "Gallery: cannot read" << job.path;
            emit ready(job.path, QString()); // the grid shows a placeholder
            continue;
        }

        // Both paths give stored pixels, so the rotation is applied here.
        image = applyOrientation(image, orientation);

        if (image.width() > job.edge || image.height() > job.edge)
            image = image.scaled(job.edge, job.edge, Qt::KeepAspectRatio, Qt::SmoothTransformation);

        const bool saved = image.save(out, "JPEG", KJpegQuality);
        if (!saved) {
            ++failed;
            qWarning() << "Gallery: cannot write thumbnail" << out;
        }
        const int ms = timer.elapsed();
        if (fromExif) { ++exifCount; exifMs += ms; }
        else { ++decodeCount; decodeMs += ms; }
        const int made = exifCount + decodeCount;
        if (made == 1 || made % 25 == 0)
            qDebug() << "Gallery: thumbnails made:" << made
                     << "- exif" << exifCount << "(avg" << (exifCount ? exifMs / exifCount : 0) << "ms),"
                     << "decode" << decodeCount << "(avg" << (decodeCount ? decodeMs / decodeCount : 0) << "ms),"
                     << "failed" << failed;
        emit ready(job.path, saved ? out : QString());
    }
}
