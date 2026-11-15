// SPDX-FileCopyrightText: 2026 Wunder Wungiel
// SPDX-License-Identifier: GPL-3.0-or-later

#include "thumbnailer.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QTimer>

#ifndef Q_OS_SYMBIAN
#include <QDesktopServices>
#endif

namespace
{
    const int KMaxEdge = 160;           // one grid cell, with a little to spare
    const int KJpegQuality = 82;
    const int KMaxRunning = 3;          // requests outstanding at the service
    const int KTimeoutMs = 20000;       // a request that never answers
    const int KWatchdogMs = 2000;

    QString g_cacheDir;

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

#ifdef Q_OS_SYMBIAN

#include <e32base.h>
#include <e32std.h>
#include <thumbnaildata.h>
#include <thumbnailmanager.h>
#include <thumbnailmanagerobserver.h>
#include <thumbnailobjectsource.h>
#include <QPixmap>

namespace
{
    // Ordinals in thumbnailmanager.dll, from the frozen export table
    // (imagehandlingutilities/thumbnailmanager/thumbnailclient/eabi/
    // thumbnailmanageru.def in the Symbian Foundation sources). Symbian's
    // binary compatibility rules keep frozen ordinals fixed across releases;
    // later ones only append.
    //   @1 _ZN17CThumbnailManager4NewLER25MThumbnailManagerObserver
    //   @4 _ZN22CThumbnailObjectSource4NewLERK7TDesC16S2_
    const TInt KOrdinalManagerNewL = 1;
    const TInt KOrdinalSourceNewL = 4;

    typedef CThumbnailManager *(*ManagerNewL)(MThumbnailManagerObserver &);
    typedef CThumbnailObjectSource *(*SourceNewL)(const TDesC &, const TDesC &);

    // Both classes' destructors are virtual but not exported: deleting
    // through CBase goes through the vtable and links against nothing.
    template <typename T> void destroy(T *p)
    {
        delete static_cast<CBase *>(p);
    }
}

class ThumbnailerPrivate : public MThumbnailManagerObserver
{
public:
    explicit ThumbnailerPrivate(Thumbnailer *q)
        : q(q), m_tried(false), m_loaded(false), m_broken(false),
          m_managerNewL(0), m_sourceNewL(0), m_manager(0)
    {
    }

    ~ThumbnailerPrivate()
    {
        if (m_manager)
            destroy(m_manager); // cancels what is still outstanding
        if (m_loaded)
            m_lib.Close();
    }

    bool load()
    {
        if (m_tried)
            return m_loaded && !m_broken;
        m_tried = true;
        const TInt err = m_lib.Load(_L("thumbnailmanager.dll"));
        if (err != KErrNone) {
            qWarning() << "Thumbs: cannot load thumbnailmanager.dll, error" << err
                       << "- the gallery gets no thumbnails";
            return false;
        }
        m_managerNewL = reinterpret_cast<ManagerNewL>(m_lib.Lookup(KOrdinalManagerNewL));
        m_sourceNewL = reinterpret_cast<SourceNewL>(m_lib.Lookup(KOrdinalSourceNewL));
        if (!m_managerNewL || !m_sourceNewL) {
            qWarning() << "Thumbs: thumbnailmanager.dll lacks the expected exports"
                       << "- the gallery gets no thumbnails";
            m_lib.Close();
            return false;
        }
        m_loaded = true;
        qDebug() << "Thumbs: loaded thumbnailmanager.dll";
        return true;
    }

