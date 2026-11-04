#include "discovery.h"

#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStringList>
#include <QUrl>
#ifndef QT_NO_OPENSSL
#include <QSslError>
#endif

#include "clientidentity.h"
#include "deviceinfo.h"
#include "json.h"
#include "multicastreceiver.h"
#include "networkmanager.h"
#include "peermodel.h"
#include "settings.h"

static const char *kMulticastGroup = "224.0.0.167";
static const int kMaxConcurrentScanJobs = 24;
static const int kScanTimeoutMs = 3000;
static const int kAnnounceReplyTimeoutMs = 5000;
// lookUp(): the user is waiting, but a phone needs time for the handshake.
static const int kLookUpTimeoutMs = 8000;
// Refresh while the device list is on screen (autoRefresh). A device is
// dropped after missing two refreshes, i.e. within about two minutes.
static const int kAutoRefreshIntervalMs = 60000;
static const int kAutoRefreshMinGapMs = 15000;
// How long past its deadline a request may take to report finished().
static const int kStuckJobGraceMs = 5000;
// An announcement is sent as a burst of 3 (see announce()); answer it once.
static const int kAnnounceDedupWindowMs = 5000;

Discovery::Discovery(DeviceInfo *device, PeerModel *peers, NetworkManager *network,
                     Settings *settings, QObject *parent)
    : QObject(parent),
      m_device(device), m_peers(peers), m_network(network), m_settings(settings),
      m_server(new HttpServer(this)),
      m_multicast(new MulticastReceiver(this)),
      m_udp(0),
      m_nam(new QNetworkAccessManager(this)),
      m_scanTotal(0), m_scanDone(0), m_activeScanJobs(0), m_scanning(false), m_serverEnabled(true),
      m_linkActive(false), m_linkEncrypted(false),
      m_autoRefresh(false)
{
    m_device->setHttps(wantHttps()); // until the server actually starts
    connect(m_settings, SIGNAL(encryptionChanged()), this, SLOT(applyEncryption()));
    m_server->addHandler(this);
    connect(m_device, SIGNAL(aliasChanged()), this, SLOT(onAliasChanged()));
    connect(m_network, SIGNAL(onlineChanged()), this, SLOT(onOnlineChanged()));
    connect(m_multicast, SIGNAL(datagramReceived(QByteArray,QHostAddress,quint16)),
            this, SLOT(onDatagram(QByteArray,QHostAddress,quint16)));
    connect(m_nam, SIGNAL(finished(QNetworkReply*)), this, SLOT(onReplyFinished(QNetworkReply*)));
    connect(m_nam, SIGNAL(networkAccessibleChanged(QNetworkAccessManager::NetworkAccessibility)),
            this, SLOT(onNetworkAccessibleChanged(QNetworkAccessManager::NetworkAccessibility)));
#ifndef QT_NO_OPENSSL
    connect(m_nam, SIGNAL(sslErrors(QNetworkReply*,QList<QSslError>)),
            this, SLOT(onSslErrors(QNetworkReply*,QList<QSslError>)));
#endif
    m_tickTimer.setInterval(500);
    connect(&m_tickTimer, SIGNAL(timeout()), this, SLOT(onScanTick()));
    m_autoRefreshTimer.setInterval(kAutoRefreshIntervalMs);
    connect(&m_autoRefreshTimer, SIGNAL(timeout()), this, SLOT(onAutoRefreshTimer()));
}

void Discovery::setAutoRefresh(bool on)
{
    if (on == m_autoRefresh)
        return;
    m_autoRefresh = on;
    qDebug() << "Discovery: auto refresh" << (on ? "on" : "off");
    if (on) {
        m_autoRefreshTimer.start();
        // Coming back to the list after a while: refresh right away, unless
        // the last refresh was just now (e.g. switching tabs back and forth).
        if (!m_scanStarted.isValid()
                || m_scanStarted.msecsTo(QDateTime::currentDateTime()) >= kAutoRefreshMinGapMs)
            onAutoRefreshTimer();
    } else {
        m_autoRefreshTimer.stop();
    }
    emit autoRefreshChanged();
}

