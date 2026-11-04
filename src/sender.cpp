#include "sender.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUuid>
#ifndef QT_NO_OPENSSL
#include <QSslError>
#endif

#include "clientidentity.h"
#include "deviceinfo.h"
#include "discovery.h"
#include "json.h"
#include "networkmanager.h"

static const int kTickMs = 500;
// An upload with no progress for this long is given up. Generous: after the
// last byte the receiver may still be writing the file out.
static const int kStallTimeoutMs = 60000;
// No time estimate before this much sending (the first numbers jump around).
static const int kEstimateAfterMs = 2000;

Sender::Sender(DeviceInfo *device, PeerModel *peers, SelectionModel *selection,
               NetworkManager *network, Discovery *discovery, QObject *parent)
    : QObject(parent),
      m_device(device), m_peers(peers), m_selection(selection), m_network(network),
      m_discovery(discovery),
      m_nam(new QNetworkAccessManager(this)),
      m_state(QLatin1String("idle")),
      m_pinSent(false),
      m_certRejected(false),
      m_reply(0),
      m_current(-1),
      m_bytesTotal(0), m_bytesDone(0), m_currentSent(0),
      m_remainingSeconds(-1)
{
#ifndef QT_NO_OPENSSL
    connect(m_nam, SIGNAL(sslErrors(QNetworkReply*,QList<QSslError>)),
            this, SLOT(onSslErrors(QNetworkReply*,QList<QSslError>)));
#endif
    connect(m_network, SIGNAL(onlineChanged()), this, SLOT(onOnlineChanged()));
    connect(m_discovery, SIGNAL(lookUpFinished(QString,QString,QString)),
            this, SLOT(onLookUpFinished(QString,QString,QString)));
    m_tick.setInterval(kTickMs);
    connect(&m_tick, SIGNAL(timeout()), this, SLOT(onTick()));
}

bool Sender::isBusy() const
{
    return m_state == QLatin1String("connecting") || m_state == QLatin1String("waiting")
            || m_state == QLatin1String("pin") || m_state == QLatin1String("sending");
}

QString Sender::fileName() const
{
    if (m_current < 0 || m_current >= m_queue.size())
        return QString();
    const OutFile &f = m_queue.at(m_current);
    return f.path.isEmpty() ? QString::fromLatin1("Text") : f.fileName;
}

qreal Sender::progress() const
{
    if (m_bytesTotal <= 0)
        return m_state == QLatin1String("finished") ? 1.0 : 0.0;
    return qreal(m_bytesDone + m_currentSent) / qreal(m_bytesTotal);
}

// ---------------------------------------------------------------- start

bool Sender::send(const QString &fingerprint)
{
    if (isBusy()) {
        qWarning() << "Sender: already sending to" << m_peer.alias;
        return false;
    }
    return start(fingerprint);
}

bool Sender::sendToAddress(const QString &ip)
{
    if (isBusy()) {
        qWarning() << "Sender: already sending to" << m_peer.alias;
        return false;
    }
    reset();
    if (!m_network->isOnline()) {
        finish(QLatin1String("failed"), QLatin1String("Not connected"));
        return false;
    }
    m_peer.alias = ip; // until we know better
    setState(QLatin1String("connecting"));
    m_discovery->lookUp(ip);
    return true;
}

void Sender::onLookUpFinished(const QString &ip, const QString &fingerprint, const QString &error)
{
    // Not ours if the user cancelled meanwhile.
    if (m_state != QLatin1String("connecting") || ip != m_peer.alias)
        return;
    if (!error.isEmpty())
        finish(QLatin1String("failed"), error);
    else
        start(fingerprint);
}

void Sender::reset()
{
    m_peer = Peer();
    m_files.clear();
    m_queue.clear();
    m_pin.clear();
    m_pinSent = false;
    m_certRejected = false;
    m_sessionId.clear();
    m_current = -1;
    m_bytesTotal = m_bytesDone = m_currentSent = 0;
    m_remainingSeconds = -1;
    emit progressChanged();
}

