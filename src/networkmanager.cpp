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
    if (m_session && (m_session->isOpen() || m_session->state() == QNetworkSession::Connecting))
        return;

    QNetworkConfiguration config = m_manager.defaultConfiguration();
    qDebug() << "Network: default configuration" << config.name()
             << "(" << configTypeName(config.type()) << ") bearer" << config.bearerName()
             << "valid" << config.isValid();

    if (!m_session) {
        m_session = new QNetworkSession(config, this);
        connect(m_session, SIGNAL(opened()), this, SLOT(onSessionOpened()));
        connect(m_session, SIGNAL(closed()), this, SLOT(onSessionClosed()));
        connect(m_session, SIGNAL(error(QNetworkSession::SessionError)),
                this, SLOT(onSessionError(QNetworkSession::SessionError)));
    }
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
    qDebug() << "Network: session closed";
    m_online = false;
    emit onlineChanged();
}

void NetworkManager::onSessionError(QNetworkSession::SessionError error)
{
    qWarning() << "Network: session error" << int(error) << m_session->errorString();
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
