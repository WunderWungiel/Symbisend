// SPDX-FileCopyrightText: 2026 Wunder Wungiel
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef THUMBNAILER_H
#define THUMBNAILER_H

#include <QHash>
#include <QImage>
#include <QList>
#include <QObject>
#include <QString>
#include <QTime>

class QTimer;
class ThumbnailerPrivate;

// Thumbnails of photos and videos for GalleryPage.qml, cached on disk as small
// JPEGs so QML can show them as file:// URLs and a second visit costs one
// small file read per cell.
//
// They come from the system thumbnail service (ThumbnailManager, what Photos
// and Videos use): it decodes with the phone's own codecs (hardware JPEG and
// video), honours EXIF orientation, and often has a grid thumbnail already
// from the camera or the media harvester. It is asked with the Photos app's
// own grid settings, so those come straight from its store. The SDK ships
// its headers but no import library, so thumbnailmanager.dll is loaded at
// run time and its two
// factory functions are looked up by ordinal (frozen, see the .cpp); all the
// rest is virtual. If that fails, available() is false and the grid shows
// placeholders. Elsewhere than Symbian it is always false for now (Harmattan
// could use Tumbler behind the same interface).
//
// The service is asynchronous and runs on this (the UI) thread's active
// scheduler, so there is no worker thread. A few requests are outstanding at
// once; the rest wait newest first, so a fast scroll doesn't leave the user
// waiting behind cells already gone past.
class Thumbnailer : public QObject
{
    Q_OBJECT

public:
    explicit Thumbnailer(QObject *parent = 0);
    ~Thumbnailer();

    // <private folder>/thumbnails.
    static QString cacheDir();
    // Deletes the cache of the earlier gallery (<private folder>/thumbs).
    static void discardLegacyCache();
    // The cache file a picture or video would use, whether or not it exists
    // yet. Keyed on path, size and modification time, so an edited or
    // replaced file gets a new thumbnail by itself.
    static QString thumbPathFor(const QString &path);

    // Whether the system service could be loaded (tried once, on first call).
    bool available();

    // Queues a file. Returns at once; ready() comes later, never from inside
    // this call.
    void request(const QString &path, int edge);
    // Drops what has not been sent to the service yet.
    void clearQueue();

signals:
    // thumbPath is the cache file now on disk, or empty if no thumbnail could
    // be made (unreadable file, unsupported codec, timeout, cache not
    // writable).
    void ready(const QString &path, const QString &thumbPath);

private slots:
    void pump();
    void checkTimeouts();

private:
    friend class ThumbnailerPrivate;
    // From the service callback: image is null on failure.
    void finished(int requestId, const QImage &image, int error);
    void schedulePump();

    struct Job {
        QString path;
        int edge;
    };
    struct Running {
        QString path;
        int edge;
        QTime started;
    };

    ThumbnailerPrivate *d;
    QList<Job> m_queue;
    QHash<int, Running> m_running;   // request id -> job
    QTimer *m_watchdog;
    bool m_pumpScheduled;
    // For the log.
    int m_made;
    int m_failed;
    qint64 m_totalMs;
};

#endif // THUMBNAILER_H