bool Sender::start(const QString &fingerprint)
{
    reset();
    if (!m_peers->find(fingerprint, &m_peer)) {
        finish(QLatin1String("failed"), QLatin1String("The device is gone"));
        return false;
    }
    if (PeerModel::isV1(m_peer.version)) {
        qWarning() << "Sender:" << m_peer.alias << "speaks protocol" << m_peer.version;
        finish(QLatin1String("failed"),
               QString::fromLatin1("%1 runs an old LocalSend version (protocol v1), "
                                   "which Symbisend can't send to yet").arg(m_peer.alias));
        return false;
    }
    if (!m_network->isOnline()) {
        finish(QLatin1String("failed"), QLatin1String("Not connected"));
        return false;
    }

    foreach (const SelectionItem &it, m_selection->items()) {
        OutFile f;
        f.id = QUuid::createUuid().toString().mid(1, 36);
        if (it.kind == QLatin1String("text")) {
            // Like the official app: a text file whose preview is the text.
            // A receiver shows a lone text as a message (and answers 204).
            f.fileName = f.id + QLatin1String(".txt");
            f.text = it.text;
            f.size = it.text.toUtf8().size();
        } else {
            QFileInfo fi(it.path);
            if (!fi.isFile()) {
                qWarning() << "Sender: file is gone, skipped:" << it.path;
                continue;
            }
            f.fileName = it.relativePath;
            f.path = it.path;
            f.size = fi.size(); // may have changed since it was picked
        }
        m_files.append(f);
    }
    if (m_files.isEmpty()) {
        finish(QLatin1String("failed"), QLatin1String("Nothing to send"));
        return false;
    }

    qDebug() << "Sender: sending" << m_files.size() << "item(s) to" << m_peer.alias
             << m_peer.ip + ":" + QString::number(m_peer.port) << m_peer.protocol
             << "v" + m_peer.version;
    // Same access point as discovery (no connection dialog).
    m_nam->setConfiguration(m_network->configuration());
    m_nam->setNetworkAccessible(QNetworkAccessManager::Accessible);
    setState(QLatin1String("waiting"));
    postPrepare();
    return true;
}

QUrl Sender::url(const QString &route) const
{
    QUrl u;
    u.setScheme(m_peer.protocol);
    u.setHost(m_peer.ip);
    u.setPort(m_peer.port);
    u.setPath(QLatin1String("/api/localsend/v2/") + route);
    return u;
}

QString Sender::isoTime(const QDateTime &t)
{
    return t.toUTC().toString(QLatin1String("yyyy-MM-dd'T'HH:mm:ss'Z'"));
}

QString Sender::mimeType(const QString &fileName)
{
    static QHash<QString, QString> types;
    if (types.isEmpty()) {
        const char *const table[][2] = {
            { "jpg", "image/jpeg" }, { "jpeg", "image/jpeg" }, { "png", "image/png" },
            { "gif", "image/gif" }, { "bmp", "image/bmp" }, { "webp", "image/webp" },
            { "heic", "image/heic" }, { "heif", "image/heif" }, { "tif", "image/tiff" },
            { "tiff", "image/tiff" }, { "svg", "image/svg+xml" },
            { "mp4", "video/mp4" }, { "m4v", "video/mp4" }, { "3gp", "video/3gpp" },
            { "3g2", "video/3gpp2" }, { "mkv", "video/x-matroska" }, { "avi", "video/x-msvideo" },
            { "mov", "video/quicktime" }, { "wmv", "video/x-ms-wmv" }, { "flv", "video/x-flv" },
            { "webm", "video/webm" }, { "mpg", "video/mpeg" }, { "mpeg", "video/mpeg" },
            { "mp3", "audio/mpeg" }, { "m4a", "audio/mp4" }, { "aac", "audio/aac" },
            { "amr", "audio/amr" }, { "wav", "audio/wav" }, { "ogg", "audio/ogg" },
            { "flac", "audio/flac" }, { "wma", "audio/x-ms-wma" },
            { "txt", "text/plain" }, { "html", "text/html" }, { "htm", "text/html" },
            { "csv", "text/csv" }, { "vcf", "text/vcard" },
            { "pdf", "application/pdf" }, { "zip", "application/zip" },
            { "apk", "application/vnd.android.package-archive" },
            { "sis", "application/vnd.symbian.install" },
            { "sisx", "x-epoc/x-sisx-app" },
            { "jar", "application/java-archive" }
        };
        for (unsigned i = 0; i < sizeof(table) / sizeof(table[0]); ++i)
            types.insert(QLatin1String(table[i][0]), QLatin1String(table[i][1]));
    }
    return types.value(QFileInfo(fileName).suffix().toLower(),
                       QLatin1String("application/octet-stream"));
}

