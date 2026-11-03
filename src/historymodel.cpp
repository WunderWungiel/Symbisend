#include "historymodel.h"

#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QSettings>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include "json.h"
#include "selectionmodel.h"

// {"entries":[{...}, ...]} as one string (Json::parseObject wants an object).
static const char *kEntriesKey = "history/entries";
static const int kSaveDelayMs = 1000;

HistoryModel::HistoryModel(QObject *parent)
    : QAbstractListModel(parent)
{
    QHash<int, QByteArray> roles;
    roles[KindRole] = "kind";
    roles[FileNameRole] = "fileName";
    roles[PathRole] = "path";
    roles[TextRole] = "text";
    roles[SizeRole] = "size";
    roles[SizeTextRole] = "sizeText";
    roles[TimeRole] = "time";
    roles[TimeTextRole] = "timeText";
    roles[SenderAliasRole] = "senderAlias";
    setRoleNames(roles);

    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(kSaveDelayMs);
    connect(&m_saveTimer, SIGNAL(timeout()), this, SLOT(save()));

    load();
}

HistoryModel::~HistoryModel()
{
    if (m_saveTimer.isActive())
        save();
}

int HistoryModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

QVariant HistoryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return QVariant();
    const HistoryEntry &e = m_entries.at(index.row());
    switch (role) {
    case KindRole: return e.kind;
    case FileNameRole: return e.fileName;
    case PathRole: return QDir::toNativeSeparators(e.path);
    case TextRole: return e.text;
    case SizeRole: return qreal(e.size);
    case SizeTextRole: return SelectionModel::formatBytes(qreal(e.size));
    case TimeRole: return e.time;
    case TimeTextRole: return QLocale::system().toString(e.time, QLocale::ShortFormat);
    case SenderAliasRole: return e.senderAlias;
    }
    return QVariant();
}

bool HistoryModel::openFile(int row)
{
    if (row < 0 || row >= m_entries.size())
        return false;
    const HistoryEntry &e = m_entries.at(row);
    if (e.path.isEmpty())
        return false;
    if (!QFileInfo(e.path).exists()) {
        qWarning() << "History: cannot open" << e.path << "- file no longer exists";
        return false;
    }
    bool ok = QDesktopServices::openUrl(QUrl::fromLocalFile(e.path));
    if (ok)
        qDebug() << "History: opened" << e.path;
    else
        qWarning() << "History: no application could open" << e.path;
    return ok;
}

void HistoryModel::addFile(const QString &path, qint64 size, const QString &senderAlias)
{
    HistoryEntry e;
    e.fileName = QFileInfo(path).fileName();
    e.kind = SelectionModel::kindForFile(e.fileName);
    e.path = path;
    e.size = size;
    e.time = QDateTime::currentDateTime();
    e.senderAlias = senderAlias;
    prepend(e);
}

void HistoryModel::addText(const QString &text, const QString &senderAlias)
{
    HistoryEntry e;
    e.kind = QString::fromLatin1("text");
    e.text = text;
    e.size = text.toUtf8().size();
    e.time = QDateTime::currentDateTime();
    e.senderAlias = senderAlias;
    prepend(e);
}

void HistoryModel::prepend(const HistoryEntry &entry)
{
    beginInsertRows(QModelIndex(), 0, 0);
    m_entries.prepend(entry);
    endInsertRows();
    if (m_entries.size() > MaxEntries) {
        beginRemoveRows(QModelIndex(), MaxEntries, m_entries.size() - 1);
        while (m_entries.size() > MaxEntries)
            m_entries.removeLast();
        endRemoveRows();
    }
    emit countChanged();
    m_saveTimer.start();
}

void HistoryModel::save()
{
    m_saveTimer.stop();
    QVariantList list;
    foreach (const HistoryEntry &e, m_entries) {
        QVariantMap m;
        m.insert(QLatin1String("kind"), e.kind);
        if (!e.path.isEmpty())
            m.insert(QLatin1String("path"), e.path);
        if (e.kind == QLatin1String("text"))
            m.insert(QLatin1String("text"), e.text);
        m.insert(QLatin1String("size"), qreal(e.size));
        m.insert(QLatin1String("time"), e.time.toString(Qt::ISODate));
        m.insert(QLatin1String("sender"), e.senderAlias);
        list.append(m);
    }
    QVariantMap root;
    root.insert(QLatin1String("entries"), list);
    QSettings().setValue(QLatin1String(kEntriesKey), QString::fromUtf8(Json::serialize(root)));
}

void HistoryModel::load()
{
    const QString stored = QSettings().value(QLatin1String(kEntriesKey)).toString();
    if (stored.isEmpty())
        return;
    bool ok = false;
    const QVariantMap root = Json::parseObject(stored.toUtf8(), &ok);
    if (!ok) {
        qWarning() << "History: stored history is unreadable, starting empty";
        return;
    }
    foreach (const QVariant &v, root.value(QLatin1String("entries")).toList()) {
        const QVariantMap m = v.toMap();
        HistoryEntry e;
        e.kind = m.value(QLatin1String("kind")).toString();
        e.path = m.value(QLatin1String("path")).toString();
        e.fileName = QFileInfo(e.path).fileName();
        e.text = m.value(QLatin1String("text")).toString();
        e.size = qint64(m.value(QLatin1String("size")).toDouble());
        e.time = QDateTime::fromString(m.value(QLatin1String("time")).toString(), Qt::ISODate);
        e.senderAlias = m.value(QLatin1String("sender")).toString();
        if (e.kind.isEmpty() || (e.path.isEmpty() && e.kind != QLatin1String("text")))
            continue;
        m_entries.append(e);
        if (m_entries.size() >= MaxEntries)
            break;
    }
    qDebug() << "History:" << m_entries.size() << "entries";
}
