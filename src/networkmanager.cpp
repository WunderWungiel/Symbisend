#include "networkmanager.h"

#include <QDebug>
#include <QNetworkInterface>

#ifdef Q_OS_SYMBIAN
#include <sys/socket.h>
#include <net/if.h>
#include <string.h>
#endif

NetworkManager::NetworkManager(QObject *parent)
    : QObject(parent), m_session(0), m_online(false), m_prefixLength(24)
{
}

QString NetworkManager::connectionName() const
{
    return configuration().name();
}

// The access point (IAP) actually in use. The session itself is often opened
// on a service network (SNAP, e.g. "Internet") or a user-choice configuration;
// those only name a group of access points, see the Qt bearermonitor example.
QNetworkConfiguration NetworkManager::configuration() const
{
    if (!m_session)
        return QNetworkConfiguration();
    QNetworkConfiguration config = m_session->configuration();
    if (config.type() == QNetworkConfiguration::InternetAccessPoint)
        return config;
    const QString id = m_session->sessionProperty(QLatin1String("ActiveConfiguration")).toString();
    QNetworkConfiguration active = m_manager.configurationFromIdentifier(id);
    return active.isValid() ? active : config;
}

static const char *configTypeName(QNetworkConfiguration::Type type)
{
    switch (type) {
    case QNetworkConfiguration::InternetAccessPoint: return "access point";
    case QNetworkConfiguration::ServiceNetwork: return "service network";
    case QNetworkConfiguration::UserChoice: return "user choice";
    default: return "invalid";
    }
}

void NetworkManager::connectNetwork()
{
    if (m_session) {
        const QNetworkSession::State state = m_session->state();
        if (state == QNetworkSession::Connecting)
            return;
        if (m_session->isOpen() && state == QNetworkSession::Connected) {
            if (m_online)
                return;
            // Opened, but no IPv4 address was found then (e.g. DHCP not
            // done yet). The session is fine, so look again instead of
            // returning here forever.
            qDebug() << "Network: session open but offline, detecting the address again";
            onSessionOpened();
            return;
        }
        // A session that lost its connection (out of WLAN range, AP switched
        // off) is not reused: start over with the current default
        // configuration, which the user may have changed meanwhile.
        qDebug() << "Network: dropping old session, state" << int(state);
        m_session->disconnect(this);
        m_session->deleteLater();
        m_session = 0;
    }

    QNetworkConfiguration config = m_manager.defaultConfiguration();
    qDebug() << "Network: default configuration" << config.name()
             << "(" << configTypeName(config.type()) << ") bearer" << config.bearerName()
             << "valid" << config.isValid();

    m_session = new QNetworkSession(config, this);
    connect(m_session, SIGNAL(opened()), this, SLOT(onSessionOpened()));
    connect(m_session, SIGNAL(closed()), this, SLOT(onSessionClosed()));
    connect(m_session, SIGNAL(error(QNetworkSession::SessionError)),
            this, SLOT(onSessionError(QNetworkSession::SessionError)));
    // Losing the link is reported via stateChanged/error(SessionAbortedError),
    // not necessarily via closed(). Roaming signals are deliberately not
    // connected, so Qt does not migrate the session to another access point.
    connect(m_session, SIGNAL(stateChanged(QNetworkSession::State)),
            this, SLOT(onSessionStateChanged(QNetworkSession::State)));
    qDebug() << "Network: opening session...";
    m_session->open();
}

void NetworkManager::onSessionOpened()
{
    const QNetworkConfiguration active = configuration();
    qDebug() << "Network: session opened on" << m_session->configuration().name()
             << "(" << configTypeName(m_session->configuration().type()) << "), access point"
             << active.name() << "(" << active.bearerName() << ")";
    setDefaultInterfaceForOpenC();
    detectLocalAddress();
    m_online = !m_localAddress.isNull();
    if (!m_online)
        qWarning() << "Network: session open but no IPv4 address found";
    emit onlineChanged();
}

void NetworkManager::onSessionClosed()
{
    goOffline("session closed");
}

void NetworkManager::onSessionError(QNetworkSession::SessionError error)
{
    qWarning() << "Network: session error" << int(error) << m_session->errorString();
    if (error == QNetworkSession::SessionAbortedError)
        goOffline("session aborted (connection lost)");
}

void NetworkManager::onSessionStateChanged(QNetworkSession::State state)
{
    qDebug() << "Network: session state" << int(state);
    if (state == QNetworkSession::Disconnected || state == QNetworkSession::NotAvailable)
        goOffline("connection lost");
}

// Going offline is reported once, however many of closed()/error()/
// stateChanged() announce it. Reconnecting is left to the user (refresh),
// like Qt's own Symbian examples do, so we never pop up connection dialogs
// in a loop.
void NetworkManager::goOffline(const char *reason)
{
    if (!m_online)
        return;
    qWarning() << "Network: offline -" << reason;
    m_online = false;
    m_localAddress = QHostAddress();
    resetDefaultInterfaceForOpenC();
    emit onlineChanged();
}

void NetworkManager::detectLocalAddress()
{
    m_localAddress = QHostAddress();
    m_prefixLength = 24;

    QList<QNetworkAddressEntry> entries;
    QNetworkInterface iface = m_session->interface();
    if (iface.isValid()) {
        qDebug() << "Network: session interface" << iface.name();
        entries = iface.addressEntries();
    }
    if (entries.isEmpty()) {
        foreach (const QNetworkInterface &i, QNetworkInterface::allInterfaces()) {
            if (!(i.flags() & QNetworkInterface::IsUp) || (i.flags() & QNetworkInterface::IsLoopBack))
                continue;
            entries += i.addressEntries();
        }
    }

    foreach (const QNetworkAddressEntry &e, entries) {
        const QHostAddress a = e.ip();
        if (a.protocol() != QAbstractSocket::IPv4Protocol || a == QHostAddress::LocalHost)
            continue;
        m_localAddress = a;
        if (e.prefixLength() > 0 && e.prefixLength() <= 32)
            m_prefixLength = e.prefixLength();
        break;
    }
    // Never scan more than a /24, whatever the netmask says.
    if (m_prefixLength < 24)
        m_prefixLength = 24;
    qDebug() << "Network: local address" << m_localAddress.toString() << "/" << m_prefixLength;
}

void NetworkManager::setDefaultInterfaceForOpenC()
{
#ifdef Q_OS_SYMBIAN
    // The raw multicast socket uses Open C directly; bind Open C to the same
    // access point so it does not prompt or pick another connection.
    // setdefaultif() wants the access point (IAP) name, not a SNAP name.
    QByteArray name = configuration().name().toUtf8();
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, name.constData(), sizeof(ifr.ifr_name) - 1);
    int rc = setdefaultif(&ifr);
    qDebug() << "Network: Open C setdefaultif(" << name << ") ->" << rc;
#endif
}

void NetworkManager::resetDefaultInterfaceForOpenC()
{
#ifdef Q_OS_SYMBIAN
    int rc = setdefaultif(0);
    qDebug() << "Network: Open C setdefaultif(NULL) ->" << rc;
#endif
}
