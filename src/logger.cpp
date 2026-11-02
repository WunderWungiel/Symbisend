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

Logger::Logger(QObject *parent)
    : QObject(parent), m_mutex(QMutex::Recursive), m_inHandler(false), m_notifyPending(false)
{
    m_file.setFileName(pickLogDir() + QLatin1String("/log.txt"));
    // Keep only the current session's log.
    m_file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);

    m_notifyTimer.setSingleShot(true);
    m_notifyTimer.setInterval(kNotifyDelayMs);
    connect(&m_notifyTimer, SIGNAL(timeout()), this, SIGNAL(logChanged()));
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

QString Logger::logText() const
{
    QMutexLocker lock(&m_mutex);
    return m_lines.join(QLatin1String("\n"));
}

QString Logger::logFilePath() const
{
    return m_file.fileName();
}

void Logger::clear()
{
    {
        QMutexLocker lock(&m_mutex);
        m_lines.clear();
    }
    emit logChanged();
}

// GUI thread: start the batching timer (logChanged fires when it expires).
void Logger::scheduleNotify()
{
    {
        QMutexLocker lock(&m_mutex);
        m_notifyPending = false;
    }
    if (!m_notifyTimer.isActive())
        m_notifyTimer.start();
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
    m_lines.append(line);
    while (m_lines.size() > kMaxLines)
        m_lines.removeFirst();
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
