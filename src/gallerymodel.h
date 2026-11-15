// SPDX-FileCopyrightText: 2026 The Symbigram authors (https://github.com/Symnok/Symbigram)
// SPDX-FileCopyrightText: 2026 Wunder Wungiel
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Based on Symbigram's ImagePicker (src/app/imagepicker.*), with videos and
// check marks added for Symbisend.

#ifndef GALLERYMODEL_H
#define GALLERYMODEL_H

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

class QTimer;
class Thumbnailer;

// The Send page's Media button (GalleryPage.qml): the photos and videos in
// the phone's media folders, newest first, with a check mark each.
//
// The scan is ours and incremental: one directory per event-loop turn, so it
// cannot block the UI however large the media library is. Listing folders is
// cheap; the expensive part is thumbnails, which Thumbnailer makes in a
// worker thread for the cells in view only (each delegate asks with
// requestThumb()) and caches on disk.
// See docs/qml-api.md (`gallery`).
class GalleryModel : public QAbstractListModel
{
    Q_OBJECT
    // A scan is running; the old list stays until it is done.
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    // Grows while the scan runs.
    Q_PROPERTY(int found READ found NOTIFY foundChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(int checkedCount READ checkedCount NOTIFY checkedChanged)
    Q_PROPERTY(QString checkedSizeText READ checkedSizeText NOTIFY checkedChanged)

public:
    enum Roles {
        PathRole = Qt::UserRole + 1,
        NameRole,
        FolderRole,
        IsVideoRole,
        ThumbRole,        // file:/// URL of the cached thumbnail, "" until ready
        ThumbFailedRole,  // could not be made; stop waiting for it
        CheckedRole
    };

    explicit GalleryModel(QObject *parent = 0);
    ~GalleryModel();

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;

    bool loading() const { return m_scanning; }
    int found() const { return m_found.size(); }
    int checkedCount() const { return m_checked.size(); }
    QString checkedSizeText() const;

    // Scans again (asynchronous; see loading). The page calls it each time
    // it is shown, so new photos show up without a cache to invalidate.
    Q_INVOKABLE void load();
    // The page is gone: stops the scan and drops the thumbnails not started.
    Q_INVOKABLE void close();
    // Asks for a row's thumbnail (photos only); a delegate calls it when it
    // is created. Returns at once; the row changes when the file is there.
    Q_INVOKABLE void requestThumb(int index, int edge);
    Q_INVOKABLE void toggle(int index);
    Q_INVOKABLE void clearChecked();
    // '/'-separated paths, in the order they were checked.
    Q_INVOKABLE QStringList checkedFiles() const;

signals:
    void loadingChanged();
    void foundChanged();
    void countChanged();
    void checkedChanged();

private slots:
    void scanSlice();
    void thumbReady(const QString &imagePath, const QString &thumbPath);

private:
    struct Entry {
        QString path;     // '/' separators
        QString name;
        QString folder;
        QString thumb;    // empty until the thumbnailer has one
        bool video;
        bool failed;
        uint modified;
    };
    // A directory still to visit, with its depth so the walk cannot run away.
    struct Dir {
        QString path;
        int depth;
    };
    struct Checked {
        QString path;
        qint64 size;
    };

    void finish();
    static bool newerFirst(const Entry &a, const Entry &b) { return a.modified > b.modified; }
    static QString key(const QString &path) { return path.toLower(); }

    QList<Entry> m_rows;           // published, newest first
    QHash<QString, int> m_rowOf;   // key(path) -> row in m_rows
    QList<Entry> m_found;          // collected during the scan, unsorted
    QList<Dir> m_todo;             // directories still to visit
    QTimer *m_timer;
    Thumbnailer *m_thumbs;
    bool m_scanning;
    int m_dirsVisited;
    qint64 m_scanStarted;          // ms since epoch, for the log
    QList<Checked> m_checked;      // in checking order
    QSet<QString> m_checkedKeys;   // key() of every m_checked path
};

#endif // GALLERYMODEL_H