void Sender::postPrepare()
{
    QVariantMap files;
    foreach (const OutFile &f, m_files) {
        QVariantMap m;
        m.insert(QLatin1String("id"), f.id);
        m.insert(QLatin1String("fileName"), f.fileName);
        m.insert(QLatin1String("size"), f.size);
        if (f.path.isEmpty()) {
            m.insert(QLatin1String("fileType"), QLatin1String("text/plain"));
            m.insert(QLatin1String("preview"), f.text);
        } else {
            m.insert(QLatin1String("fileType"), mimeType(f.fileName));
            QFileInfo fi(f.path);
            QVariantMap meta;
            meta.insert(QLatin1String("modified"), isoTime(fi.lastModified()));
            meta.insert(QLatin1String("accessed"), isoTime(fi.lastRead()));
            m.insert(QLatin1String("metadata"), meta);
        }
        files.insert(f.id, m);
    }
    QVariantMap body;
    body.insert(QLatin1String("info"), m_device->toRegisterMap());
    body.insert(QLatin1String("files"), files);

    QUrl u = url(QLatin1String("prepare-upload"));
    if (!m_pin.isEmpty())
        u.addQueryItem(QLatin1String("pin"), m_pin);
    m_pinSent = !m_pin.isEmpty();
    QNetworkRequest request(u);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QLatin1String("application/json"));
    qDebug() << "Sender: prepare-upload to" << m_peer.alias
             << (m_pinSent ? "(with PIN)" : "") << "- waiting for the receiver to accept";
    // No timeout: the receiver answers only after its user decided.
    m_reply = m_nam->post(request, Json::serialize(body));
    connect(m_reply, SIGNAL(finished()), this, SLOT(onPrepareFinished()));
}

void Sender::submitPin(const QString &pin)
{
    if (m_state != QLatin1String("pin"))
        return;
    m_pin = pin;
    setState(QLatin1String("waiting"));
    postPrepare();
}

// ---------------------------------------------------------------- replies

void Sender::onPrepareFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply || reply != m_reply)
        return;
    m_reply = 0;
    reply->deleteLater();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray data = reply->readAll();
    qDebug() << "Sender: prepare-upload answered" << status
             << "(error" << int(reply->error()) << reply->errorString() + ")";

    // Checked again here, not only in onSslErrors(): a reused kept-alive
    // connection has no new handshake.
    if (!m_certRejected && status != 0 && m_peer.protocol == QLatin1String("https")) {
        const QString actual = ClientIdentity::peerFingerprint(reply);
        if (actual != m_peer.fingerprint.toUpper()) {
            qWarning() << "Sender: certificate fingerprint mismatch: expected" << m_peer.fingerprint.toUpper()
                       << "got" << (actual.isEmpty() ? QString::fromLatin1("none") : actual);
            m_certRejected = true;
        }
    }
    if (m_certRejected) {
        finish(QLatin1String("failed"), certificateError());
        return;
    }

    switch (status) {
    case 200: {
        bool ok = false;
        const QVariantMap map = Json::parseObject(data, &ok);
        const QVariantMap tokens = map.value(QLatin1String("files")).toMap();
        m_sessionId = map.value(QLatin1String("sessionId")).toString();
        if (!ok || m_sessionId.isEmpty()) {
            qWarning() << "Sender: invalid prepare-upload answer:" << data.left(300);
            finish(QLatin1String("failed"), QLatin1String("The receiver sent an invalid answer"));
            return;
        }
        // The receiver may accept only some of the files.
        m_queue.clear();
        m_bytesTotal = 0;
        foreach (OutFile f, m_files) {
            if (!tokens.contains(f.id))
                continue;
            f.token = tokens.value(f.id).toString();
            m_bytesTotal += f.size;
            m_queue.append(f);
        }
        qDebug() << "Sender: session" << m_sessionId << "-" << m_queue.size() << "of"
                 << m_files.size() << "item(s) accepted," << m_bytesTotal << "bytes";
        if (m_queue.isEmpty()) {
            finish(QLatin1String("finished"));
            return;
        }
        m_sendingSince = QDateTime::currentDateTime();
        setState(QLatin1String("sending"));
        m_tick.start();
        startNextUpload();
        return;
    }
    case 204:
        // Accepted, nothing to upload (e.g. a text message, already shown).
        finish(QLatin1String("finished"));
        return;
    case 401:
        qDebug() << "Sender:" << (m_pinSent ? "wrong PIN" : "PIN required");
        setState(QLatin1String("pin"), m_pinSent ? QLatin1String("Wrong PIN") : QString());
        return;
    case 403:
        finish(QLatin1String("declined"),
               QString::fromLatin1("%1 declined the transfer").arg(m_peer.alias));
        return;
    case 409:
        finish(QLatin1String("failed"),
               QString::fromLatin1("%1 is busy with another transfer").arg(m_peer.alias));
        return;
    case 429:
        finish(QLatin1String("failed"), QLatin1String("Too many attempts, try again later"));
        return;
    case 0:
        finish(QLatin1String("failed"), reply->errorString());
        return;
    default:
        finish(QLatin1String("failed"),
               QString::fromLatin1("%1 answered with error %2").arg(m_peer.alias).arg(status));
    }
}

