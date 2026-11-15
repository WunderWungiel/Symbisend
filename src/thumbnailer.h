// SPDX-FileCopyrightText: 2026 The Symbigram authors (https://github.com/Symnok/Symbigram)
// SPDX-FileCopyrightText: 2026 Wunder Wungiel
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Taken from Symbigram (src/app/thumbnailer.*), trimmed to what Symbisend
// needs: the cache is the app's private folder, no settings or migrations.

#ifndef THUMBNAILER_H
#define THUMBNAILER_H

#include <QByteArray>
#include <QImage>
#include <QList>
#include <QMutex>
#include <QString>
#include <QThread>
#include <QWaitCondition>

// Thumbnails for GalleryPage.qml, made off the UI thread and cached on disk.
//
// 1. Camera JPEGs carry a thumbnail inside their EXIF block. Reading that
//    touches tens of KB instead of several MB. Anything without one
//    (screenshots, downloads, PNGs) falls back to a scaled read, which the
//    JPEG reader does cheaply by DCT scaling.
// 2. The result is written to cacheDir() as a small JPEG, so opening the
//    gallery again costs one small file read per cell.
//
// Requests come only from the cells in view, newest first (LIFO), so a fast
// scroll doesn't leave the user waiting behind cells already gone past.
class Thumbnailer : public QThread
{
    Q_OBJECT

public:
    explicit Thumbnailer(QObject *parent = 0);
    ~Thumbnailer();

    // <private folder>/thumbnails. Settled by the first Thumbnailer, on the
    // UI thread, before its worker starts.
    static QString cacheDir();
    // Deletes the cache of the earlier gallery (<private folder>/thumbs).
    static void discardLegacyCache();

    // The cache file a picture would use, whether or not it exists yet. Keyed
    // on path, size and modification time, so an edited or replaced picture
    // gets a new thumbnail by itself.
    static QString thumbPathFor(const QString &imagePath);

    // Queues a picture. Returns at once; ready() arrives later on the UI
    // thread.
    void request(const QString &imagePath, int edge);
    // Drops anything not yet started (the page was closed or listed again).
    void clearQueue();
    void stop();

    // The embedded EXIF thumbnail, or a null array when there is none.
    // orientation, when given, receives the EXIF Orientation tag (1 if
    // absent): Qt 4.7 has no setAutoTransform, so the caller rotates.
    static QByteArray exifThumbnail(const QString &path, int *orientation = 0);
    static QImage applyOrientation(const QImage &image, int orientation);

signals:
    // thumbPath is the file now on disk, or empty if the picture could not
    // be read (or the cache not written).
    void ready(const QString &imagePath, const QString &thumbPath);

protected:
    void run();

private:
    struct Job {
        QString path;
        int edge;
    };

    QList<Job> m_queue;
    QMutex m_mutex;
    QWaitCondition m_wake;
    bool m_stop;
};

#endif // THUMBNAILER_H