void Discovery::onAutoRefreshTimer()
{
    // Never connect from here: an automatic refresh must not pop up the
    // connection dialog. The user's refresh button does that.
    if (!m_network->isOnline() || m_scanning)
        return;
    refresh();
}

int Discovery::scanProgress() const
{
    return m_scanTotal > 0 ? (m_scanDone * 100) / m_scanTotal : 0;
}

bool Discovery::isServerRunning() const
{
    return m_server->isListening();
}

bool Discovery::isServerHttps() const
{
    return m_server->isListening() && m_server->isHttps();
}

bool Discovery::wantHttps() const
{
    // HttpServer falls back to http if it can't do https (no certificate).
    return m_linkActive ? m_linkEncrypted : m_settings->encryption();
}

void Discovery::setLinkEncryption(bool linkActive, bool encrypted)
{
    if (linkActive == m_linkActive && encrypted == m_linkEncrypted)
        return;
    m_linkActive = linkActive;
    m_linkEncrypted = encrypted;
    applyEncryption();
}

// Restarts a running server whose protocol no longer matches the settings
// (like the official app, which restarts its server for web share
// encryption). Requests in progress are aborted; peers learn the new
// protocol from the announcement. A stopped server uses it on its next start.
void Discovery::applyEncryption()
{
    const bool https = wantHttps();
    qDebug() << "Discovery: encryption" << (https ? "on" : "off")
             << (m_linkActive ? "(receive via link)" : "(settings)");
    if (!m_server->isListening()) {
        m_device->setHttps(https);
        return;
    }
    // isHttps() differs from https also when https is impossible: restarting
    // then only tries again (and logs why it can't).
    // The client certificate is optional only while browsers connect.
    if (m_server->isHttps() == https && (!https || m_server->clientCertRequired() == !m_linkActive))
        return;
    qDebug() << "Discovery: restarting the server as" << (https ? "https" : "http");
    startListening();
    if (isServerRunning())
        announce();
}

bool Discovery::isMulticastListening() const
{
    return m_multicast->isActive();
}

void Discovery::onOnlineChanged()
{
    if (!m_network->isOnline()) {
        qDebug() << "Discovery: offline, stopping";
        stopAll();
        // Nothing is reachable any more, and the next network may be another one.
        m_peers->clear();
        emit statusChanged();
        return;
    }
    // (Re)bind everything to the new connection: sockets created on the old
    // one are dead on Symbian even if they still report being open.
    m_nam->setConfiguration(m_network->configuration());
    m_nam->setNetworkAccessible(QNetworkAccessManager::Accessible);
    resetUdpSocket(true);
    if (m_serverEnabled)
        startListening();
    else
        qDebug() << "Discovery: online, but the server was stopped by the user";
    refresh();
}

// (Re)starts the HTTP server and the multicast listener.
void Discovery::startListening()
{
    m_server->close();
    const bool https = wantHttps();
    if (m_server->listen(m_device->port(), https, !m_linkActive))
        m_device->setHttps(m_server->isHttps());
    else
        m_device->setHttps(https);
    m_multicast->start(QHostAddress(QLatin1String(kMulticastGroup)), m_device->port(),
                       m_network->localAddress());
    emit statusChanged();
}

void Discovery::stopListening()
{
    m_multicast->stop();
    m_server->close();
    m_recentReplies.clear();
    emit statusChanged();
}

void Discovery::startServer()
{
    qDebug() << "Discovery: server (re)start requested";
    m_serverEnabled = true;
    if (!m_network->isOnline()) {
        // onOnlineChanged() starts it once connected.
        m_network->connectNetwork();
        return;
    }
    startListening();
    if (isServerRunning())
        announce();
}

void Discovery::stopServer()
{
    qDebug() << "Discovery: server stop requested";
    m_serverEnabled = false;
    stopListening();
}

void Discovery::restartServer()
{
    startServer();
}

// Let the others pick up our new name right away.
void Discovery::onAliasChanged()
{
    if (m_network->isOnline() && isServerRunning())
        announce();
}

