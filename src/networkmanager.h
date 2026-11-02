#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <QObject>
#include <QHostAddress>
#include <QNetworkConfigurationManager>
#include <QNetworkSession>

// Opens the network connection (Symbian access point) and reports our local
// IPv4 address and subnet.
class NetworkManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool online READ isOnline NOTIFY onlineChanged)
    Q_PROPERTY(QString localAddress READ localAddressString NOTIFY onlineChanged)
    Q_PROPERTY(QString connectionName READ connectionName NOTIFY onlineChanged)

public:
    explicit NetworkManager(QObject *parent = 0);

    bool isOnline() const { return m_online; }
    QHostAddress localAddress() const { return m_localAddress; }
    QString localAddressString() const { return m_localAddress.toString(); }
    int prefixLength() const { return m_prefixLength; }
    QString connectionName() const;
    QNetworkConfiguration configuration() const;

    Q_INVOKABLE void connectNetwork();

signals:
    void onlineChanged();

private slots:
    void onSessionOpened();
    void onSessionClosed();
    void onSessionError(QNetworkSession::SessionError error);
    void onSessionStateChanged(QNetworkSession::State state);

private:
    void goOffline(const char *reason);
    void detectLocalAddress();
    void setDefaultInterfaceForOpenC();
    void resetDefaultInterfaceForOpenC();

    QNetworkConfigurationManager m_manager;
    QNetworkSession *m_session;
    bool m_online;
    QHostAddress m_localAddress;
    int m_prefixLength;
};

#endif // NETWORKMANAGER_H
