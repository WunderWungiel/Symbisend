#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QStringList>
#include <QFile>
#include <QMutex>
#include <QTimer>

// Captures qDebug/qWarning output: writes it to a log file and keeps the last
// lines in memory so QML can show them on a debug page.
//
// The message handler may be called from any thread (Qt 4.8 runs
// QNetworkAccessManager's HTTP code in a worker thread), so all state is
// guarded by a mutex and logChanged is emitted from the GUI thread only,
// batched so the log page does not re-layout on every single line.
class Logger : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString logText READ logText NOTIFY logChanged)
    Q_PROPERTY(QString logFilePath READ logFilePath CONSTANT)

public:
    static Logger *instance();
    static void install();

    QString logText() const;
    QString logFilePath() const;

    Q_INVOKABLE void clear();

signals:
    void logChanged();

private slots:
    void scheduleNotify();

private:
    explicit Logger(QObject *parent = 0);
    void append(const QString &line);
    static void messageHandler(QtMsgType type, const char *msg);

    mutable QMutex m_mutex; // recursive: a warning raised while logging must not deadlock
    QStringList m_lines;
    QFile m_file;
    bool m_inHandler;       // same-thread recursion guard (other threads block on m_mutex)
    bool m_notifyPending;
    QTimer m_notifyTimer;
};

#endif // LOGGER_H