void Sender::startNextUpload()
{
    ++m_current;
    m_currentSent = 0;
    if (m_current >= m_queue.size()) {
        qDebug() << "Sender: all" << m_queue.size() << "file(s) sent to" << m_peer.alias;
        finish(QLatin1String("finished"));
        return;
    }
    const OutFile &f = m_queue.at(m_current);
    QUrl u = url(QLatin1String("upload"));
    u.addEncodedQueryItem("sessionId", QUrl::toPercentEncoding(m_sessionId));
    u.addEncodedQueryItem("fileId", QUrl::toPercentEncoding(f.id));
    u.addEncodedQueryItem("token", QUrl::toPercentEncoding(f.token));
    QNetworkRequest request(u);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QLatin1String("application/octet-stream"));
    request.setHeader(QNetworkRequest::ContentLengthHeader, f.size);

    qDebug() << "Sender: uploading" << (m_current + 1) << "/" << m_queue.size()
             << f.fileName << f.size << "bytes";
    if (f.path.isEmpty()) {
        m_reply = m_nam->post(request, f.text.toUtf8());
    } else {
        QFile *file = new QFile(f.path);
        if (!file->open(QIODevice::ReadOnly)) {
            qWarning() << "Sender: cannot open" << f.path << file->errorString();
            const QString error = QString::fromLatin1("Can't read %1").arg(f.fileName);
            delete file;
            sendCancel();
            finish(QLatin1String("failed"), error);
            return;
        }
        // Streamed in chunks, not loaded into memory. The reply owns the
        // file, so it lives exactly as long as the reply reads it.
        m_reply = m_nam->post(request, file);
        file->setParent(m_reply);
    }
    connect(m_reply, SIGNAL(uploadProgress(qint64,qint64)), this, SLOT(onUploadProgress(qint64,qint64)));
    connect(m_reply, SIGNAL(finished()), this, SLOT(onUploadFinished()));
    m_lastActivity = QDateTime::currentDateTime();
    emit progressChanged();
}

void Sender::onUploadProgress(qint64 sent, qint64 total)
{
    Q_UNUSED(total);
    if (sender() != m_reply)
        return;
    if (sent > m_currentSent)
        m_lastActivity = QDateTime::currentDateTime();
    m_currentSent = sent;
    // progressChanged() is emitted by onTick(): a signal per network chunk
    // would keep the QML bindings busy.
}

void Sender::onUploadFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply || reply != m_reply)
        return;
    m_reply = 0;
    reply->deleteLater();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const OutFile &f = m_queue.at(m_current);
    if (status >= 200 && status < 300) {
        m_bytesDone += f.size;
        m_currentSent = 0;
        startNextUpload();
        return;
    }
    qWarning() << "Sender: upload of" << f.fileName << "failed:" << status
               << int(reply->error()) << reply->errorString();
    QString error;
    switch (status) {
    case 0:   error = m_certRejected ? certificateError() : reply->errorString(); break;
    case 403: error = QString::fromLatin1("%1 cancelled the transfer").arg(m_peer.alias); break;
    case 409: error = QString::fromLatin1("%1 is busy with another transfer").arg(m_peer.alias); break;
    default:  error = QString::fromLatin1("%1 answered with error %2").arg(m_peer.alias).arg(status);
    }
    sendCancel();
    finish(QLatin1String("failed"), error);
}

