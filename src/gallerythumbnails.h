// SPDX-FileCopyrightText: 2026 Wunder Wungiel
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GALLERYTHUMBNAILS_H
#define GALLERYTHUMBNAILS_H

#include <QAtomicInt>
#include <QDeclarativeImageProvider>
#include <QImage>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QThread>

// Thumbnails for GalleryPage.qml, made once and kept in a disk cache (the
// app's private folder, "thumbs/"; one small JPEG per photo, keyed by path,
// size and date, so a changed photo gets a new one). Thread-safe.
//
// Making one: JPEGs use the thumbnail the camera put into the EXIF data, so
// a 41 MP photo costs reading that one segment rather than a full decode;
// rotated by the EXIF orientation. Other images, and JPEGs without one: a
// scaled decode (Qt's JPEG reader decodes at 1/2 to 1/8 then), which is
// slow, hence the cache. Timings are logged as a summary per method.
namespace GalleryThumbs {
    // What the cache stores: covers this box (the grid's tiles are ~90 px).
    QSize cacheSize();
    // The thumbnail of a photo ('/' or native path), from the cache or made
    // and cached; null if the file can't be read.
    QImage load(const QString &path);
    // Keeps the cache below its limit, 10 MB (oldest files go first);
    // returns its size in bytes.
    qint64 prune();
}

// image://gallery/<percent-encoded path>: GalleryThumbs::load(), scaled to
// cover the requested size (QML sourceSize). Meant for
// `asynchronous: true` Images, which QML loads in its own (single) thread.
class GalleryThumbnailProvider : public QDeclarativeImageProvider
{
public:
    GalleryThumbnailProvider();
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize);
};

// Fills the cache in the background while the gallery is open, in list
// order (newest first), at the lowest thread priority so the visible tiles
// (QML's loader thread) and the UI go first.
class GalleryPrefetcher : public QThread
{
public:
    explicit GalleryPrefetcher(QObject *parent = 0);
    ~GalleryPrefetcher();
    // Stops a running pass, then starts one over these paths.
    void start(const QStringList &paths);
    // Stops and waits (a thumbnail being made is finished first).
    void stop();

protected:
    void run();

private:
    QStringList m_paths;
    QAtomicInt m_stop;
};

#endif // GALLERYTHUMBNAILS_H
