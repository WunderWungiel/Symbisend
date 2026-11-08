// SPDX-FileCopyrightText: 2026 Wunder Wungiel
// SPDX-License-Identifier: GPL-3.0-or-later

#include "foldermodel.h"

#include "selectionmodel.h"
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QHash>

#if defined(Q_OS_SYMBIAN) || defined(Q_OS_WIN)
#define SYMBISEND_HAS_DRIVES
#endif

FolderModel::FolderModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_locale(QLocale::system())
{
    QHash<int, QByteArray> roles;
    roles[NameRole] = "name";
    roles[PathRole] = "path";
    roles[IsDirRole] = "isDir";
    roles[IsDriveRole] = "isDrive";
    roles[SizeRole] = "size";
    roles[SizeTextRole] = "sizeText";
    roles[ModifiedTextRole] = "modifiedText";
    roles[CheckedRole] = "checked";
    setRoleNames(roles);

    // Listed when the page first asks (refresh()), not at startup.
    m_folder = startFolder();
}

QString FolderModel::startFolder()
{
#ifdef SYMBISEND_HAS_DRIVES
    return QString();
#else
    return QDir::homePath();
#endif
}

int FolderModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

QVariant FolderModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return QVariant();
    const Entry &e = m_entries.at(index.row());
    switch (role) {
    case NameRole: return e.name;
    case PathRole: return e.path;
    case IsDirRole: return e.isDir;
    case IsDriveRole: return e.isDrive;
    case SizeRole: return qreal(e.size);
    case SizeTextRole: return e.isDir ? QString() : SelectionModel::formatBytes(qreal(e.size));
    case ModifiedTextRole:
        return e.modified.isValid() ? m_locale.toString(e.modified, QLocale::ShortFormat) : QString();
    case CheckedRole: return m_checkedKeys.contains(key(e.path));
    }
    return QVariant();
}

QString FolderModel::folderText() const
{
    return QDir::toNativeSeparators(m_folder);
}

bool FolderModel::atRoot() const
{
#ifdef SYMBISEND_HAS_DRIVES
    return m_folder.isEmpty();
#else
    return QDir(m_folder).isRoot();
#endif
}

QString FolderModel::checkedSizeText() const
{
    qint64 total = 0;
    foreach (const Checked &c, m_checked)
        total += c.size;
    return SelectionModel::formatBytes(qreal(total));
}

QList<FolderModel::Entry> FolderModel::listDrives()
{
    QList<Entry> drives;
    foreach (const QFileInfo &fi, QDir::drives()) {
        QString path = fi.absoluteFilePath(); // "E:/"
        QString letter = path.left(1).toUpper();
#ifdef Q_OS_SYMBIAN
        // D: is the RAM disk (temporary files), Z: the ROM.
        if (letter == QLatin1String("D") || letter == QLatin1String("Z"))
            continue;
#endif
        // A memory card slot without a card is listed but can't be read.
        if (!QDir(path).exists()) {
            qDebug() << "FolderModel: drive" << path << "not readable, hidden";
            continue;
        }
        Entry e;
        e.name = letter + QLatin1Char(':');
        e.path = path;
        e.isDir = true;
        e.isDrive = true;
        e.size = 0;
        drives << e;
    }
    return drives;
}

bool FolderModel::load(const QString &folder)
{
    QList<Entry> entries;
    QElapsedTimer timer;
    timer.start();

    if (folder.isEmpty()) {
        entries = listDrives();
    } else {
        QDir dir(folder);
        if (!dir.exists() || !QFileInfo(folder).isReadable()) {
            qWarning() << "FolderModel: can't read" << folder;
            return false;
        }
        // Without QDir::Hidden, hidden files and folders are left out, as in
        // SelectionModel::addFolder().
        QFileInfoList infos = dir.entryInfoList(
                    QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Readable,
                    QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
        foreach (const QFileInfo &fi, infos) {
            Entry e;
            e.name = fi.fileName();
            e.path = fi.absoluteFilePath();
            e.isDir = fi.isDir();
            e.isDrive = false;
            e.size = e.isDir ? 0 : fi.size();
            e.modified = fi.lastModified();
            entries << e;
        }
    }

    beginResetModel();
    m_entries = entries;
    m_folder = folder;
    endResetModel();
    qDebug() << "FolderModel: listed" << (folder.isEmpty() ? QString::fromLatin1("drives") : folder)
             << "-" << entries.size() << "entries in" << timer.elapsed() << "ms";
    emit folderChanged();
    return true;
}

void FolderModel::open(int index)
{
    if (index < 0 || index >= m_entries.size() || !m_entries.at(index).isDir)
        return;
    QString path = m_entries.at(index).path;
    if (!load(path))
        emit openFailed(QDir::toNativeSeparators(path));
}

int FolderModel::up()
{
    if (atRoot())
        return -1;
    QString from = m_folder;
    QDir dir(m_folder);
    QString parent;
    if (dir.isRoot()) {
#ifdef SYMBISEND_HAS_DRIVES
        parent = QString(); // the drive list
#else
        return -1;
#endif
    } else {
        dir.cdUp();
        parent = dir.absolutePath();
    }
    if (!load(parent)) {
        // The parent of a folder we could read is gone: start over.
        load(startFolder());
    }
    for (int i = 0; i < m_entries.size(); ++i) {
        if (key(m_entries.at(i).path) == key(from))
            return i;
    }
    return -1;
}

void FolderModel::refresh()
{
    if (!load(m_folder) && !load(startFolder()))
        qWarning() << "FolderModel: can't list" << startFolder();
}

void FolderModel::toggle(int index)
{
    if (index < 0 || index >= m_entries.size())
        return;
    const Entry &e = m_entries.at(index);
    if (e.isDrive)
        return;
    QString k = key(e.path);
    if (m_checkedKeys.contains(k)) {
        m_checkedKeys.remove(k);
        for (int i = 0; i < m_checked.size(); ++i) {
            if (key(m_checked.at(i).path) == k) {
                m_checked.removeAt(i);
                break;
            }
        }
    } else {
        Checked c;
        c.path = e.path;
        c.isDir = e.isDir;
        c.size = e.size;
        m_checked << c;
        m_checkedKeys.insert(k);
    }
    QModelIndex mi = createIndex(index, 0);
    emit dataChanged(mi, mi);
    emit checkedChanged();
}

void FolderModel::clearChecked()
{
    if (m_checked.isEmpty())
        return;
    m_checked.clear();
    m_checkedKeys.clear();
    if (!m_entries.isEmpty())
        emit dataChanged(createIndex(0, 0), createIndex(m_entries.size() - 1, 0));
    emit checkedChanged();
}

QStringList FolderModel::checkedFiles() const
{
    QStringList paths;
    foreach (const Checked &c, m_checked) {
        if (!c.isDir)
            paths << c.path;
    }
    return paths;
}

QStringList FolderModel::checkedFolders() const
{
    QStringList paths;
    foreach (const Checked &c, m_checked) {
        if (c.isDir)
            paths << c.path;
    }
    return paths;
}
