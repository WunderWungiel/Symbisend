#include "discovery.h"

#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#ifndef QT_NO_OPENSSL
#include <QSslError>
#endif

#include "deviceinfo.h"
#include "json.h"
#include "multicastreceiver.h"
#include "networkmanager.h"
#include "peermodel.h"

static const char *kMulticastGroup = "224.0.0.167";
static const int kMaxConcurrentScanJobs = 24;
static const int kScanTimeoutMs = 3000;
static const int kAnnounceReplyTimeoutMs = 5000;

Discovery::Discovery(DeviceInfo *device, PeerModel *peers, NetworkManager *network, QObject *parent)
    : QObject(parent),
      m_device(device), m_peers(peers), m_network(network),
      m_server(new HttpServer(this, this)),
      m_multicast(new MulticastReceiver(this)),
      m_nam(new QNetworkAccessManager(this)),
      m_scanTotal(0), m_scanDone(0), m_activeScanJobs(0), m_scanning(false)
{
    connect(m_network, SIGNAL(onlineChanged()), this, SLOT(onOnlineChanged()));
    connect(m_multicast, SIGNAL(datagramReceived(QByteArray,QHostAddress,quint16)),
            this, SLOT(onDatagram(QByteArray,QHostAddress,quint16)));
    connect(m_nam, SIGNAL(finished(QNetworkReply*)), this, SLOT(onReplyFinished(QNetworkReply*)));
#ifndef QT_NO_OPENSSL
    connect(m_nam, SIGNAL(sslErrors(QNetworkReply*,QList<QSslError>)),
            this, SLOT(onSslErrors(QNetworkReply*,QList<QSslError>)));
#endif
    m_tickTimer.setInterval(500);
    connect(&m_tickTimer, SIGNAL(timeout()), this, SLOT(onScanTick()));
}

int Discovery::scanProgress() const
{
    return m_scanTotal > 0 ? (m_scanDone * 100) / m_scanTotal : 0;
}

bool Discovery::isServerRunning() const
{
    return m_server->isListening();
}

bool Discovery::isMulticastListening() const
{
    return m_multicast->isActive();
}

void Discovery::onOnlineChanged()
{
    if (!m_network->isOnline()) {
        m_multicast->stop();
        emit statusChanged();
        return;
    }
    m_nam->setConfiguration(m_network->configuration());
    if (!m_server->isListening())
        m_server->listen(m_device->port());
    m_multicast->start(QHostAddress(QLatin1String(kMulticastGroup)), m_device->port(),
                       m_network->localAddress());
    emit statusChanged();
    refresh();
}

void Discovery::refresh()
{
    if (!m_network->isOnline()) {
        qWarning() << "Discovery: refresh requested while offline, connecting";
        m_network->connectNetwork();
        return;
    }
    announce();
    scan();
}

// ---------------------------------------------------------------- multicast

void Discovery::announce()
{
    // LocalSend sends its announcement a few times, because UDP may be lost.
    sendAnnouncement();
    QTimer::singleShot(500, this, SLOT(sendAnnouncement()));
    QTimer::singleShot(2000, this, SLOT(sendAnnouncement()));
}

void Discovery::sendAnnouncement()
{
    QByteArray data = Json::serialize(m_device->toAnnounceMap(true));
    qint64 n = m_udp.writeDatagram(data, QHostAddress(QLatin1String(kMulticastGroup)), m_device->port());
    if (n < 0)
        qWarning() << "Discovery: announce failed:" << m_udp.errorString();
    else
        qDebug() << "Discovery: announcement sent (" << n << "bytes)";
}

void Discovery::replyViaUdp()
{
    QByteArray data = Json::serialize(m_device->toAnnounceMap(false));
    m_udp.writeDatagram(data, QHostAddress(QLatin1String(kMulticastGroup)), m_device->port());
}

