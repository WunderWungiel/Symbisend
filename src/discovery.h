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
class PeerModel;
class NetworkManager;
class MulticastReceiver;

// LocalSend v2 discovery:
//  - multicast announcements (send) + receiving announcements (raw socket)
//  - HTTP server answering /register and /info
//  - HTTP(S) subnet scan as a fallback
class Discovery : public QObject, public HttpHandler
{
    Q_OBJECT
    Q_PROPERTY(bool scanning READ isScanning NOTIFY scanningChanged)
    Q_PROPERTY(int scanProgress READ scanProgress NOTIFY scanProgressChanged)
    Q_PROPERTY(bool serverRunning READ isServerRunning NOTIFY statusChanged)
    Q_PROPERTY(bool multicastListening READ isMulticastListening NOTIFY statusChanged)
    // While true (the Send page is on screen), refresh() runs every minute so
    // the list stays current and departed devices expire.
    Q_PROPERTY(bool autoRefresh READ autoRefresh WRITE setAutoRefresh NOTIFY autoRefreshChanged)

public:
    Discovery(DeviceInfo *device, PeerModel *peers, NetworkManager *network, QObject *parent = 0);

    bool isScanning() const { return m_scanning; }
    int scanProgress() const;
    bool isServerRunning() const;
    bool isMulticastListening() const;
    bool autoRefresh() const { return m_autoRefresh; }
    void setAutoRefresh(bool on);

    // Announce + rescan the subnet.
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void announce();
    Q_INVOKABLE void scan();

    // Our HTTP server + multicast listener, i.e. whether others can find us
    // and send to us. Stopping is not persisted: the server runs again after
    // a restart of the app.
    Q_INVOKABLE void startServer();
    Q_INVOKABLE void stopServer();
    Q_INVOKABLE void restartServer();

    // Our server, for other handlers of its routes (Receiver).
    HttpServer *httpServer() const { return m_server; }

    // HttpHandler: /register and /info
    bool handles(const HttpRequest &request) const;
    HttpResponse handleRequest(const HttpRequest &request);

signals:
    void scanningChanged();
    void scanProgressChanged();
    void statusChanged();
    void autoRefreshChanged();

private slots:
    void onOnlineChanged();
    void onNetworkAccessibleChanged(QNetworkAccessManager::NetworkAccessibility accessible);
    void onDatagram(const QByteArray &data, const QHostAddress &sender, quint16 senderPort);
    void onReplyFinished(QNetworkReply *reply);
    void onSslErrors(QNetworkReply *reply, const QList<QSslError> &errors);
    void onScanTick();
    void sendAnnouncement();
    void onAliasChanged();
    void onAutoRefreshTimer();

private:
    enum JobKind { ScanJob, AnnounceReplyJob };
    struct Job {
        JobKind kind;
        QString ip;
        int port;
        QString protocol;
        bool v1;           // peer speaks protocol v1: use /api/localsend/v1/register
        QDateTime deadline;
        bool timedOut;
    };

    void stopAll();
    void startListening();
    void stopListening();
    void abortJobs();
    static void abortReply(QNetworkReply *reply);
    void startJob(JobKind kind, const QString &ip, int port, const QString &protocol,
                  bool v1 = false);
    void pumpScanQueue();
    void finishScanIfDone();
    void replyViaUdp();
    void sendDatagram(const QByteArray &data);
    void resetUdpSocket(bool create);

    DeviceInfo *m_device;
    PeerModel *m_peers;
    NetworkManager *m_network;
    HttpServer *m_server;
    MulticastReceiver *m_multicast;
    // Sends announcements. Recreated for every connection (see onOnlineChanged()).
    QUdpSocket *m_udp;
    QNetworkAccessManager *m_nam;

    QHash<QNetworkReply *, Job> m_jobs;
    QHash<QString, QDateTime> m_recentReplies; // fingerprint -> when we last answered its announcement
    QList<QString> m_scanQueue;
    int m_scanTotal;
    int m_scanDone;
    int m_activeScanJobs;
    bool m_scanning;
    bool m_serverEnabled;  // false after stopServer() until startServer()
    QDateTime m_scanStarted;
    QDateTime m_previousScanStarted; // start of the refresh before the current one
    QTimer m_tickTimer;
    bool m_autoRefresh;
    QTimer m_autoRefreshTimer;
};

#endif // DISCOVERY_H
