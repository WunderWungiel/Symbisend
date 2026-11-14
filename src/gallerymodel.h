// SPDX-FileCopyrightText: 2026 Wunder Wungiel
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GALLERYMODEL_H
#define GALLERYMODEL_H

#include <QAbstractListModel>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

class GalleryMdsQuery;
class GalleryPrefetcher;

// The Send page's Media button (GalleryPage.qml): the phone's photos and
// videos, newest first, with a check mark each. Replaces the system MGFetch
// picker, which also listed app caches and thumbnail folders and can't be
// filtered.
//
// Symbian: asks the metadata store (MdS, the index the Photos app uses) for
// Image and Video objects. Elsewhere: scans the Pictures and Movies folders.
// Either way, files in hidden, cache, thumbnail and system folders are left
// out (isJunk()); what is left out and what is listed per folder is logged.
// Thumbnails come from image://gallery (GalleryThumbnailProvider); after
// each listing, GalleryPrefetcher fills their cache in the background until
// close().
// See docs/qml-api.md (`gallery`).
class GalleryModel : public QAbstractListModel
{
    Q_OBJECT
    // A listing is running; the old list stays until it is done.
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(int checkedCount READ checkedCount NOTIFY checkedChanged)
    Q_PROPERTY(QString checkedSizeText READ checkedSizeText NOTIFY checkedChanged)

public:
    enum Roles {
        PathRole = Qt::UserRole + 1,
        NameRole,
        IsVideoRole,
        ThumbnailRole,
        CheckedRole
    };

    // One photo or video (public for the sort function).
    struct Item {
        QString path;   // '/' separators
        QString name;
        bool video;
        qint64 time;    // for sorting only (newest first)
    };

    explicit GalleryModel(QObject *parent = 0);
    ~GalleryModel();

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;

    bool loading() const { return m_loading; }
    int checkedCount() const { return m_checked.size(); }
    QString checkedSizeText() const;

    // Lists the photos and videos again (asynchronous; see loading). The
    // page calls it each time it is shown.
    Q_INVOKABLE void load();
    Q_INVOKABLE void toggle(int index);
    Q_INVOKABLE void clearChecked();
    // '/'-separated paths, in the order they were checked.
    Q_INVOKABLE QStringList checkedFiles() const;
    // The page is gone: stops making thumbnails in the background.
    Q_INVOKABLE void close();

    // Whether a path (native or '/') lies in a folder that holds no user
    // media: hidden (".x", "_x" such as _PAlbTN), caches, thumbnails,
    // temporary and system folders. why gets the folder name that matched.
    static bool isJunk(const QString &path, QString *why = 0);

signals:
    void loadingChanged();
    void countChanged();
    void checkedChanged();

private slots:
    void scanFolders(); // not Symbian
    void mdsStep();     // Symbian: the next MdS step (queued from GalleryMdsQuery)

private:
    friend class GalleryMdsQuery;

    struct Checked {
        QString path;
        qint64 size;
    };

    // Called with what the listing found (any order); filters, sorts and
    // replaces the list.
    void finishLoad(const QList<Item> &found, const char *source);
    static QString key(const QString &path) { return path.toLower(); }

    QList<Item> m_items;
    QList<Checked> m_checked;     // in checking order
    QSet<QString> m_checkedKeys;  // key() of every m_checked path
    bool m_loading;
    qint64 m_loadStarted;         // ms since epoch, for the log
    GalleryMdsQuery *m_mds;       // Symbian only
    GalleryPrefetcher *m_prefetcher;
    bool m_open;                  // between load() and close()
};

#endif // GALLERYMODEL_H
