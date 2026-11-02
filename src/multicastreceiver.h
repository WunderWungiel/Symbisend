#ifndef MULTICASTRECEIVER_H
#define MULTICASTRECEIVER_H

#include <QObject>
#include <QHostAddress>
#include <QTimer>

// Receives UDP multicast datagrams.
//
// Qt 4.7 has no QUdpSocket::joinMulticastGroup(), and on Qt 4.8 for Symbian the
// QUdpSocket descriptor is not an Open C file descriptor, so we use our own
// BSD socket (Open C on Symbian, POSIX on Harmattan) and poll it with a timer.
class MulticastReceiver : public QObject
{
    Q_OBJECT
public:
    explicit MulticastReceiver(QObject *parent = 0);
    ~MulticastReceiver();

    // Returns true if bound and the group was joined.
    bool start(const QHostAddress &group, quint16 port, const QHostAddress &localInterface);
    void stop();
    bool isActive() const { return m_fd >= 0; }

signals:
    void datagramReceived(const QByteArray &data, const QHostAddress &sender, quint16 senderPort);

private slots:
    void poll();

private:
    int m_fd;
    bool m_badReadLogged; // bogus reads (link going down) are logged once per start()
    QTimer m_pollTimer;
};

#endif // MULTICASTRECEIVER_H
