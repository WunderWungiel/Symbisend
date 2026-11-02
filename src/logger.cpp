#include "logger.h"

#include <QDateTime>
#include <QDir>
#include <QMetaObject>
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

Logger::Logger(QObject *parent)
    : QObject(parent), m_inHandler(false)
{
    m_file.setFileName(pickLogDir() + QLatin1String("/log.txt"));
    // Keep only the current session's log.
    m_file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
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
    return m_lines.join(QLatin1String("\n"));
}

QString Logger::logFilePath() const
{
    return m_file.fileName();
}

void Logger::clear()
{
    m_lines.clear();
    emit logChanged();
}

void Logger::append(const QString &line)
{
    if (m_file.isOpen()) {
        QByteArray data = line.toUtf8();
        data.append('\n');
        m_file.write(data);
        m_file.flush();
    }
    m_lines.append(line);
    while (m_lines.size() > kMaxLines)
        m_lines.removeFirst();
    emit logChanged();
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

    // Guard against recursion (e.g. QML binding warnings emitted while the
    // log page re-evaluates logText).
    if (self->m_inHandler)
        return;
    self->m_inHandler = true;
    self->append(line);
    self->m_inHandler = false;

    if (type == QtFatalMsg)
        abort();
}