void Discovery::onDatagram(const QByteArray &data, const QHostAddress &sender, quint16)
{
    bool ok = false;
    QVariantMap map = Json::parseObject(data, &ok);
    if (!ok) {
        qWarning() << "Discovery: invalid multicast datagram from" << sender.toString();
        return;
    }
    if (map.value(QLatin1String("fingerprint")).toString() == m_device->fingerprint())
        return; // our own announcement looped back

    Peer peer = PeerModel::peerFromMap(map, sender.toString(), m_device->port(),
                                       QLatin1String("https"), QLatin1String("multicast"));
    m_peers->upsert(peer);

    bool isAnnounce = map.value(QLatin1String("announce")).toBool()
            || map.value(QLatin1String("announcement")).toBool();
    if (isAnnounce) {
        qDebug() << "Discovery: announcement from" << peer.alias << peer.ip << "- replying";
        startJob(AnnounceReplyJob, peer.ip, peer.port, peer.protocol);
    }
}

// ---------------------------------------------------------------- HTTP client

void Discovery::scan()
{
    if (m_scanning) {
        qDebug() << "Discovery: scan already running";
        return;
    }
    QHostAddress local = m_network->localAddress();
    if (local.isNull()) {
        qWarning() << "Discovery: no local address, cannot scan";
        return;
    }
    const int prefix = m_network->prefixLength();
    const quint32 mask = prefix >= 32 ? 0xffffffffu : ~((1u << (32 - prefix)) - 1);
    const quint32 self = local.toIPv4Address();
    const quint32 network = self & mask;
    const quint32 broadcast = network | ~mask;

    m_scanQueue.clear();
    for (quint32 a = network + 1; a < broadcast; ++a) {
        if (a != self)
            m_scanQueue.append(QHostAddress(a).toString());
    }
    m_scanTotal = m_scanQueue.size();
    m_scanDone = 0;
    m_scanning = true;
    m_scanStarted = QDateTime::currentDateTime();
    qDebug() << "Discovery: scanning" << m_scanTotal << "hosts in"
             << QHostAddress(network).toString() + "/" + QString::number(prefix);
    emit scanningChanged();
    emit scanProgressChanged();
    m_tickTimer.start();
    pumpScanQueue();
}

void Discovery::pumpScanQueue()
{
    while (m_activeScanJobs < kMaxConcurrentScanJobs && !m_scanQueue.isEmpty())
        startJob(ScanJob, m_scanQueue.takeFirst(), m_device->port(), QLatin1String("https"));
    finishScanIfDone();
}

void Discovery::startJob(JobKind kind, const QString &ip, int port, const QString &protocol)
{
    QUrl url;
    url.setScheme(protocol);
    url.setHost(ip);
    url.setPort(port);
    url.setPath(QLatin1String("/api/localsend/v2/register"));

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QLatin1String("application/json"));
    QNetworkReply *reply = m_nam->post(request, Json::serialize(m_device->toRegisterMap()));

    Job job;
    job.kind = kind;
    job.ip = ip;
    job.port = port;
    job.protocol = protocol;
    job.deadline = QDateTime::currentDateTime().addMSecs(
                kind == ScanJob ? kScanTimeoutMs : kAnnounceReplyTimeoutMs);
    job.timedOut = false;
    m_jobs.insert(reply, job);
    if (kind == ScanJob)
        ++m_activeScanJobs;
    if (!m_tickTimer.isActive())
        m_tickTimer.start();
}

void Discovery::onSslErrors(QNetworkReply *reply, const QList<QSslError> &errors)
{
    Q_UNUSED(errors);
    // LocalSend uses self-signed certificates.
    reply->ignoreSslErrors();
}

void Discovery::onScanTick()
{
    const QDateTime now = QDateTime::currentDateTime();
    QList<QNetworkReply *> expired;
    for (QHash<QNetworkReply *, Job>::iterator it = m_jobs.begin(); it != m_jobs.end(); ++it) {
        if (!it.value().timedOut && it.value().deadline < now) {
            it.value().timedOut = true;
            expired.append(it.key());
        }
    }
    // abort() emits finished() synchronously, so do it outside the iteration.
    foreach (QNetworkReply *reply, expired)
        reply->abort();
    if (m_jobs.isEmpty() && !m_scanning)
        m_tickTimer.stop();
}