// The access manager follows its own session to our access point and is
// usually the first to notice the link going down. Drop the pending requests
// right away: Qt 4.8 fails a request that once waited for that session both
// with a session error and with the socket error, which it reports as
// "QNetworkReplyImplPrivate::error: Internal problem, this method must only
// be called once" - once per request.
void Discovery::onNetworkAccessibleChanged(QNetworkAccessManager::NetworkAccessibility accessible)
{
    qDebug() << "Discovery: network accessible" << int(accessible)
             << "(" << m_jobs.size() << "request(s) pending)";
    if (accessible == QNetworkAccessManager::NotAccessible && !m_jobs.isEmpty()) {
        qWarning() << "Discovery: network not accessible - stopping scan";
        abortJobs();
    }
}

void Discovery::stopAll()
{
    // Requests made while offline fail at once instead of making the access
    // manager reopen its session (and possibly show a connection dialog).
    m_nam->setNetworkAccessible(QNetworkAccessManager::NotAccessible);
    stopListening();
    abortJobs();
    resetUdpSocket(false);
    m_tickTimer.stop();
    m_scanStarted = QDateTime();
    m_previousScanStarted = QDateTime();
}

// Drops all pending HTTP jobs and ends a running scan without completing it.
void Discovery::abortJobs()
{
    // abort() emits finished() synchronously; with m_jobs already cleared
    // onReplyFinished() only deletes the replies (no retries, no UDP replies).
    // A reply that can't be aborted (it already failed) is deleted here, or
    // it would leak with its connection if it never reports finished();
    // deleteLater() twice is harmless if it does.
    QList<QNetworkReply *> replies = m_jobs.keys();
    QStringList lookUps;
    foreach (const Job &job, m_jobs) {
        if (job.kind == LookUpJob)
            lookUps.append(job.ip);
    }
    m_jobs.clear();
    foreach (QNetworkReply *reply, replies) {
        abortReply(reply);
        reply->deleteLater();
    }

    m_scanQueue.clear();
    m_activeScanJobs = 0;
    if (m_scanning) {
        m_scanning = false;
        emit scanningChanged();
    }
    foreach (const QString &ip, lookUps)
        emit lookUpFinished(ip, QString(), QLatin1String("Connection lost"));
}

// A reply that already failed is about to emit finished() by itself; aborting
// it would make Qt set a second error ("QNetworkReplyImplPrivate::error:
// Internal problem, this method must only be called once").
void Discovery::abortReply(QNetworkReply *reply)
{
    if (reply->error() == QNetworkReply::NoError && !reply->isFinished())
        reply->abort();
}

void Discovery::refresh()
{
    if (!m_network->isOnline()) {
        qWarning() << "Discovery: refresh requested while offline, connecting";
        m_network->connectNetwork();
        return;
    }
    // Without our server nobody could answer an announcement.
    if (isServerRunning())
        announce();
    scan();
}

// ---------------------------------------------------------------- multicast

void Discovery::announce()
{
    // Same burst as the official app (delays of 100, 500 and 2000 ms before
    // each message): UDP may be lost and peers may not be ready to answer yet.
    QTimer::singleShot(100, this, SLOT(sendAnnouncement()));
    QTimer::singleShot(100 + 500, this, SLOT(sendAnnouncement()));
    QTimer::singleShot(100 + 500 + 2000, this, SLOT(sendAnnouncement()));
}

void Discovery::sendAnnouncement()
{
    // The burst's timers outlive stopServer() and going offline: don't
    // announce a server that isn't there, or write to a dead connection.
    if (!m_network->isOnline() || !isServerRunning()) {
        qDebug() << "Discovery: announcement skipped (offline or server stopped)";
        return;
    }
    sendDatagram(Json::serialize(m_device->toAnnounceMap(true)));
}

void Discovery::replyViaUdp()
{
    if (!m_network->isOnline())
        return;
    sendDatagram(Json::serialize(m_device->toAnnounceMap(false)));
}

void Discovery::sendDatagram(const QByteArray &data)
{
    if (!m_udp)
        resetUdpSocket(true);
    qint64 n = m_udp->writeDatagram(data, QHostAddress(QLatin1String(kMulticastGroup)), m_device->port());
    if (n < 0)
        qWarning() << "Discovery: multicast send failed:" << m_udp->errorString();
    else
        qDebug() << "Discovery: multicast message sent (" << n << "bytes)";
}

