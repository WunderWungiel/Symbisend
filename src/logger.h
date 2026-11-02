#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QStringList>
#include <QFile>

// Captures qDebug/qWarning output: writes it to a log file and keeps the last
// lines in memory so QML can show them on a debug page.
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

private:
    explicit Logger(QObject *parent = 0);
    void append(const QString &line);
    static void messageHandler(QtMsgType type, const char *msg);

    QStringList m_lines;
    QFile m_file;
    bool m_inHandler;
};

#endif // LOGGER_H
