#include "historymodel.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QSettings>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include "json.h"
#include "selectionmodel.h"

#ifdef Q_OS_SYMBIAN
#include <apacmdln.h>
#include <apgcli.h>
#include <apgtask.h>
#include <coemain.h>
#endif

// Where the history used to be stored (one QSettings string); moved to
// filePath() on the first start of a newer version.
static const char *kOldEntriesKey = "history/entries";
static const int kSaveDelayMs = 1000;

HistoryModel::HistoryModel(QObject *parent)
    : QAbstractListModel(parent), m_locale(QLocale::system())
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
    roles[PreviewRole] = "preview";
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
    case TimeTextRole: return m_locale.toString(e.time, QLocale::ShortFormat);
    case SenderAliasRole: return e.senderAlias;
    case PreviewRole: return e.kind == QLatin1String("text") ? e.preview : e.fileName;
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

#ifdef Q_OS_SYMBIAN
// The stock File manager app (FileMgr.exe).
static const TUid kFileManagerUid = { 0x101F84EB };

static TPtrC toPtr(const QString &s)
{
    return TPtrC(reinterpret_cast<const TUint16 *>(s.utf16()), s.length());
}

// Starts File manager with `folder` (native separators, trailing '\') as the
// document to open; it then shows that folder. If it is already running, the
// folder is handed to the running instance and it is brought to the front.
static void openInFileManagerL(const QString &folder)
{
    TApaTaskList tasks(CCoeEnv::Static()->WsSession());
    TApaTask task = tasks.FindApp(kFileManagerUid);
    if (task.Exists()) {
        qDebug() << "History: File manager is running, switching it to" << folder;
        task.SwitchOpenFile(toPtr(folder));
        task.BringToForeground();
        return;
    }

    RApaLsSession ls;
    User::LeaveIfError(ls.Connect());
    CleanupClosePushL(ls);
    TApaAppInfo info;
    User::LeaveIfError(ls.GetAppInfo(info, kFileManagerUid));
    CApaCommandLine *cmd = CApaCommandLine::NewLC();
    cmd->SetExecutableNameL(info.iFullName);
    cmd->SetDocumentNameL(toPtr(folder));
    cmd->SetCommandL(EApaCommandOpen);
    User::LeaveIfError(ls.StartApp(*cmd));
    CleanupStack::PopAndDestroy(2); // cmd, ls
}
#endif

bool HistoryModel::openFolder(const QString &folder)
{
    const QString path = QDir::fromNativeSeparators(folder);
    if (path.isEmpty())
        return false;
    if (!QDir(path).exists() && !QDir().mkpath(path)) {
        qWarning() << "History: cannot open folder" << path << "- it does not exist and cannot be created";
        return false;
    }
#ifdef Q_OS_SYMBIAN
    QString native = QDir::toNativeSeparators(path);
    if (!native.endsWith(QLatin1Char('\\')))
        native += QLatin1Char('\\');
    TRAPD(err, openInFileManagerL(native));
    if (err != KErrNone) {
        qWarning() << "History: cannot start File manager for" << native << "- error" << err;
        return false;
    }
    qDebug() << "History: opened folder" << native << "in File manager";
    return true;
#else
    bool ok = QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    if (ok)
        qDebug() << "History: opened folder" << path;
    else
        qWarning() << "History: nothing could open folder" << path;
    return ok;
#endif
}

void HistoryModel::clear()
{
    if (m_entries.isEmpty())
        return;
    qDebug() << "History: cleared" << m_entries.size() << "entries";
    beginResetModel();
    m_entries.clear();
    endResetModel();
    emit countChanged();
    save();
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
    e.size = text.toUtf8().size();
    if (text.size() > MaxTextLength) {
        qDebug() << "History: message of" << text.size() << "characters kept shortened to"
                 << MaxTextLength;
        e.text = text.left(MaxTextLength) + QChar(0x2026); // "..."
    } else {
        e.text = text;
    }
    e.preview = previewOf(e.text);
    e.time = QDateTime::currentDateTime();
    e.senderAlias = senderAlias;
    prepend(e);
}