// The sending socket's engine is created on the first writeDatagram() and,
// on Symbian, tied to the connection that was up then. close() does not
// drop that engine for a socket that was never bound (it stays in
// UnconnectedState), so a socket from an earlier connection keeps writing
// into the dead one. Replace the whole object instead.
void Discovery::resetUdpSocket(bool create)
{
    if (m_udp) {
        m_udp->deleteLater();
        m_udp = 0;
    }
    if (create)
        m_udp = new QUdpSocket(this);
}

void Discovery::onDatagram(const QByteArray &data, const QHostAddress &sender, quint16)
{
    bool ok = false;
    QVariantMap map = Json::parseObject(data, &ok);
    if (!ok) {
        qWarning() << "Discovery: invalid multicast datagram from" << sender.toString();
        return;
    }
    const QString fingerprint = map.value(QLatin1String("fingerprint")).toString();
    if (fingerprint == m_device->fingerprint())
        return; // our own announcement looped back

    // The announcement carries no IP. If the socket couldn't tell us the
    // sender, use the address we already know for this device, if any.
    QString ip = sender.isNull() ? QString() : sender.toString();
    if (ip.isEmpty()) {
        Peer known;
        if (m_peers->find(fingerprint, &known))
            ip = known.ip;
    }

    // Not stored yet: like the official app, a device that multicasts is
    // stored only once our register request to it has succeeded - over https
    // pinned to the fingerprint it multicast, so a device can't pose as
    // another one by copying its fingerprint.
    const Peer peer = PeerModel::peerFromMap(map, ip, m_device->port(),
                                             QLatin1String("https"), QLatin1String("multicast"));

    const bool isAnnounce = map.value(QLatin1String("announce")).toBool()
            || map.value(QLatin1String("announcement")).toBool();
    const QDateTime now = QDateTime::currentDateTime();
    // Forget old entries, then skip the repeated messages of one burst.
    QMutableHashIterator<QString, QDateTime> it(m_recentReplies);
    while (it.hasNext()) {
        if (it.next().value().msecsTo(now) >= kAnnounceDedupWindowMs)
            it.remove();
    }
    if (m_recentReplies.contains(peer.fingerprint)) {
        qDebug() << "Discovery: repeated multicast from" << peer.alias << "- already answered";
        return;
    }
    m_recentReplies.insert(peer.fingerprint, now);

    if (ip.isEmpty()) {
        if (isAnnounce) {
            // The spec's fallback: answer by multicast; the peer registers
            // us from the address it sees.
            qDebug() << "Discovery: announcement from" << peer.alias
                     << "with unknown address - replying via multicast";
            replyViaUdp();
        }
        return;
    }
    const bool v1 = PeerModel::isV1(peer.version);
    const bool https = peer.protocol == QLatin1String("https");
    qDebug() << "Discovery:" << (isAnnounce ? "announcement" : "multicast answer") << "from"
             << peer.alias << peer.ip << "v" + peer.version << "- registering";
    // A UDP answer (not an announcement) is checked the same way, but a
    // failure isn't answered by multicast again.
    startJob(AnnounceReplyJob, peer.ip, peer.port, peer.protocol, v1,
             https ? peer.fingerprint : QString(), isAnnounce);
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
    m_previousScanStarted = m_scanStarted;
    m_scanStarted = QDateTime::currentDateTime();
    qDebug() << "Discovery: scanning" << m_scanTotal << "hosts in"
             << QHostAddress(network).toString() + "/" + QString::number(prefix);
    emit scanningChanged();
    emit scanProgressChanged();
    m_tickTimer.start();
    pumpScanQueue();
}

void Discovery::lookUp(const QString &ip)
{
    qDebug() << "Discovery: looking up" << ip;
    startJob(LookUpJob, ip, m_device->port(), QLatin1String("https"));
}

void Discovery::pumpScanQueue()
{
    while (m_activeScanJobs < kMaxConcurrentScanJobs && !m_scanQueue.isEmpty())
        startJob(ScanJob, m_scanQueue.takeFirst(), m_device->port(), QLatin1String("https"));
    finishScanIfDone();
}

