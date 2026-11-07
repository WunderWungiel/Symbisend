// SPDX-FileCopyrightText: 2026 Wunder Wungiel
// SPDX-License-Identifier: GPL-3.0-or-later

#include "selectionmodel.h"

#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

SelectionModel::SelectionModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_totalSize(0)
{
    QHash<int, QByteArray> roles;
    roles[KindRole] = "kind";
    roles[NameRole] = "name";
    roles[PathRole] = "path";
    roles[RelativePathRole] = "relativePath";
    roles[TextRole] = "text";
    roles[SizeRole] = "size";
    roles[SizeTextRole] = "sizeText";
    setRoleNames(roles);
}

int SelectionModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

QVariant SelectionModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
        return QVariant();
    const SelectionItem &it = m_items.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case NameRole: return it.name;
    case KindRole: return it.kind;
    case PathRole: return it.path.isEmpty() ? QString() : QDir::toNativeSeparators(it.path);
    case RelativePathRole: return it.relativePath;
    case TextRole: return it.text;
    case SizeRole: return qreal(it.size);
    case SizeTextRole: return formatSize(qreal(it.size));
    }
    return QVariant();
}

qreal SelectionModel::totalSize() const
{
    return qreal(m_totalSize);
}

QString SelectionModel::totalSizeText() const
{
    return formatSize(qreal(m_totalSize));
}

QString SelectionModel::formatBytes(qreal bytes)
{
    // Same steps as LocalSend's byte formatting: 1024-based, one decimal.
    if (bytes < 1024)
        return QString::fromLatin1("%1 B").arg(qint64(bytes));
    static const char *const units[] = { "KB", "MB", "GB", "TB" };
    int unit = -1;
    while (bytes >= 1024 && unit < 3) {
        bytes /= 1024;
        ++unit;
    }
    return QString::fromLatin1("%1 %2").arg(bytes, 0, 'f', 1).arg(QLatin1String(units[unit]));
}

QString SelectionModel::kindForFile(const QString &fileName)
{
    QString ext = QFileInfo(fileName).suffix().toLower();
    static const QStringList images = QString::fromLatin1(
            "jpg jpeg png gif bmp webp heic heif tif tiff svg").split(QLatin1Char(' '));
    static const QStringList videos = QString::fromLatin1(
            "mp4 3gp 3g2 mkv avi mov wmv flv webm m4v mpg mpeg").split(QLatin1Char(' '));
    if (images.contains(ext))
        return QString::fromLatin1("image");
    if (videos.contains(ext))
        return QString::fromLatin1("video");
    return QString::fromLatin1("file");
}

bool SelectionModel::containsPath(const QString &path) const
{
    return m_pathKeys.contains(pathKey(path));
}

void SelectionModel::append(const QList<SelectionItem> &items)
{
    if (items.isEmpty())
        return;
    beginInsertRows(QModelIndex(), m_items.size(), m_items.size() + items.size() - 1);
    foreach (const SelectionItem &it, items) {
        m_items << it;
        m_totalSize += it.size;
        if (!it.path.isEmpty())
            m_pathKeys.insert(pathKey(it.path));
    }
    endInsertRows();
    qDebug() << "Selection: now" << m_items.size() << "items," << totalSizeText();
    emit countChanged();
}

void SelectionModel::addFiles(const QStringList &paths)
{
    QList<SelectionItem> added;
    QSet<QString> addedKeys; // the same file twice in one pick
    foreach (const QString &native, paths) {
        QString path = QDir::cleanPath(QDir::fromNativeSeparators(native));
        QFileInfo fi(path);
        if (!fi.isFile()) {
            qWarning() << "Selection: not a file, skipped:" << native;
            continue;
        }
        if (containsPath(path) || addedKeys.contains(pathKey(path))) {
            qDebug() << "Selection: already selected, skipped:" << native;
            continue;
        }
        addedKeys.insert(pathKey(path));
        SelectionItem it;
        it.kind = kindForFile(fi.fileName());
        it.path = path;
        it.name = fi.fileName();
        it.relativePath = it.name;
        it.size = fi.size();
        qDebug() << "Selection: + " << it.kind << native << formatSize(qreal(it.size));
        added << it;
    }
    append(added);
}

