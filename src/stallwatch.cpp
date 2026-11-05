#include "stallwatch.h"

#include <QDebug>

static const int kTickMs = 100;
// How late a tick must be to count: below that the eye barely notices.
static const int kStallMs = 300;

StallWatch::StallWatch(QObject *parent)
    : QObject(parent)
{
    m_timer.setInterval(kTickMs);
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(onTick()));
    m_sinceLastTick.start();
    m_timer.start();
}

void StallWatch::onTick()
{
    const qint64 late = m_sinceLastTick.restart() - kTickMs;
    if (late >= kStallMs)
        qWarning() << "Stall: the UI thread was busy for" << late << "ms";
}