void Discovery::startJob(JobKind kind, const QString &ip, int port, const QString &protocol, bool v1,
                         const QString &expectedFingerprint, bool udpFallback)
{
    QUrl url;
    url.setScheme(protocol);
    url.setHost(ip);
    url.setPort(port);
    // v1 answers its announcements via /v1/register (extra v2 fields are ignored).
    url.setPath(v1 ? QLatin1String("/api/localsend/v1/register")
                   : QLatin1String("/api/localsend/v2/register"));

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QLatin1String("application/json"));
    QNetworkReply *reply = m_nam->post(request, Json::serialize(m_device->toRegisterMap()));

    Job job;
    job.kind = kind;
    job.ip = ip;
    job.port = port;
    job.protocol = protocol;
    job.v1 = v1;
    job.deadline = QDateTime::currentDateTime().addMSecs(
                kind == ScanJob ? kScanTimeoutMs
                                : kind == LookUpJob ? kLookUpTimeoutMs : kAnnounceReplyTimeoutMs);
    job.timedOut = false;
    job.expectedFingerprint = expectedFingerprint;
    job.udpFallback = udpFallback;
    job.certRejected = false;
    m_jobs.insert(reply, job);
    if (kind == ScanJob)
        ++m_activeScanJobs;
    if (!m_tickTimer.isActive())
        m_tickTimer.start();
}

void Discovery::onSslErrors(QNetworkReply *reply, const QList<QSslError> &errors)
{
    // LocalSend uses self-signed certificates: accept them if they are valid
    // and, when answering a device, have the fingerprint it multicast.
    QHash<QNetworkReply *, Job>::iterator job = m_jobs.find(reply);
    const QString expected = job != m_jobs.end() ? job.value().expectedFingerprint : QString();
    QString why;
    if (!ClientIdentity::acceptPeer(errors, expected, &why)) {
        qWarning() << "Discovery: certificate of" << reply->url().host() << "rejected:" << why;
        if (job != m_jobs.end())
            job.value().certRejected = true;
        return; // the handshake fails, nothing is sent
    }
    reply->ignoreSslErrors();
}

static QString notFoundError(const QString &ip)
{
    return QString::fromLatin1("No LocalSend device found at %1").arg(ip);
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
        abortReply(reply);

    // A reply that neither finished nor could be aborted (it already had an
    // error, see abortReply()) would hold its scan slot forever, and the scan
    // - with the refresh button, which is disabled while scanning - would
    // never end. Forget such jobs a while after their deadline, and delete
    // the reply: deleting it (unlike abort()) sets no second error, and it
    // releases the reply's connection, which would otherwise stay open for
    // good - one more with every scan. Should it still report finished() in
    // the meantime, onReplyFinished() only deletes it (again: harmless).
    const QDateTime giveUp = now.addMSecs(-kStuckJobGraceMs);
    QList<QNetworkReply *> stuck;
    for (QHash<QNetworkReply *, Job>::const_iterator it = m_jobs.constBegin(); it != m_jobs.constEnd(); ++it) {
        if (it.value().timedOut && it.value().deadline < giveUp)
            stuck.append(it.key());
    }
    foreach (QNetworkReply *reply, stuck) {
        const Job job = m_jobs.take(reply);
        qWarning() << "Discovery:" << job.ip << job.protocol << "request never finished"
                   << "(error" << int(reply->error()) << ") - giving up on it";
        reply->deleteLater();
        if (job.kind == ScanJob) {
            --m_activeScanJobs;
            ++m_scanDone;
            emit scanProgressChanged();
        } else if (job.kind == LookUpJob) {
            emit lookUpFinished(job.ip, QString(), notFoundError(job.ip));
        }
    }
    if (!stuck.isEmpty())
        pumpScanQueue();

    if (m_jobs.isEmpty() && !m_scanning)
        m_tickTimer.stop();
}

// Errors that mean a TCP connection was made but the TLS handshake failed,
// which is what a TLS ClientHello sent to a plain-HTTP server ends in.
static bool looksLikePlainHttpServer(QNetworkReply::NetworkError error)
{
    switch (error) {
    case QNetworkReply::SslHandshakeFailedError:
    case QNetworkReply::RemoteHostClosedError:
    case QNetworkReply::ProtocolFailure:
    case QNetworkReply::UnknownNetworkError:
        return true;
    default:
        return false;
    }
}