void Discovery::onReplyFinished(QNetworkReply *reply)
{
    reply->deleteLater();
    if (!m_jobs.contains(reply))
        return;
    const Job job = m_jobs.take(reply);
    if (job.kind == ScanJob)
        --m_activeScanJobs;

    const QNetworkReply::NetworkError error = reply->error();
    bool handled = false;

    if (error == QNetworkReply::NoError) {
        bool ok = false;
        QVariantMap map = Json::parseObject(reply->readAll(), &ok);
        if (ok && map.value(QLatin1String("fingerprint")).toString() != m_device->fingerprint()) {
            map.insert(QLatin1String("protocol"), job.protocol); // the one that actually worked
            map.insert(QLatin1String("port"), job.port);
            m_peers->upsert(PeerModel::peerFromMap(map, job.ip, job.port, job.protocol,
                                                   job.kind == ScanJob ? QLatin1String("scan")
                                                                       : QLatin1String("announce")));
        } else if (!ok) {
            qWarning() << "Discovery:" << job.ip << "answered with invalid JSON";
        }
        handled = true;
    } else if (job.timedOut || error == QNetworkReply::ConnectionRefusedError
               || error == QNetworkReply::HostNotFoundError) {
        handled = true; // nothing there
    } else if (job.protocol == QLatin1String("https")) {
        // Probably a LocalSend instance with encryption disabled: retry over HTTP.
        qDebug() << "Discovery:" << job.ip << "https failed (" << int(error) << reply->errorString()
                 << ") - trying http";
        startJob(job.kind, job.ip, job.port, QLatin1String("http"));
        return;
    } else {
        qDebug() << "Discovery:" << job.ip << job.protocol << "failed:" << int(error) << reply->errorString();
        handled = true;
    }

    if (handled && job.kind == AnnounceReplyJob && error != QNetworkReply::NoError) {
        qDebug() << "Discovery: HTTP reply to" << job.ip << "failed, replying via UDP";
        replyViaUdp();
    }

    if (job.kind == ScanJob) {
        ++m_scanDone;
        emit scanProgressChanged();
        pumpScanQueue();
    }
}

void Discovery::finishScanIfDone()
{
    if (!m_scanning || !m_scanQueue.isEmpty() || m_activeScanJobs > 0)
        return;
    m_scanning = false;
    qDebug() << "Discovery: scan finished," << m_peers->rowCount() << "device(s) known";
    // Drop devices not seen at all during this refresh (scan, register or multicast).
    m_peers->removeOlderThan(m_scanStarted);
    emit scanningChanged();
}

// ---------------------------------------------------------------- HTTP server

HttpResponse Discovery::handleRequest(const HttpRequest &request)
{
    HttpResponse response;
    const QString path = request.path;
    const bool isRegister = path == QLatin1String("/api/localsend/v2/register")
            || path == QLatin1String("/api/localsend/v1/register");
    const bool isInfo = path == QLatin1String("/api/localsend/v2/info")
            || path == QLatin1String("/api/localsend/v1/info");

    if (isRegister && request.method == QLatin1String("POST")) {
        bool ok = false;
        QVariantMap map = Json::parseObject(request.body, &ok);
        if (!ok) {
            response.status = 400;
            response.body = "{\"message\":\"Invalid body\"}";
            return response;
        }
        if (map.value(QLatin1String("fingerprint")).toString() != m_device->fingerprint()) {
            m_peers->upsert(PeerModel::peerFromMap(map, request.remoteAddress.toString(),
                                                   m_device->port(), QLatin1String("https"),
                                                   QLatin1String("register")));
        }
        response.body = Json::serialize(m_device->toRegisterMap());
        return response;
    }
    if (isInfo && request.method == QLatin1String("GET")) {
        response.body = Json::serialize(m_device->toRegisterMap());
        return response;
    }

    response.status = 404;
    response.body = "{\"message\":\"Not found\"}";
    return response;
}
