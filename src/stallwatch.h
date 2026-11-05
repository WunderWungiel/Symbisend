#ifndef STALLWATCH_H
#define STALLWATCH_H

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

// Reports in the log when the UI thread's event loop stops turning: a timer
// that fires late means that something kept the thread busy meanwhile. The
// lines logged just before a "Stall:" line show what that was.
class StallWatch : public QObject
{
    Q_OBJECT

public:
    explicit StallWatch(QObject *parent = 0);

private slots:
    void onTick();

private:
    QTimer m_timer;
    QElapsedTimer m_sinceLastTick;
};

#endif // STALLWATCH_H
