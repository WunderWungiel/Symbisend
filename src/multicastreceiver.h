// SPDX-FileCopyrightText: 2026 Wunder Wungiel
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MULTICASTRECEIVER_H
#define MULTICASTRECEIVER_H

#include <QObject>
#include <QHostAddress>
#include <QTimer>

class QSocketNotifier;

// Receives UDP multicast datagrams.
//
// Qt 4.7 has no QUdpSocket::joinMulticastGroup(), and on Qt 4.8 for Symbian the
// QUdpSocket descriptor is not an Open C file descriptor, so we use our own
// BSD socket (Open C on Symbian, POSIX on Harmattan). A QSocketNotifier tells
// us when a datagram is waiting (on Symbian, Qt's event dispatcher watches
// Open C descriptors with select() in its own thread), so nothing runs while
// the network is quiet. A slow fallback poll covers a notifier that never
// fires; it stops once the notifier has.
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
    // Reading kept failing, so the receiver stopped itself. start() again.
    void failed();

private slots:
    void onActivated();
    void onFallbackPoll();
    void resumeNotifier();

private:
    // Reads what is waiting; false if reading should pause for a while
    // (empty reads or errors, e.g. while the link goes down).
    bool readPending();
    void pauseNotifier();
    void readFailed();

    int m_fd;
    QSocketNotifier *m_notifier;
    bool m_notifierFired;     // the notifier works: the fallback poll is off
    bool m_badReadLogged;     // empty reads (link going down) are logged once per start()
    bool m_oddAddressLogged;  // unusable sender addresses are logged once per start()
    bool m_selectErrorLogged; // select() failures are logged once per start()
    bool m_fallbackLogged;    // a datagram found by the fallback poll, logged once per start()
    int m_readFailures;       // since the last datagram
    QTimer m_fallbackTimer;
    QTimer m_resumeTimer;     // re-enables the notifier after a pause
};

#endif // MULTICASTRECEIVER_H