// Our own connection went away (WLAN off / out of range): every further
// request would fail the same way.
//
// Qt 4.7 and 4.8 have no NetworkSessionFailedError (that is Qt 5). Qt 4.8's
// QNetworkReplyImpl reports a failed session as UnknownNetworkError with the
// text "Network session error.", and a request made while the manager is
// NotAccessible as UnknownNetworkError "Network access is disabled.". The
// HTTP backend also uses UnknownNetworkError for socket errors it cannot map,
// e.g. a TLS handshake answered by a plain-http server, so the code alone
// says nothing; only these two texts (untranslated: we install no
// translator) mean that our connection is gone.
static bool isNetworkGone(QNetworkReply::NetworkError error, const QString &errorString)
{
    if (error == QNetworkReply::TemporaryNetworkFailureError)
        return true;
    return error == QNetworkReply::UnknownNetworkError
            && (errorString == QLatin1String("Network session error.")
                || errorString == QLatin1String("Network access is disabled."));
}

// No (LocalSend) server at that address: not worth logging during a scan.
static bool isNothingThere(QNetworkReply::NetworkError error, bool timedOut)
{
    return timedOut
            || error == QNetworkReply::ConnectionRefusedError
            || error == QNetworkReply::HostNotFoundError
            || error == QNetworkReply::TimeoutError
            || error == QNetworkReply::OperationCanceledError;
}

