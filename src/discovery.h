#ifndef DISCOVERY_H
#define DISCOVERY_H

#include <QObject>
#include <QDateTime>
#include <QHash>
#include <QHostAddress>
#include <QList>
#include <QTimer>
#include <QUdpSocket>

#include "httpserver.h"

class QNetworkAccessManager;
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

public:
    Discovery(DeviceInfo *device, PeerModel *peers, NetworkManager *network, QObject *parent = 0);

    bool isScanning() const { return m_scanning; }
    int scanProgress() const;
    bool isServerRunning() const;
    bool isMulticastListening() const;

    // Announce + rescan the subnet.
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void announce();
    Q_INVOKABLE void scan();

    // HttpHandler
    HttpResponse handleRequest(const HttpRequest &request);

signals:
    void scanningChanged();
    void scanProgressChanged();
    void statusChanged();

private slots:
    void onOnlineChanged();
    void onDatagram(const QByteArray &data, const QHostAddress &sender, quint16 senderPort);
    void onReplyFinished(QNetworkReply *reply);
    void onSslErrors(QNetworkReply *reply, const QList<QSslError> &errors);
    void onScanTick();
    void sendAnnouncement();

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
    void startJob(JobKind kind, const QString &ip, int port, const QString &protocol,
                  bool v1 = false);
    void pumpScanQueue();
    void finishScanIfDone();
    void replyViaUdp();

    DeviceInfo *m_device;
    PeerModel *m_peers;
    NetworkManager *m_network;
    HttpServer *m_server;
    MulticastReceiver *m_multicast;
    QUdpSocket m_udp;
    QNetworkAccessManager *m_nam;

    QHash<QNetworkReply *, Job> m_jobs;
    QList<QString> m_scanQueue;
    int m_scanTotal;
    int m_scanDone;
    int m_activeScanJobs;
    bool m_scanning;
    QDateTime m_scanStarted;
    QTimer m_tickTimer;
};

#endif // DISCOVERY_H