    // The session with the thumbnail server, opened on the first request.
    bool ensureManager()
    {
        if (m_manager)
            return true;
        if (!load())
            return false;
        CThumbnailManager *manager = 0;
        TRAPD(err, manager = m_managerNewL(*this));
        if (err != KErrNone || !manager) {
            qWarning() << "Thumbs: cannot open the thumbnail service, error" << err;
            m_broken = true;
            return false;
        }
        // Exactly what the Photos app asks for in its grid (glxcachemanager.cpp
        // in the Symbian Foundation sources), so the service hands back the
        // thumbnails it already keeps for Photos instead of making new ones.
        // A failed setting is logged and the service's default used.
        TRAP(err, manager->SetDisplayModeL(EColor64K));
        if (err != KErrNone)
            qWarning() << "Thumbs: cannot set the display mode, error" << err;
        TRAP(err, manager->SetFlagsL(CThumbnailManager::ECropToAspectRatio));
        if (err != KErrNone)
            qWarning() << "Thumbs: cannot set crop to aspect ratio, error" << err;
        TRAP(err, manager->SetQualityPreferenceL(CThumbnailManager::EOptimizeForPerformance));
        if (err != KErrNone)
            qWarning() << "Thumbs: cannot set the quality preference, error" << err;
        TRAP(err, manager->SetThumbnailSizeL(EGridThumbnailSize));
        if (err != KErrNone)
            qWarning() << "Thumbs: cannot set the grid size, error" << err;
        m_manager = manager;
        qDebug() << "Thumbs: thumbnail service ready";
        return true;
    }

    // A request id (> 0), or a Symbian error (< 0).
    int start(const QString &path)
    {
        if (!ensureManager())
            return KErrNotSupported;
        const QString native = QDir::toNativeSeparators(path);
        const TPtrC uri(reinterpret_cast<const TUint16 *>(native.utf16()), native.length());
        CThumbnailObjectSource *source = 0;
        TRAPD(err, source = m_sourceNewL(uri, KNullDesC));
        if (err != KErrNone || !source)
            return err != KErrNone ? err : KErrNoMemory;
        TThumbnailRequestId id = 0;
        TRAP(err, id = m_manager->GetThumbnailL(*source));
        destroy(source); // the request has copied what it needs
        if (err != KErrNone)
            return err;
        return id > 0 ? int(id) : KErrGeneral;
    }

    void cancel(int id)
    {
        if (m_manager)
            m_manager->CancelRequest(id);
    }

    void ThumbnailPreviewReady(MThumbnailData &, TThumbnailRequestId)
    {
    }

