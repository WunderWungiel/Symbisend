#include "logger.h"

#include <QDateTime>
#include <QDir>
#include <QMetaObject>
#include <QMutexLocker>
#include <QTextStream>
#include <cstdio>
#include <cstdlib>

static const int kMaxLines = 300;

static QString pickLogDir()
{
#ifdef Q_OS_SYMBIAN
    // E: is mass memory on most Symbian^3 phones (808, N8); fall back to C:/Data.
    const char *candidates[] = { "E:/Symbisend", "F:/Symbisend", "C:/Data/Symbisend" };
    for (unsigned i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        QString path = QString::fromLatin1(candidates[i]);
        QDir root(path.left(3));
        if (root.exists() && QDir().mkpath(path))
            return path;
    }
    return QDir::currentPath();
#else
    QString path = QDir::homePath() + QLatin1String("/.symbisend");
    QDir().mkpath(path);
    return path;
#endif
}

static const int kNotifyDelayMs = 200;

// ---------------------------------------------------------------- LogModel

LogModel::LogModel(int maxLines, QObject *parent)
    : QAbstractListModel(parent), m_maxLines(maxLines)
{
    QHash<int, QByteArray> roles;
    roles[LineRole] = "line";
    setRoleNames(roles);
}

int LogModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_lines.size();
}

QVariant LogModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_lines.size())
        return QVariant();
    if (role == LineRole || role == Qt::DisplayRole)
        return m_lines.at(index.row());
    return QVariant();
}

void LogModel::appendLines(const QStringList &lines)
{
    if (lines.isEmpty())
        return;
    if (lines.size() >= m_maxLines) {
        // The batch alone replaces everything.
        beginResetModel();
        m_lines = lines.mid(lines.size() - m_maxLines);
        endResetModel();
        return;
    }
    beginInsertRows(QModelIndex(), m_lines.size(), m_lines.size() + lines.size() - 1);
    m_lines += lines;
    endInsertRows();
    const int excess = m_lines.size() - m_maxLines;
    if (excess > 0) {
        beginRemoveRows(QModelIndex(), 0, excess - 1);
        m_lines.erase(m_lines.begin(), m_lines.begin() + excess);
        endRemoveRows();
    }
}

void LogModel::clear()
{
    if (m_lines.isEmpty())
        return;
    beginResetModel();
    m_lines.clear();
    endResetModel();
}

// ---------------------------------------------------------------- Logger

Logger::Logger(QObject *parent)
    : QObject(parent), m_mutex(QMutex::Recursive), m_inHandler(false), m_notifyPending(false),
      m_model(new LogModel(kMaxLines, this))
{
    m_file.setFileName(pickLogDir() + QLatin1String("/log.txt"));
    // Keep only the current session's log.
    m_file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);

    m_notifyTimer.setSingleShot(true);
    m_notifyTimer.setInterval(kNotifyDelayMs);
    connect(&m_notifyTimer, SIGNAL(timeout()), this, SLOT(flushPending()));
}

Logger *Logger::instance()
{
    static Logger *s_instance = 0;
    if (!s_instance)
        s_instance = new Logger;
    return s_instance;
}

void Logger::install()
{
    instance();
    qInstallMsgHandler(&Logger::messageHandler);
}

// GUI thread.
QString Logger::logText() const
{
    return m_model->lines().join(QLatin1String("\n"));
}

QString Logger::logFilePath() const
{
    return m_file.fileName();
}

void Logger::clear()
{
    {
        QMutexLocker lock(&m_mutex);
        m_pending.clear();
    }
    m_model->clear();
    emit logChanged();
}

// GUI thread: start the batching timer (flushPending() runs when it expires).
void Logger::scheduleNotify()
{
    if (!m_notifyTimer.isActive())
        m_notifyTimer.start();
}

// GUI thread: move the queued lines into the model. Lines logged while the
// model updates (e.g. by a QML binding) are queued for the next batch.
void Logger::flushPending()
{
    QStringList lines;
    {
        QMutexLocker lock(&m_mutex);
        lines = m_pending; // implicitly shared; QList::swap() is Qt 4.8+
        m_pending.clear();
        m_notifyPending = false;
    }
    if (lines.isEmpty())
        return;
    m_model->appendLines(lines);
    emit logChanged();
}

// Called with m_mutex held, from any thread.
void Logger::append(const QString &line)
{
    if (m_file.isOpen()) {
        QByteArray data = line.toUtf8();
        data.append('\n');
        m_file.write(data);
        m_file.flush(); // keep the file complete in case the app crashes
    }
    m_pending.append(line);
    while (m_pending.size() > kMaxLines)
        m_pending.removeFirst();
    if (!m_notifyPending) {
        m_notifyPending = true;
        // Never touch QML-bound state from here: hop to the GUI thread.
        QMetaObject::invokeMethod(this, "scheduleNotify", Qt::QueuedConnection);
    }
}

void Logger::messageHandler(QtMsgType type, const char *msg)
{
    Logger *self = instance();
    const char *prefix = "D";
    switch (type) {
    case QtDebugMsg: prefix = "D"; break;
    case QtWarningMsg: prefix = "W"; break;
    case QtCriticalMsg: prefix = "C"; break;
    case QtFatalMsg: prefix = "F"; break;
    }
    QString line = QString::fromLatin1("%1 %2 %3")
            .arg(QTime::currentTime().toString(QLatin1String("hh:mm:ss.zzz")))
            .arg(QLatin1String(prefix))
            .arg(QString::fromUtf8(msg));

#ifndef Q_OS_SYMBIAN
    fprintf(stderr, "%s\n", line.toLocal8Bit().constData());
#endif

    {
        QMutexLocker lock(&self->m_mutex);
        // A message raised while appending (same thread, since other threads
        // block on the mutex) is dropped instead of recursing.
        if (!self->m_inHandler) {
            self->m_inHandler = true;
            self->append(line);
            self->m_inHandler = false;
        }
    }

    if (type == QtFatalMsg)
        abort();
}