QString Sender::certificateError() const
{
    return QString::fromLatin1("This is not the %1 that was found (its certificate is different)")
            .arg(m_peer.alias);
}

void Sender::onSslErrors(QNetworkReply *reply, const QList<QSslError> &errors)
{
    // LocalSend uses self-signed certificates. Like the official app, every
    // request is pinned to the certificate of the device that was found: its
    // fingerprint is that certificate's hash.
    QString why;
    if (!ClientIdentity::acceptPeer(errors, m_peer.fingerprint, &why)) {
        qWarning() << "Sender: certificate of" << reply->url().host() << "rejected:" << why;
        m_certRejected = true;
        return; // the handshake fails, nothing is sent
    }
    reply->ignoreSslErrors();
}

void Sender::onTick()
{
    if (m_state != QLatin1String("sending")) {
        m_tick.stop();
        return;
    }
    const QDateTime now = QDateTime::currentDateTime();
    if (m_reply && m_lastActivity.msecsTo(now) > kStallTimeoutMs) {
        qWarning() << "Sender: no progress for" << kStallTimeoutMs / 1000 << "s - giving up";
        dropReply();
        sendCancel();
        finish(QLatin1String("failed"),
               QString::fromLatin1("%1 stopped responding").arg(m_peer.alias));
        return;
    }
    // Average speed since the first upload started.
    const qint64 elapsed = m_sendingSince.msecsTo(now);
    const qint64 sent = m_bytesDone + m_currentSent;
    if (elapsed >= kEstimateAfterMs && sent > 0) {
        const double left = double(m_bytesTotal - sent) * double(elapsed) / double(sent) / 1000.0;
        m_remainingSeconds = int(left + 0.999);
    }
    emit progressChanged();
}

void Sender::onOnlineChanged()
{
    if (!m_network->isOnline() && isBusy()) {
        qWarning() << "Sender: connection lost during the transfer";
        dropReply();
        finish(QLatin1String("failed"), QLatin1String("Connection lost"));
    }
}

// ---------------------------------------------------------------- stop

void Sender::cancel()
{
    if (!isBusy())
        return;
    qDebug() << "Sender: cancelled by the user (state" << m_state + ")";
    dropReply();
    sendCancel();
    finish(QLatin1String("cancelled"));
}

// Forgets the request in flight without its finished() reaching us.
void Sender::dropReply()
{
    if (!m_reply)
        return;
    QNetworkReply *reply = m_reply;
    m_reply = 0;
    disconnect(reply, 0, this, 0);
    // Aborting a reply that already failed makes Qt 4.8 complain
    // ("Internal problem, this method must only be called once").
    if (reply->error() == QNetworkReply::NoError && !reply->isFinished())
        reply->abort();
    reply->deleteLater();
}

// Tells the receiver to close the session. Fire and forget.
void Sender::sendCancel()
{
    if (m_sessionId.isEmpty())
        return;
    QUrl u = url(QLatin1String("cancel"));
    u.addEncodedQueryItem("sessionId", QUrl::toPercentEncoding(m_sessionId));
    qDebug() << "Sender: cancelling session" << m_sessionId;
    QNetworkReply *reply = m_nam->post(QNetworkRequest(u), QByteArray());
    connect(reply, SIGNAL(finished()), reply, SLOT(deleteLater()));
    m_sessionId.clear();
}

void Sender::finish(const QString &state, const QString &error)
{
    dropReply();
    m_tick.stop();
    m_remainingSeconds = state == QLatin1String("finished") ? 0 : -1;
    if (state == QLatin1String("finished")) {
        m_currentSent = 0;
        m_bytesDone = m_bytesTotal;
    }
    m_sessionId.clear();
    // Sent: the selection was copied at send(), so it can go. Kept after
    // declined/failed/cancelled so the user can try again.
    if (state == QLatin1String("finished")) {
        qDebug() << "Sender: clearing the selection";
        m_selection->clear();
    }
    setState(state, error);
    emit progressChanged();
}

void Sender::setState(const QString &state, const QString &error)
{
    if (!error.isEmpty())
        qDebug() << "Sender:" << state + ":" << error;
    else
        qDebug() << "Sender:" << state;
    m_state = state;
    m_errorText = error;
    emit stateChanged();
}