void Discovery::onReplyFinished(QNetworkReply *reply)
{
    reply->deleteLater();
    if (!m_jobs.contains(reply))
        return;
    Job job = m_jobs.take(reply);
    if (job.kind == ScanJob)
        --m_activeScanJobs;

    QNetworkReply::NetworkError error = reply->error();

    // A failed session is reported as UnknownNetworkError, which on its own
    // may also be a TLS handshake with a plain-http server; the error text
    // (see isNetworkGone()) and the access manager's accessibility tell the
    // two apart.
    if (error != QNetworkReply::NoError
            && (isNetworkGone(error, reply->errorString()) || !m_network->isOnline()
                || m_nam->networkAccessible() == QNetworkAccessManager::NotAccessible)) {
        // Don't start more requests (scan queue, http retries) on a dead
        // connection: each would just fail. NetworkManager reports offline
        // separately and stopAll() cleans up the rest.
        qWarning() << "Discovery: network lost (" << int(error) << reply->errorString()
                   << ", accessible" << int(m_nam->networkAccessible()) << ") - stopping scan";
        abortJobs();
        if (job.kind == LookUpJob)
            emit lookUpFinished(job.ip, QString(), QLatin1String("Connection lost"));
        return;
    }

    // Over https the peer's identity is its certificate's hash (the spec:
    // the body's fingerprint is "ignored in HTTPS mode"). Checked again here,
    // not only in onSslErrors(): a reused kept-alive connection has none.
    QString certFingerprint;
    if (error == QNetworkReply::NoError && job.protocol == QLatin1String("https")) {
        certFingerprint = ClientIdentity::peerFingerprint(reply);
        if (certFingerprint.isEmpty()) {
            qWarning() << "Discovery:" << job.ip << "answered over https without a certificate - ignored";
            job.certRejected = true;
        } else if (!job.expectedFingerprint.isEmpty()
                   && certFingerprint != job.expectedFingerprint.toUpper()) {
            qWarning() << "Discovery:" << job.ip << "certificate fingerprint mismatch: expected"
                       << job.expectedFingerprint.toUpper() << "got" << certFingerprint << "- ignored";
            job.certRejected = true;
        }
        if (job.certRejected)
            error = QNetworkReply::SslHandshakeFailedError;
    }

    QString fingerprint; // of the device that answered
    if (error == QNetworkReply::NoError) {
        bool ok = false;
        QVariantMap map = Json::parseObject(reply->readAll(), &ok);
        if (ok && !certFingerprint.isEmpty())
            map.insert(QLatin1String("fingerprint"), certFingerprint);
        if (ok)
            fingerprint = map.value(QLatin1String("fingerprint")).toString();
        if (ok && fingerprint != m_device->fingerprint()) {
            map.insert(QLatin1String("protocol"), job.protocol); // the one that actually worked
            map.insert(QLatin1String("port"), job.port);
            const char *source = job.kind == ScanJob ? "scan"
                               : job.kind == LookUpJob ? "address" : "announce";
            m_peers->upsert(PeerModel::peerFromMap(map, job.ip, job.port, job.protocol,
                                                   QLatin1String(source)));
        } else if (!ok) {
            qWarning() << "Discovery:" << job.ip << "answered with invalid JSON";
        }
    } else if (job.certRejected) {
        // Logged already; no http retry, no UDP answer.
    } else if (job.kind != AnnounceReplyJob && job.protocol == QLatin1String("https")
               && !job.timedOut && looksLikePlainHttpServer(error)) {
        // Something answered, but not with TLS: probably a LocalSend instance
        // with encryption disabled. Retry over HTTP.
        qDebug() << "Discovery:" << job.ip << "https failed (" << int(error) << reply->errorString()
                 << ") - trying http";
        startJob(job.kind, job.ip, job.port, QLatin1String("http"), job.v1);
        return;
    } else if (!isNothingThere(error, job.timedOut) || job.kind != ScanJob) {
        qDebug() << "Discovery:" << job.ip << job.protocol << "failed:" << int(error)
                 << reply->errorString();
    }

    if (job.kind == AnnounceReplyJob && job.udpFallback && !job.certRejected
            && error != QNetworkReply::NoError) {
        // The announcing peer told us its protocol, so there is no point in
        // switching it (the official app does not either). Fall back to UDP.
        qDebug() << "Discovery: HTTP reply to" << job.ip << "failed, replying via UDP";
        replyViaUdp();
    }

    if (job.kind == LookUpJob) {
        QString why;
        if (job.certRejected)
            why = QString::fromLatin1("The certificate of %1 was rejected").arg(job.ip);
        else if (fingerprint.isEmpty())
            why = notFoundError(job.ip);
        else if (fingerprint == m_device->fingerprint())
            why = QLatin1String("This is the address of this phone");
        emit lookUpFinished(job.ip, why.isEmpty() ? fingerprint : QString(), why);
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
    // Drop devices not seen (scan, register or multicast) during this refresh
    // *and* the one before it. A single miss is common and not a reason to
    // drop a device: a slow TLS handshake on the phone, a lost UDP packet, or
    // a peer outside the scanned /24 whose register reply got lost.
    if (m_previousScanStarted.isValid())
        m_peers->removeOlderThan(m_previousScanStarted);
    emit scanningChanged();
}

// ---------------------------------------------------------------- HTTP server

static bool isRegisterPath(const QString &path)
{
    return path == QLatin1String("/api/localsend/v2/register")
            || path == QLatin1String("/api/localsend/v1/register");
}

static bool isInfoPath(const QString &path)
{
    return path == QLatin1String("/api/localsend/v2/info")
            || path == QLatin1String("/api/localsend/v1/info");
}

bool Discovery::handles(const HttpRequest &request) const
{
    return isRegisterPath(request.path) || isInfoPath(request.path);
}

HttpResponse Discovery::handleRequest(const HttpRequest &request)
{
    HttpResponse response;
    const bool isRegister = isRegisterPath(request.path);
    const bool isInfo = isInfoPath(request.path);

    if (isRegister && request.method == QLatin1String("POST")) {
        bool ok = false;
        QVariantMap map = Json::parseObject(request.body, &ok);
        if (!ok) {
            response.status = 400;
            response.body = "{\"message\":\"Invalid body\"}";
            return response;
        }
        const QString claimed = map.value(QLatin1String("fingerprint")).toString();
        if (!request.peerFingerprint.isEmpty() && claimed.toUpper() != request.peerFingerprint) {
            // Like the official app: answered, but not stored.
            qWarning() << "Discovery: ignoring register from" << request.remoteAddress.toString()
                       << "- claimed fingerprint does not match the client certificate";
        } else if (claimed != m_device->fingerprint()) {
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

    response.status = 405;
    response.body = "{\"message\":\"Method not allowed\"}";
    return response;
}