// The first non-empty line, at most MaxPreviewLength characters, found
// without splitting the whole text.
QString HistoryModel::previewOf(const QString &text)
{
    int start = 0;
    while (start < text.size() && text.at(start).isSpace())
        ++start;
    int end = start;
    const int limit = qMin(text.size(), start + MaxPreviewLength);
    while (end < limit && text.at(end) != QLatin1Char('\n') && text.at(end) != QLatin1Char('\r'))
        ++end;
    QString line = text.mid(start, end - start);
    if (end == limit && limit < text.size() && text.at(limit) != QLatin1Char('\n')
            && text.at(limit) != QLatin1Char('\r'))
        line += QChar(0x2026); // cut in the middle of the line
    return line;
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
    const QByteArray data = Json::serialize(root);
    list.clear();
    root.clear();

    // Written next to the old file and then swapped in, so a crash or a full
    // disk in the middle leaves the previous history intact.
    const QString path = filePath();
    const QString temp = path + QLatin1String(".new");
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile out(temp);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "History: cannot write" << temp << out.errorString();
        return;
    }
    const bool written = out.write(data) == data.size();
    out.close();
    if (!written || out.error() != QFile::NoError) {
        qWarning() << "History: writing" << temp << "failed:" << out.errorString();
        QFile::remove(temp);
        return;
    }
    QFile::remove(path);
    if (!QFile::rename(temp, path))
        qWarning() << "History: cannot replace" << path;
}

// history.json in the app's own data folder. Symbian: the private folder
// (applicationDirPath() is that there), which needs no capabilities.
QString HistoryModel::filePath()
{
#ifdef Q_OS_SYMBIAN
    const QString dir = QCoreApplication::applicationDirPath();
#else
    QString dir = QDesktopServices::storageLocation(QDesktopServices::DataLocation);
    if (dir.isEmpty())
        dir = QDir::homePath() + QLatin1String("/.symbisend");
#endif
    return QDir::fromNativeSeparators(dir) + QLatin1String("/history.json");
}

void HistoryModel::load()
{
    if (!loadFromFile())
        migrateFromSettings();
    qDebug() << "History:" << m_entries.size() << "entries";
}

// False if there is no history file yet.
bool HistoryModel::loadFromFile()
{
    QFile in(filePath());
    if (!in.exists()) {
        // A save that was cut short before its rename.
        if (QFile::exists(filePath() + QLatin1String(".new")))
            QFile::rename(filePath() + QLatin1String(".new"), filePath());
        if (!in.exists())
            return false;
    }
    if (!in.open(QIODevice::ReadOnly)) {
        qWarning() << "History: cannot read" << in.fileName() << in.errorString();
        return true;
    }
    bool ok = false;
    const QVariantMap root = Json::parseObject(in.readAll(), &ok);
    in.close();
    if (!ok) {
        qWarning() << "History:" << in.fileName() << "is unreadable, starting empty";
        return true;
    }
    foreach (const QVariant &v, root.value(QLatin1String("entries")).toList()) {
        addLoaded(v.toMap());
        if (m_entries.size() >= MaxEntries)
            break;
    }
    return true;
}

// Versions before history.json kept the history in QSettings: move it over
// once and drop it from the settings file.
void HistoryModel::migrateFromSettings()
{
    QSettings settings;
    const QString stored = settings.value(QLatin1String(kOldEntriesKey)).toString();
    if (stored.isEmpty())
        return;
    bool ok = false;
    const QVariantMap root = Json::parseObject(stored.toUtf8(), &ok);
    if (ok) {
        foreach (const QVariant &v, root.value(QLatin1String("entries")).toList()) {
            addLoaded(v.toMap());
            if (m_entries.size() >= MaxEntries)
                break;
        }
    } else {
        qWarning() << "History: stored history is unreadable, starting empty";
    }
    save();
    if (QFile::exists(filePath())) {
        settings.remove(QLatin1String(kOldEntriesKey));
        qDebug() << "History: moved" << m_entries.size() << "entries from the settings to" << filePath();
    }
}

void HistoryModel::addLoaded(const QVariantMap &m)
{
    HistoryEntry e;
    e.kind = m.value(QLatin1String("kind")).toString();
    e.path = m.value(QLatin1String("path")).toString();
    e.fileName = QFileInfo(e.path).fileName();
    e.text = m.value(QLatin1String("text")).toString();
    if (e.text.size() > MaxTextLength)
        e.text = e.text.left(MaxTextLength) + QChar(0x2026);
    e.preview = previewOf(e.text);
    e.size = qint64(m.value(QLatin1String("size")).toDouble());
    e.time = QDateTime::fromString(m.value(QLatin1String("time")).toString(), Qt::ISODate);
    e.senderAlias = m.value(QLatin1String("sender")).toString();
    if (e.kind.isEmpty() || (e.path.isEmpty() && e.kind != QLatin1String("text")))
        return;
    m_entries.append(e);
}
