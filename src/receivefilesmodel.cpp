#include "receivefilesmodel.h"

#include <QDebug>

#include "selectionmodel.h"

// The official limit for a file name, in bytes.
static const int kMaxNameBytes = 255;

ReceiveFilesModel::ReceiveFilesModel(QObject *parent)
    : QAbstractListModel(parent)
{
    QHash<int, QByteArray> roles;
    roles[OriginalNameRole] = "originalName";
    roles[NameRole] = "name";
    roles[EditNameRole] = "editName";
    roles[KindRole] = "kind";
    roles[SizeRole] = "size";
    roles[SizeTextRole] = "sizeText";
    roles[SelectedRole] = "selected";
    roles[RenamedRole] = "renamed";
    setRoleNames(roles);
}

int ReceiveFilesModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_files.size();
}

QVariant ReceiveFilesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_files.size())
        return QVariant();
    const File &f = m_files.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case NameRole: return f.name;
    case OriginalNameRole: return f.originalName;
    case EditNameRole: return f.name.mid(leafStart(f.name));
    case KindRole: return SelectionModel::kindForFile(f.name);
    case SizeRole: return qreal(f.size);
    case SizeTextRole: return SelectionModel::formatBytes(qreal(f.size));
    case SelectedRole: return f.selected;
    case RenamedRole: return f.name != f.originalName;
    default: return QVariant();
    }
}

int ReceiveFilesModel::selectedCount() const
{
    int n = 0;
    foreach (const File &f, m_files) {
        if (f.selected)
            ++n;
    }
    return n;
}

void ReceiveFilesModel::setFiles(const QStringList &names, const QList<qint64> &sizes)
{
    beginResetModel();
    m_files.clear();
    for (int i = 0; i < names.size(); ++i) {
        File f;
        f.originalName = f.name = names.at(i);
        f.size = sizes.at(i);
        f.selected = true;
        m_files.append(f);
    }
    endResetModel();
    emit countChanged();
    emit selectionChanged();
}

bool ReceiveFilesModel::isSelected(int row) const
{
    return row >= 0 && row < m_files.size() && m_files.at(row).selected;
}

QString ReceiveFilesModel::name(int row) const
{
    return row >= 0 && row < m_files.size() ? m_files.at(row).name : QString();
}

void ReceiveFilesModel::setSelected(int row, bool selected)
{
    if (row < 0 || row >= m_files.size() || m_files.at(row).selected == selected)
        return;
    File &f = m_files[row];
    f.selected = selected;
    f.name = f.originalName;
    const QModelIndex i = index(row);
    emit dataChanged(i, i);
    emit selectionChanged();
}

QString ReceiveFilesModel::nameError(const QString &input) const
{
    const QString name = input.trimmed();
    if (name.isEmpty())
        return QLatin1String("Filename cannot be empty");
    static const QString bad = QLatin1String("<>:\"/\\|?*");
    bool valid = name != QLatin1String(".") && name != QLatin1String("..")
            && !name.endsWith(QLatin1Char('.')) // Symbian drops it, as Windows does
            && name.toUtf8().size() <= kMaxNameBytes;
    for (int i = 0; valid && i < name.size(); ++i)
        valid = name.at(i).unicode() >= 32 && !bad.contains(name.at(i));
    return valid ? QString() : QString::fromLatin1("Filename contains invalid characters");
}

bool ReceiveFilesModel::rename(int row, const QString &input)
{
    if (row < 0 || row >= m_files.size() || !nameError(input).isEmpty())
        return false;
    File &f = m_files[row];
    const QString original = f.originalName.mid(leafStart(f.originalName));
    QString leaf = input.trimmed();
    // Like the official app: the user forgot the extension, we fix it.
    const int dot = original.lastIndexOf(QLatin1Char('.'));
    if (!leaf.contains(QLatin1Char('.')) && dot >= 0 && dot < original.size() - 1)
        leaf += QLatin1Char('.') + original.mid(dot + 1).toLower();
    f.name = f.originalName.left(leafStart(f.originalName)) + leaf;
    qDebug() << "ReceiveFiles:" << f.originalName << "will be saved as" << f.name;
    const QModelIndex i = index(row);
    emit dataChanged(i, i);
    return true;
}

void ReceiveFilesModel::reset()
{
    if (m_files.isEmpty())
        return;
    for (int i = 0; i < m_files.size(); ++i) {
        m_files[i].name = m_files.at(i).originalName;
        m_files[i].selected = true;
    }
    emit dataChanged(index(0), index(m_files.size() - 1));
    emit selectionChanged();
}

// Where the last part of a path begins.
int ReceiveFilesModel::leafStart(const QString &path)
{
    return qMax(path.lastIndexOf(QLatin1Char('/')), path.lastIndexOf(QLatin1Char('\\'))) + 1;
}