    // Runs in the service's active object: must not leave or throw.
    void ThumbnailReady(TInt aError, MThumbnailData &aThumbnail, TThumbnailRequestId aId)
    {
        try {
            QImage image;
            if (aError == KErrNone) {
                CFbsBitmap *bitmap = aThumbnail.Bitmap();
                // Copied out now: the bitmap is the service's and goes when
                // this returns.
                if (bitmap)
                    image = QPixmap::fromSymbianCFbsBitmap(bitmap).toImage();
            }
            q->finished(int(aId), image, int(aError));
        } catch (...) {
            qWarning() << "Thumbs: exception in the thumbnail callback";
        }
    }

private:
    Thumbnailer *q;
    bool m_tried;
    bool m_loaded;
    bool m_broken;
    RLibrary m_lib;
    ManagerNewL m_managerNewL;
    SourceNewL m_sourceNewL;
    CThumbnailManager *m_manager;
};

#else // !Q_OS_SYMBIAN

class ThumbnailerPrivate
{
public:
    explicit ThumbnailerPrivate(Thumbnailer *) {}
    bool load() { return false; }
    int start(const QString &) { return -5; } // KErrNotSupported
    void cancel(int) {}
};

#endif

Thumbnailer::Thumbnailer(QObject *parent)
    : QObject(parent)
    , d(new ThumbnailerPrivate(this))
    , m_watchdog(new QTimer(this))
    , m_pumpScheduled(false)
    , m_made(0)
    , m_failed(0)
    , m_totalMs(0)
{
    cacheDir();
    m_watchdog->setInterval(KWatchdogMs);
    connect(m_watchdog, SIGNAL(timeout()), this, SLOT(checkTimeouts()));
}

Thumbnailer::~Thumbnailer()
{
    delete d;
}

QString Thumbnailer::cacheDir()
{
    if (g_cacheDir.isEmpty()) {
        g_cacheDir = privateFolder() + QLatin1String("/thumbnails");
        if (!QDir().mkpath(g_cacheDir))
            qWarning() << "Thumbs: cannot create the thumbnail cache" << g_cacheDir;
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
    qDebug() << "Thumbs: removed the old thumbnail cache," << files.size() << "files";
}

QString Thumbnailer::thumbPathFor(const QString &path)
{
    const QFileInfo fi(path);
    QByteArray key = path.toUtf8();
    key += '\0';
    key += QByteArray::number(qint64(fi.size()));
    key += '\0';
    key += QByteArray::number(qint64(fi.lastModified().toTime_t()));
    const QByteArray hash = QCryptographicHash::hash(key, QCryptographicHash::Md5).toHex();
    return cacheDir() + QLatin1Char('/') + QString::fromLatin1(hash) + QLatin1String(".jpg");
}

bool Thumbnailer::available()
{
    return d->load();
}

void Thumbnailer::request(const QString &path, int edge)
{
    for (int i = 0; i < m_queue.size(); ++i)
        if (m_queue.at(i).path == path) return;
    for (QHash<int, Running>::const_iterator it = m_running.constBegin(); it != m_running.constEnd(); ++it)
        if (it.value().path == path) return;
    Job job;
    job.path = path;
    job.edge = edge > 0 ? qMin(edge, KMaxEdge) : KMaxEdge;
    m_queue.prepend(job); // the cells revealed last are the ones looked at
    schedulePump();
}

void Thumbnailer::clearQueue()
{
    m_queue.clear();
}

void Thumbnailer::schedulePump()
{
    if (m_pumpScheduled)
        return;
    m_pumpScheduled = true;
    // Queued: request() is called while QML builds a delegate, and the
    // service must not be called back into from its own callback.
    QMetaObject::invokeMethod(this, "pump", Qt::QueuedConnection);
}

void Thumbnailer::pump()
{
    m_pumpScheduled = false;
    while (m_running.size() < KMaxRunning && !m_queue.isEmpty()) {
        const Job job = m_queue.takeFirst();
        const QString out = thumbPathFor(job.path);
        if (QFile::exists(out)) {
            emit ready(job.path, out);
            continue;
        }
        const int id = d->start(job.path);
        if (id <= 0) {
            ++m_failed;
            qWarning() << "Thumbs: cannot request" << job.path << "error" << id;
            emit ready(job.path, QString());
            continue;
        }
        Running r;
        r.path = job.path;
        r.edge = job.edge;
        r.started.start();
        m_running.insert(id, r);
    }
    if (!m_running.isEmpty() && !m_watchdog->isActive())
        m_watchdog->start();
}

void Thumbnailer::finished(int requestId, const QImage &image, int error)
{
    QHash<int, Running>::iterator it = m_running.find(requestId);
    if (it == m_running.end())
        return; // timed out meanwhile
    const Running r = it.value();
    m_running.erase(it);
    const int ms = r.started.elapsed();

    QString saved;
    if (image.isNull()) {
        ++m_failed;
        qWarning() << "Thumbs: none for" << r.path << "error" << error << "after" << ms << "ms";
    } else {
        if (m_made == 0)
            qDebug() << "Thumbs: first thumbnail" << image.width() << "x" << image.height()
                     << "in" << ms << "ms";
        QImage scaled = image;
        if (scaled.width() > r.edge || scaled.height() > r.edge)
            scaled = scaled.scaled(r.edge, r.edge, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        const QString out = thumbPathFor(r.path);
        if (scaled.save(out, "JPEG", KJpegQuality)) {
            saved = out;
            ++m_made;
            m_totalMs += ms;
            if (m_made % 25 == 0)
                qDebug() << "Thumbs: made" << m_made << "(avg" << (m_totalMs / m_made)
                         << "ms), failed" << m_failed;
        } else {
            ++m_failed;
            qWarning() << "Thumbs: cannot write" << out;
        }
    }
    emit ready(r.path, saved);

    if (m_running.isEmpty())
        m_watchdog->stop();
    schedulePump();
}

void Thumbnailer::checkTimeouts()
{
    QList<int> late;
    for (QHash<int, Running>::const_iterator it = m_running.constBegin(); it != m_running.constEnd(); ++it)
        if (it.value().started.elapsed() > KTimeoutMs) late << it.key();
    foreach (int id, late) {
        const QString path = m_running.take(id).path;
        d->cancel(id);
        ++m_failed;
        qWarning() << "Thumbs: no answer for" << path << "in" << KTimeoutMs / 1000 << "s, cancelled";
        emit ready(path, QString());
    }
    if (m_running.isEmpty())
        m_watchdog->stop();
    if (!late.isEmpty())
        schedulePump();
}
