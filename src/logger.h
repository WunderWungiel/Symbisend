// SPDX-FileCopyrightText: 2026 Wunder Wungiel
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LOGGER_H
#define LOGGER_H

#include <QAbstractListModel>
#include <QObject>
#include <QStringList>
#include <QFile>
#include <QMutex>
#include <QTimer>

// The last log lines as a list model (role "line"), for the Log page. Lives
// in the GUI thread; Logger feeds it in batches.
class LogModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles { LineRole = Qt::UserRole + 1 };

    explicit LogModel(int maxLines, QObject *parent = 0);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;

    // Appends, then drops the oldest lines beyond maxLines.
    void appendLines(const QStringList &lines);
    void clear();
    const QStringList &lines() const { return m_lines; }

private:
    int m_maxLines;
    QStringList m_lines;
};

// Captures qDebug/qWarning output: writes it to a log file and keeps the last
// lines in memory so QML can show them on a debug page.
//
// The message handler may be called from any thread (Qt 4.8 runs
// QNetworkAccessManager's HTTP code in a worker thread), so new lines are
// queued under a mutex and moved into the model from the GUI thread only,
// batched so the log page does not re-layout on every single line.
class Logger : public QObject
{
    Q_OBJECT
    // All lines in the model joined with "\n" (built on every read).
    Q_PROPERTY(QString logText READ logText NOTIFY logChanged)
    // LogModel with the last lines; use as a ListView model.
    Q_PROPERTY(QObject *model READ model CONSTANT)
    Q_PROPERTY(QString logFilePath READ logFilePath CONSTANT)

public:
    static Logger *instance();
    static void install();

    QString logText() const;
    QObject *model() const { return m_model; }
    QString logFilePath() const;

    Q_INVOKABLE void clear();

signals:
    void logChanged();

private slots:
    void scheduleNotify();
    void flushPending();

private:
    explicit Logger(QObject *parent = 0);
    void append(const QString &line);
    static void messageHandler(QtMsgType type, const char *msg);

    mutable QMutex m_mutex; // recursive: a warning raised while logging must not deadlock
    QStringList m_pending;  // lines not yet in the model (guarded by m_mutex)
    QFile m_file;           // guarded by m_mutex
    bool m_inHandler;       // same-thread recursion guard (other threads block on m_mutex)
    bool m_notifyPending;   // a flush is scheduled (guarded by m_mutex)
    QTimer m_notifyTimer;
    LogModel *m_model;      // GUI thread only
};

#endif // LOGGER_H
