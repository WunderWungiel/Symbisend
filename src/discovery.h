#ifndef DISCOVERY_H
#define DISCOVERY_H

#include <QObject>
#include <QDateTime>
#include <QHash>
#include <QHostAddress>
#include <QList>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QUdpSocket>

#include "httpserver.h"

class QNetworkReply;
class QSslError;
class DeviceInfo;
class FavoritesModel;
class PeerModel;
class NetworkManager;
class MulticastReceiver;
class Settings;

// LocalSend v2 discovery:
//  - multicast announcements (send) + receiving announcements (raw socket)
//  - HTTP server answering /register and /info
//  - discover(): announcing and probing the favourites, then an HTTP(S) subnet
//    scan only if neither confirmed a device
class Discovery : public QObject, public HttpHandler
{
    Q_OBJECT
    // A discover(), refresh() or scan() is running, in any of its stages.
    Q_PROPERTY(bool scanning READ isScanning NOTIFY scanningChanged)
    Q_PROPERTY(int scanProgress READ scanProgress NOTIFY scanProgressChanged)
    Q_PROPERTY(bool serverRunning READ isServerRunning NOTIFY statusChanged)
    // Whether the running server speaks https (see settings.encryption,
    // webShare.encrypted).
    Q_PROPERTY(bool serverHttps READ isServerHttps NOTIFY statusChanged)
    Q_PROPERTY(bool multicastListening READ isMulticastListening NOTIFY statusChanged)

public:
    Discovery(DeviceInfo *device, PeerModel *peers, FavoritesModel *favorites,
              NetworkManager *network, Settings *settings, QObject *parent = 0);

    bool isScanning() const { return m_scanning; }
    int scanProgress() const;
    bool isServerRunning() const;
    bool isServerHttps() const;
    bool isMulticastListening() const;

    // Looks for devices in stages, cheapest first, keeping the list. Does
    // nothing while offline or while already running. Never connects: it must
    // not pop up the connection dialog by itself.
    Q_INVOKABLE void discover();
    // The refresh button: connects if offline, clears the list, discover().
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void announce();
    Q_INVOKABLE void scan();

    // Asks the device at this address who it is (POST /register, like the
    // official app's "Enter address"; https, then http), which also adds it
    // to the peer list. Ends with lookUpFinished(). Port 0 = the default port.
    Q_INVOKABLE void lookUp(const QString &ip, int port = 0);

    // Our HTTP server + multicast listener, i.e. whether others can find us
    // and send to us. Stopping is not persisted: the server runs again after
    // a restart of the app.
    Q_INVOKABLE void startServer();
    Q_INVOKABLE void stopServer();
    Q_INVOKABLE void restartServer();

    // Our server, for other handlers of its routes (Receiver).
    HttpServer *httpServer() const { return m_server; }

    // WebShare: while "Receive via link" is active, the server speaks https
    // only if encrypted (instead of following settings.encryption). A running
    // server is restarted at once if that changes what it speaks.
    void setLinkEncryption(bool linkActive, bool encrypted);

    // HttpHandler: /register and /info
    bool handles(const HttpRequest &request) const;
    HttpResponse handleRequest(const HttpRequest &request);

signals:
    void scanningChanged();
    void scanProgressChanged();
    void statusChanged();
    // After lookUp(): the device's fingerprint, or what went wrong in error.
    void lookUpFinished(const QString &ip, const QString &fingerprint, const QString &error);

private slots:
    void onOnlineChanged();
    void onNetworkAccessibleChanged(QNetworkAccessManager::NetworkAccessibility accessible);
    void onDatagram(const QByteArray &data, const QHostAddress &sender, quint16 senderPort);
    void onReplyFinished(QNetworkReply *reply);
    void onSslErrors(QNetworkReply *reply, const QList<QSslError> &errors);
    void onScanTick();
    void sendAnnouncement();
    void onAliasChanged();
    void onPeerConfirmed();
    void onGraceTimer();
    void finishScanIfDone();
    void applyEncryption();

private:
    enum JobKind { ScanJob, FavoriteJob, AnnounceReplyJob, LookUpJob };
    struct Job {
        JobKind kind;
        QString ip;
        int port;
        QString protocol;
        bool v1;           // peer speaks protocol v1: use /api/localsend/v1/register
        QDateTime deadline;
        bool timedOut;
        // https: the fingerprint the peer's certificate must have (the one it
        // multicast); empty = accept any valid one (subnet scan, lookUp()).
        QString expectedFingerprint;
        bool udpFallback;  // reply via UDP if the register fails (announcements)
        bool certRejected; // its certificate was refused
        // AnnounceReplyJob: the multicast's fingerprint (m_recentReplies key).
        QString announcer;
    };

    bool wantHttps() const;
    void stopAll();
    void startListening();
    void stopListening();
    void abortJobs();
    static void abortReply(QNetworkReply *reply);
    void startJob(JobKind kind, const QString &ip, int port, const QString &protocol,
                  bool v1 = false, const QString &expectedFingerprint = QString(),
                  bool udpFallback = false, const QString &announcer = QString());
    void scanSubnet();
    void pumpScanQueue();
    void replyViaUdp();
    void sendDatagram(const QByteArray &data);
    void resetUdpSocket(bool create);

    DeviceInfo *m_device;
    PeerModel *m_peers;
    FavoritesModel *m_favorites;
    NetworkManager *m_network;
    Settings *m_settings;
    HttpServer *m_server;
    MulticastReceiver *m_multicast;
    // Sends announcements. Recreated for every connection (see onOnlineChanged()).
    QUdpSocket *m_udp;
    QNetworkAccessManager *m_nam;

    QHash<QNetworkReply *, Job> m_jobs;
    // fingerprint -> when we started answering its multicast; a failed answer
    // is removed, so the next message of the burst tries again.
    QHash<QString, QDateTime> m_recentReplies;
    QList<QString> m_scanQueue;
    int m_scanTotal;
    int m_scanDone;
    int m_activeScanJobs;
    int m_activeFavoriteJobs;
    bool m_scanning;
    bool m_confirmed;      // a device was confirmed since discover() started
    bool m_serverEnabled;  // false after stopServer() until startServer()
    bool m_linkActive;     // see setLinkEncryption()
    bool m_linkEncrypted;
    QTimer m_tickTimer;
    QTimer m_graceTimer;   // after the favourites: does anything still confirm?
    QTimer m_burstTimer;   // discover() lasts at least as long as its announce()
};

#endif // DISCOVERY_H