void SelectionModel::addFolder(const QString &native)
{
    QString root = QDir::cleanPath(QDir::fromNativeSeparators(native));
    QFileInfo rootInfo(root);
    if (!rootInfo.isDir()) {
        qWarning() << "Selection: not a folder, skipped:" << native;
        return;
    }
    // "E:/Images/Trip" -> "Trip"; a drive root ("E:/") has no name, use "E".
    QString folderName = rootInfo.fileName();
    if (folderName.isEmpty())
        folderName = root.left(1);
    QDir rootDir(root);

    QList<SelectionItem> added;
    int skipped = 0;
    bool truncated = false;
    // Without QDir::Hidden the iterator skips hidden files and does not
    // descend into hidden folders (system/cache data, not the user's files).
    QDirIterator iter(root, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (iter.hasNext()) {
        if (added.size() >= MaxFolderFiles) {
            truncated = true;
            break;
        }
        QString path = iter.next();
        if (containsPath(path)) {
            ++skipped;
            continue;
        }
        QFileInfo fi = iter.fileInfo();
        SelectionItem it;
        it.kind = kindForFile(fi.fileName());
        it.path = path;
        it.name = fi.fileName();
        it.relativePath = folderName + QLatin1Char('/') + rootDir.relativeFilePath(path);
        it.size = fi.size();
        added << it;
    }
    qDebug() << "Selection: folder" << native << "->" << added.size() << "files"
             << (skipped ? QString::fromLatin1("(%1 already selected)").arg(skipped) : QString());
    if (added.isEmpty())
        qDebug() << "Selection: folder has no new files";
    append(added);
    if (truncated) {
        qWarning() << "Selection: folder" << native << "has more than" << MaxFolderFiles
                   << "files, the rest was not added";
        emit folderTruncated(QDir::toNativeSeparators(root), MaxFolderFiles);
    }
}

void SelectionModel::addText(const QString &text)
{
    if (text.isEmpty()) {
        qDebug() << "Selection: empty text ignored";
        return;
    }
    SelectionItem it;
    it.kind = QString::fromLatin1("text");
    it.name = text;
    it.text = text;
    it.size = text.toUtf8().size();
    qDebug() << "Selection: + text," << text.length() << "chars";
    append(QList<SelectionItem>() << it);
}

void SelectionModel::setText(int index, const QString &text)
{
    if (index < 0 || index >= m_items.size() || m_items.at(index).kind != QLatin1String("text"))
        return;
    if (text.isEmpty()) {
        remove(index);
        return;
    }
    SelectionItem &it = m_items[index];
    m_totalSize -= it.size;
    it.name = text;
    it.text = text;
    it.size = text.toUtf8().size();
    m_totalSize += it.size;
    QModelIndex mi = createIndex(index, 0);
    emit dataChanged(mi, mi);
    qDebug() << "Selection: text" << index << "edited," << text.length() << "chars";
    emit countChanged();
}

void SelectionModel::remove(int index)
{
    if (index < 0 || index >= m_items.size())
        return;
    beginRemoveRows(QModelIndex(), index, index);
    m_totalSize -= m_items.at(index).size;
    if (!m_items.at(index).path.isEmpty())
        m_pathKeys.remove(pathKey(m_items.at(index).path));
    m_items.removeAt(index);
    endRemoveRows();
    qDebug() << "Selection: removed item" << index << "-" << m_items.size() << "left";
    emit countChanged();
}

void SelectionModel::clear()
{
    if (m_items.isEmpty())
        return;
    beginResetModel();
    m_items.clear();
    m_pathKeys.clear();
    m_totalSize = 0;
    endResetModel();
    qDebug() << "Selection: cleared";
    emit countChanged();
}
