// SPDX-FileCopyrightText: 2026 Wunder Wungiel
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FOLDERMODEL_H
#define FOLDERMODEL_H

#include <QAbstractListModel>
#include <QDateTime>
#include <QList>
#include <QLocale>
#include <QSet>
#include <QString>
#include <QStringList>

// The Send page's file browser (FileBrowserPage.qml): one folder's contents,
// or the list of drives, with a check mark per file that is kept while the
// user moves between folders (folders aren't checked: the Folder button
// adds a whole folder). Symbian has no native multi-select file
// picker (the Avkon dialog picks one file, MGFetch only media), hence this.
// See docs/qml-api.md (`fileBrowser`).
//
// Folders are listed on the UI thread in one go (QDir::entryInfoList, which
// brings size and date along); the time it takes is logged.
class FolderModel : public QAbstractListModel
{
    Q_OBJECT
    // '/'-separated path of the folder shown; "" = the drive list.
    Q_PROPERTY(QString folder READ folder NOTIFY folderChanged)
    // The folder for display (native separators); "" for the drive list.
    Q_PROPERTY(QString folderText READ folderText NOTIFY folderChanged)
    // Nothing above: the drive list (Symbian, Windows) or "/".
    Q_PROPERTY(bool atRoot READ atRoot NOTIFY folderChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY folderChanged)
    // Checked files, in all folders.
    Q_PROPERTY(int checkedCount READ checkedCount NOTIFY checkedChanged)
    // Total size of the checked files.
    Q_PROPERTY(QString checkedSizeText READ checkedSizeText NOTIFY checkedChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        PathRole,
        IsDirRole,
        IsDriveRole,
        SizeRole,
        SizeTextRole,
        ModifiedTextRole,
        CheckedRole
    };

    explicit FolderModel(QObject *parent = 0);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;

    QString folder() const { return m_folder; }
    QString folderText() const;
    bool atRoot() const;
    int checkedCount() const { return m_checked.size(); }
    QString checkedSizeText() const;

    // Go into the folder (or drive) at index. Emits openFailed() if it can't
    // be read; the current folder stays.
    Q_INVOKABLE void open(int index);
    // One level up; returns the index of the folder we came from in the new
    // listing (to scroll to it), or -1.
    Q_INVOKABLE int up();
    // Lists the start folder (the drive list, or home without drives); the
    // page calls it each time it is shown.
    Q_INVOKABLE void openStart();
    // Checks or unchecks the file at index (folders and drives: nothing).
    Q_INVOKABLE void toggle(int index);
    Q_INVOKABLE void clearChecked();
    // '/'-separated paths, in the order they were checked.
    Q_INVOKABLE QStringList checkedFiles() const;

signals:
    void folderChanged();
    void checkedChanged();
    // path is native ("E:\Private").
    void openFailed(const QString &path);

private:
    struct Entry {
        QString name;
        QString path;       // '/' separators, no trailing '/' except drive roots
        bool isDir;
        bool isDrive;
        qint64 size;
        QDateTime modified;
    };
    struct Checked {
        QString path;
        qint64 size;
    };

    // Lists folder ("" = drives) into m_entries; false if it can't be read.
    bool load(const QString &folder);
    static QList<Entry> listDrives();
    static QString startFolder();
    // Drive letters and names are case-insensitive (as in SelectionModel).
    static QString key(const QString &path) { return path.toLower(); }

    QString m_folder;
    QList<Entry> m_entries;
    QList<Checked> m_checked;          // in checking order
    QSet<QString> m_checkedKeys;       // key() of every m_checked path
    QLocale m_locale;
};

#endif // FOLDERMODEL_H
