#include "receiver.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QUuid>

#include "historymodel.h"
#include "json.h"
#include "networkmanager.h"
#include "sender.h"
#include "settings.h"
#include "webshare.h"

static const int kTickMs = 500;
// No upload running and none started for this long: the sender is gone.
static const int kGoneTimeoutMs = 30000;
// Every file finished or failed (and some failed): wait this long for the
// sender's /cancel, so a cancelled transfer is reported as cancelled.
static const int kSettleMs = 3000;
// No time estimate before this much receiving (the first numbers jump around).
static const int kEstimateAfterMs = 2000;
// Wrong PINs in a row from one address before we answer 429.
static const int kMaxPinAttempts = 5;

static const char *kRoutePrefix = "/api/localsend/v2/";

static QString route(const HttpRequest &request)
{
    const QString prefix = QLatin1String(kRoutePrefix);
    return request.path.startsWith(prefix) ? request.path.mid(prefix.size()) : QString();
}

Receiver::Receiver(HttpServer *server, Settings *settings, NetworkManager *network,
                   WebShare *webShare, HistoryModel *history, QObject *parent)
    : QObject(parent),
      m_server(server), m_settings(settings), m_network(network), m_webShare(webShare),
      m_history(history),
      m_state(QLatin1String("idle")),
      m_peerHttps(false),
      m_totalSize(0),
      m_isMessage(false),
      m_pendingRequest(0),
      m_toGallery(false),
      m_remainingSeconds(-1)
{
    m_server->addHandler(this);
    connect(m_network, SIGNAL(onlineChanged()), this, SLOT(onOnlineChanged()));
    m_tick.setInterval(kTickMs);
    connect(&m_tick, SIGNAL(timeout()), this, SLOT(onTick()));
}

Receiver::~Receiver()
{
    // Don't leave half-written files behind.
    foreach (quint32 id, m_uploads.keys())
        closeUpload(id, false);
}

bool Receiver::isBusy() const
{
    return m_state == QLatin1String("request") || m_state == QLatin1String("receiving");
}

QVariantList Receiver::files() const
{
    QVariantList list;
    foreach (const InFile &f, m_files) {
        QVariantMap m;
        m.insert(QLatin1String("fileName"), f.fileName);
        m.insert(QLatin1String("size"), qreal(f.size));
        m.insert(QLatin1String("fileType"), f.fileType);
        list.append(m);
    }
    return list;
}

QString Receiver::message() const
{
    return m_isMessage && !m_files.isEmpty() ? m_files.first().preview : QString();
}

QString Receiver::folder() const
{
    if (m_usedFolders.size() == 1)
        return QDir::toNativeSeparators(m_usedFolders.first());
    return QDir::toNativeSeparators(m_folder.isEmpty() ? m_settings->downloadFolder() : m_folder);
}

QStringList Receiver::folders() const
{
    if (m_usedFolders.isEmpty())
        return QStringList() << folder();
    QStringList list;
    foreach (const QString &f, m_usedFolders)
        list.append(QDir::toNativeSeparators(f));
    return list;
}

int Receiver::filesDone() const
{
    int n = 0;
    foreach (const InFile &f, m_files) {
        if (f.status == Done)
            ++n;
    }
    return n;
}

qreal Receiver::bytesReceived() const
{
    qint64 n = 0;
    foreach (const InFile &f, m_files)
        n += f.received;
    return qreal(n);
}

qreal Receiver::progress() const
{
    if (m_totalSize <= 0)
        return m_state == QLatin1String("finished") ? 1.0 : 0.0;
    return bytesReceived() / qreal(m_totalSize);
}

// ---------------------------------------------------------------- HTTP

bool Receiver::handles(const HttpRequest &request) const
{
    const QString r = route(request);
    return r == QLatin1String("prepare-upload") || r == QLatin1String("upload")
            || r == QLatin1String("cancel");
}

bool Receiver::streamsBody(const HttpRequest &request) const
{
    return route(request) == QLatin1String("upload");
}

HttpResponse Receiver::bodyData(const HttpRequest &request, const QByteArray &data)
{
    if (!m_uploads.contains(request.id))
        return startUpload(request); // called once with empty data, after the headers
    return writeUpload(request, data);
}

HttpResponse Receiver::handleRequest(const HttpRequest &request)
{
    if (request.method != QLatin1String("POST"))
        return HttpResponse::message(405, QLatin1String("Method not allowed"));
    const QString r = route(request);
    if (r == QLatin1String("prepare-upload"))
        return prepareUpload(request);
    if (r == QLatin1String("cancel"))
        return cancelRequest(request);
    return finishUpload(request);
}

void Receiver::requestAborted(const HttpRequest &request)
{
    if (request.id == m_pendingRequest) {
        m_pendingRequest = 0;
        if (m_state == QLatin1String("request")) {
            qDebug() << "Receiver:" << m_peerAlias << "withdrew the request";
            finish(QLatin1String("cancelled"),
                   m_network->isOnline()
                   ? QString::fromLatin1("%1 cancelled the transfer").arg(m_peerAlias)
                   : QString::fromLatin1("Connection lost"));
        }
        return;
    }
    if (m_uploads.contains(request.id)) {
        const InFile &f = m_files.at(m_uploads.value(request.id).file);
        qWarning() << "Receiver: upload of" << f.fileName << "broke off after"
                   << f.received << "of" << f.size << "bytes";
        closeUpload(request.id, false);
        // Not the end of the session by itself: the sender may still send
        // /cancel or the other files. onTick() gives up eventually.
        m_lastActivity = QDateTime::currentDateTime();
    }
}

// ---------------------------------------------------------------- offer

HttpResponse Receiver::prepareUpload(const HttpRequest &request)
{
    bool ok = false;
    const QVariantMap body = Json::parseObject(request.body, &ok);
    const QVariantMap info = body.value(QLatin1String("info")).toMap();
    const QVariantMap filesMap = body.value(QLatin1String("files")).toMap();
    if (!ok || info.isEmpty() || filesMap.isEmpty()) {
        qWarning() << "Receiver: invalid prepare-upload from" << request.remoteAddress.toString();
        return HttpResponse::message(400, QLatin1String("Invalid body"));
    }
    const QString alias = info.value(QLatin1String("alias")).toString();
    if (isBusy()) {
        qDebug() << "Receiver: request from" << alias << "refused, busy with" << m_peerAlias;
        return HttpResponse::message(409, QLatin1String("Blocked by another session"));
    }

    const QString ip = request.remoteAddress.toString();
    // While the link is active its PIN applies to everyone (official app:
    // the web share PIN replaces the receive PIN).
    const bool viaLink = m_webShare && m_webShare->isReceiveActive();
    const QString requiredPin = viaLink ? m_webShare->pin()
                                        : (m_settings->requirePin() ? m_settings->pin() : QString());
    if (!requiredPin.isEmpty()) {
        const QString pin = request.queryValue(QLatin1String("pin"));
        if (pin != requiredPin) {
            int wrong = m_wrongPins.value(ip);
            if (!pin.isEmpty())
                m_wrongPins.insert(ip, ++wrong);
            if (wrong >= kMaxPinAttempts) {
                qWarning() << "Receiver:" << alias << ip << "- too many wrong PINs";
                return HttpResponse::message(429, QLatin1String("Too many attempts"));
            }
            qDebug() << "Receiver:" << alias << (pin.isEmpty() ? "sent no PIN" : "sent a wrong PIN");
            return HttpResponse::message(401, pin.isEmpty() ? QLatin1String("PIN required")
                                                            : QLatin1String("Invalid PIN"));
        }
        m_wrongPins.remove(ip);
    }

    reset();
    m_peerAlias = alias;
    m_peerDeviceModel = info.value(QLatin1String("deviceModel")).toString();
    m_peerDeviceType = info.value(QLatin1String("deviceType")).toString();
    m_peerFingerprint = info.value(QLatin1String("fingerprint")).toString();
    m_peerVersion = info.value(QLatin1String("version")).toString();
    m_peerHttps = info.value(QLatin1String("protocol")).toString() == QLatin1String("https");
    m_peerIp = request.remoteAddress;

    for (QVariantMap::const_iterator it = filesMap.constBegin(); it != filesMap.constEnd(); ++it) {
        const QVariantMap m = it.value().toMap();
        InFile f;
        f.id = m.value(QLatin1String("id"), it.key()).toString();
        f.fileName = m.value(QLatin1String("fileName")).toString();
        f.fileType = m.value(QLatin1String("fileType")).toString();
        f.preview = m.value(QLatin1String("preview")).toString();
        f.size = qint64(m.value(QLatin1String("size")).toDouble());
        f.status = Pending;
        f.received = 0;
        if (f.id.isEmpty() || f.fileName.isEmpty() || f.size < 0 || m_fileIndex.contains(f.id)) {
            qWarning() << "Receiver: skipping invalid file entry" << it.key();
            continue;
        }
        m_fileIndex.insert(f.id, m_files.size());
        m_files.append(f);
        m_totalSize += f.size;
    }
    if (m_files.isEmpty()) {
        reset();
        return HttpResponse::message(400, QLatin1String("No valid files"));
    }
    // Like the official app: a single text with its content as preview is a
    // message, shown instead of saved.
    m_isMessage = m_files.size() == 1
            && m_files.first().fileType.startsWith(QLatin1String("text/"))
            && !m_files.first().preview.isEmpty();

    qDebug() << "Receiver:" << alias << ip << m_peerDeviceModel << m_peerDeviceType
             << "v" + m_peerVersion << "offers"
             << (m_isMessage ? QString::fromLatin1("a message")
                             : QString::fromLatin1("%1 file(s), %2 bytes")
                               .arg(m_files.size()).arg(m_totalSize));

    if (m_settings->quickSave() && !m_isMessage) {
        qDebug() << "Receiver: quick save - accepting at once";
        return startSession();
    }
    if (viaLink && m_settings->receiveViaLinkAutoAccept() && !m_isMessage) {
        qDebug() << "Receiver: receive via link with auto accept - accepting at once";
        return startSession();
    }
    m_pendingRequest = request.id;
    setState(QLatin1String("request"));
    return HttpResponse(0); // answered by accept() / decline()
}

void Receiver::accept()
{
    if (m_state != QLatin1String("request") || !m_pendingRequest)
        return;
    qDebug() << "Receiver: accepted by the user";
    const quint32 id = m_pendingRequest;
    m_pendingRequest = 0;
    m_server->respond(id, startSession());
}

void Receiver::decline()
{
    if (m_state != QLatin1String("request"))
        return;
    qDebug() << "Receiver: declined by the user";
    const quint32 id = m_pendingRequest;
    m_pendingRequest = 0;
    if (id)
        m_server->respond(id, HttpResponse::message(403, QLatin1String("Rejected")));
    setState(QLatin1String("idle"));
}

// The answer to an accepted prepare-upload.
HttpResponse Receiver::startSession()
{
    if (m_isMessage) {
        // Nothing to upload: the message is all there is.
        if (m_history && m_settings->saveToHistory())
            m_history->addText(message(), m_peerAlias);
        finish(QLatin1String("finished"));
        return HttpResponse(204);
    }
    m_folder = m_settings->downloadFolder();
    if (!QDir().mkpath(m_folder)) {
        qWarning() << "Receiver: cannot create" << m_folder;
        finish(QLatin1String("failed"),
               QString::fromLatin1("Can't create the folder %1").arg(QDir::toNativeSeparators(m_folder)));
        return HttpResponse::message(500, QLatin1String("Cannot save files"));
    }
    // Like the official app: media go to the gallery unless the files come
    // with folders (a sent folder stays together in the download folder).
    m_toGallery = m_settings->saveMediaToGallery();
    if (m_toGallery) {
        foreach (const InFile &f, m_files) {
            if (f.fileName.contains(QLatin1Char('/')) || f.fileName.contains(QLatin1Char('\\'))) {
                qDebug() << "Receiver: files come with folders - not saving media to the gallery";
                m_toGallery = false;
                break;
            }
        }
    }
    m_usedFolders.clear();
    m_sessionId = randomToken();
    QVariantMap tokens;
    for (int i = 0; i < m_files.size(); ++i) {
        m_files[i].token = randomToken();
        tokens.insert(m_files.at(i).id, m_files.at(i).token);
    }
    QVariantMap answer;
    answer.insert(QLatin1String("sessionId"), m_sessionId);
    answer.insert(QLatin1String("files"), tokens);

    qDebug() << "Receiver: session" << m_sessionId << "- saving to" << m_folder
             << (m_toGallery ? "(media to the gallery)" : "");
    m_receivingSince = m_lastActivity = QDateTime::currentDateTime();
    m_remainingSeconds = -1;
    setState(QLatin1String("receiving"));
    m_tick.start();
    emit progressChanged();
    return HttpResponse(200, Json::serialize(answer));
}

HttpResponse Receiver::cancelRequest(const HttpRequest &request)
{
    const QString sessionId = request.queryValue(QLatin1String("sessionId"));
    const bool fromPeer = request.remoteAddress == m_peerIp;
    if (m_state == QLatin1String("receiving") && sessionId == m_sessionId && fromPeer) {
        qDebug() << "Receiver:" << m_peerAlias << "cancelled the session";
        finish(QLatin1String("cancelled"),
               QString::fromLatin1("%1 cancelled the transfer").arg(m_peerAlias));
    } else if (m_state == QLatin1String("request") && fromPeer) {
        // No session yet: the sender gave up waiting for our answer.
        qDebug() << "Receiver:" << m_peerAlias << "cancelled the request";
        finish(QLatin1String("cancelled"),
               QString::fromLatin1("%1 cancelled the transfer").arg(m_peerAlias));
    } else {
        qDebug() << "Receiver: cancel for an unknown session ignored";
    }
    return HttpResponse(200);
}

// ---------------------------------------------------------------- upload

HttpResponse Receiver::startUpload(const HttpRequest &request)
{
    if (request.method != QLatin1String("POST"))
        return HttpResponse::message(405, QLatin1String("Method not allowed"));
    const QString sessionId = request.queryValue(QLatin1String("sessionId"));
    const QString fileId = request.queryValue(QLatin1String("fileId"));
    const QString token = request.queryValue(QLatin1String("token"));
    if (sessionId.isEmpty() || fileId.isEmpty() || token.isEmpty())
        return HttpResponse::message(400, QLatin1String("Missing parameters"));
    if (m_state != QLatin1String("receiving") || sessionId != m_sessionId) {
        qWarning() << "Receiver: upload for an unknown session from" << request.remoteAddress.toString();
        return HttpResponse::message(403, QLatin1String("Invalid session"));
    }
    if (request.remoteAddress != m_peerIp) {
        qWarning() << "Receiver: upload from" << request.remoteAddress.toString()
                   << "but the session belongs to" << m_peerIp.toString();
        return HttpResponse::message(403, QLatin1String("Invalid IP address"));
    }
    const int index = m_fileIndex.value(fileId, -1);
    if (index < 0 || m_files.at(index).token != token)
        return HttpResponse::message(403, QLatin1String("Invalid token"));
    InFile &f = m_files[index];
    if (f.status == Done || f.status == Receiving)
        return HttpResponse::message(409, QLatin1String("File already received"));

    QString base = m_folder;
    const int kind = m_toGallery ? mediaKind(f) : 0;
    if (kind) {
        const QString gallery = m_settings->mediaFolder(kind == 2);
        if (QDir().mkpath(gallery))
            base = gallery;
        else
            qWarning() << "Receiver: cannot create" << gallery << "- saving" << f.fileName
                       << "to the download folder";
    }
    if (!m_usedFolders.contains(base))
        m_usedFolders.append(base);
    const QString path = uniquePath(base + QLatin1Char('/') + safeRelativePath(f.fileName));
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile *out = new QFile(path);
    if (!out->open(QIODevice::WriteOnly)) {
        qWarning() << "Receiver: cannot write" << path << out->errorString();
        delete out;
        f.status = Failed;
        finish(QLatin1String("failed"),
               QString::fromLatin1("Can't save %1").arg(f.fileName));
        return HttpResponse::message(500, QLatin1String("Cannot save the file"));
    }
    f.status = Receiving;
    f.received = 0;
    f.savedPath = path;
    Upload u;
    u.file = index;
    u.out = out;
    m_uploads.insert(request.id, u);
    m_currentName = f.fileName;
    m_lastActivity = QDateTime::currentDateTime();
    qDebug() << "Receiver: receiving" << f.fileName << f.size << "bytes into" << path;
    return HttpResponse(0);
}

HttpResponse Receiver::writeUpload(const HttpRequest &request, const QByteArray &data)
{
    const Upload u = m_uploads.value(request.id);
    InFile &f = m_files[u.file];
    if (f.received + data.size() > f.size) {
        qWarning() << "Receiver:" << f.fileName << "is larger than the" << f.size << "bytes announced";
        closeUpload(request.id, false);
        return HttpResponse::message(400, QLatin1String("More data than announced"));
    }
    if (u.out->write(data) != data.size()) {
        qWarning() << "Receiver: writing" << f.savedPath << "failed:" << u.out->errorString();
        const QString name = f.fileName;
        closeUpload(request.id, false);
        finish(QLatin1String("failed"),
               QString::fromLatin1("Can't save %1 (memory full?)").arg(name));
        return HttpResponse::message(500, QLatin1String("Cannot save the file"));
    }
    f.received += data.size();
    m_lastActivity = QDateTime::currentDateTime();
    // progressChanged() comes from onTick(): one per network chunk would
    // keep the QML bindings busy.
    return HttpResponse(0);
}

HttpResponse Receiver::finishUpload(const HttpRequest &request)
{
    if (!m_uploads.contains(request.id))
        return HttpResponse::message(500, QLatin1String("No such upload"));
    const InFile &f = m_files.at(m_uploads.value(request.id).file);
    if (f.received != f.size) {
        qWarning() << "Receiver:" << f.fileName << "ended after" << f.received << "of"
                   << f.size << "bytes";
        closeUpload(request.id, false);
        return HttpResponse::message(400, QLatin1String("Incomplete file"));
    }
    closeUpload(request.id, true);
    if (f.status == Done && m_history && m_settings->saveToHistory())
        m_history->addFile(f.savedPath, f.size, m_peerAlias);
    qDebug() << "Receiver: saved" << QDir::toNativeSeparators(f.savedPath)
             << "(" << filesDone() << "of" << m_files.size() << ")";
    m_lastActivity = QDateTime::currentDateTime();
    emit progressChanged();
    checkDone();
    return HttpResponse(200);
}

// Ends an upload; a file that isn't kept is deleted and marked failed.
void Receiver::closeUpload(quint32 requestId, bool keepFile)
{
    if (!m_uploads.contains(requestId))
        return;
    const Upload u = m_uploads.take(requestId);
    InFile &f = m_files[u.file];
    u.out->close();
    if (!u.out->error() && keepFile) {
        f.status = Done;
    } else {
        if (keepFile)
            qWarning() << "Receiver: closing" << f.savedPath << "failed:" << u.out->errorString();
        u.out->remove();
        f.status = Failed;
        f.received = 0;
        f.savedPath.clear();
    }
    delete u.out;
}

// Stops every running upload (the senders see their connection close).
void Receiver::dropUploads()
{
    foreach (quint32 id, m_uploads.keys()) {
        closeUpload(id, false);
        m_server->drop(id);
    }
}

void Receiver::checkDone()
{
    if (m_state != QLatin1String("receiving"))
        return;
    foreach (const InFile &f, m_files) {
        if (f.status != Done)
            return;
    }
    qDebug() << "Receiver: all" << m_files.size() << "file(s) received from" << m_peerAlias;
    finish(QLatin1String("finished"));
}

void Receiver::onTick()
{
    if (m_state != QLatin1String("receiving")) {
        m_tick.stop();
        return;
    }
    const QDateTime now = QDateTime::currentDateTime();
    if (m_uploads.isEmpty()) {
        bool pending = false;
        foreach (const InFile &f, m_files) {
            if (f.status == Pending)
                pending = true;
        }
        const qint64 idle = m_lastActivity.msecsTo(now);
        if ((!pending && idle >= kSettleMs) || idle >= kGoneTimeoutMs) {
            const int done = filesDone();
            qWarning() << "Receiver: nothing more from" << m_peerAlias << "-" << done
                       << "of" << m_files.size() << "file(s) received";
            finish(QLatin1String("failed"),
                   done == 0 ? QString::fromLatin1("%1 stopped sending").arg(m_peerAlias)
                             : QString::fromLatin1("%1 stopped sending. %2 of %3 files were saved.")
                               .arg(m_peerAlias).arg(done).arg(m_files.size()));
            return;
        }
    }
    // Average speed since we accepted.
    const qint64 elapsed = m_receivingSince.msecsTo(now);
    const qreal received = bytesReceived();
    if (elapsed >= kEstimateAfterMs && received > 0) {
        const double left = double(m_totalSize - received) * double(elapsed) / double(received) / 1000.0;
        m_remainingSeconds = int(left + 0.999);
    }
    emit progressChanged();
}

void Receiver::onOnlineChanged()
{
    if (!m_network->isOnline() && isBusy()) {
        qWarning() << "Receiver: connection lost (state" << m_state + ")";
        finish(QLatin1String("failed"), QLatin1String("Connection lost"));
    }
}

void Receiver::cancel()
{
    if (m_state == QLatin1String("request")) {
        decline();
        return;
    }
    if (m_state != QLatin1String("receiving"))
        return;
    qDebug() << "Receiver: cancelled by the user";
    finish(QLatin1String("cancelled"), QLatin1String("The transfer was cancelled"));
}

// ---------------------------------------------------------------- state

void Receiver::finish(const QString &state, const QString &error)
{
    dropUploads();
    if (m_pendingRequest) {
        m_server->drop(m_pendingRequest);
        m_pendingRequest = 0;
    }
    m_tick.stop();
    m_sessionId.clear();
    m_currentName.clear();
    m_remainingSeconds = state == QLatin1String("finished") ? 0 : -1;
    setState(state, error);
    emit progressChanged();
}

void Receiver::setState(const QString &state, const QString &error)
{
    if (!error.isEmpty())
        qDebug() << "Receiver:" << state + ":" << error;
    else
        qDebug() << "Receiver:" << state;
    m_state = state;
    m_errorText = error;
    emit stateChanged();
}

// Forgets the previous session (not its state: setState() follows).
void Receiver::reset()
{
    m_peerAlias.clear();
    m_peerDeviceModel.clear();
    m_peerDeviceType.clear();
    m_peerFingerprint.clear();
    m_peerVersion.clear();
    m_peerHttps = false;
    m_peerIp = QHostAddress();
    m_files.clear();
    m_fileIndex.clear();
    m_totalSize = 0;
    m_isMessage = false;
    m_sessionId.clear();
    m_folder.clear();
    m_toGallery = false;
    m_usedFolders.clear();
    m_currentName.clear();
    m_remainingSeconds = -1;
}

// ---------------------------------------------------------------- helpers

// 1 for an image, 2 for a video, 0 otherwise. Like the official app this
// goes by the announced MIME type ("image/...", "video/..."); a missing or
// generic type ("application/octet-stream", e.g. from some browsers) is
// guessed from the file name.
int Receiver::mediaKind(const InFile &f)
{
    QString type = f.fileType.trimmed().toLower();
    if (type == QLatin1String("image") || type == QLatin1String("video")) // protocol v1
        type += QLatin1Char('/');
    if (type.isEmpty() || type == QLatin1String("application/octet-stream"))
        type = Sender::mimeType(f.fileName);
    if (type.startsWith(QLatin1String("image/")))
        return 1;
    if (type.startsWith(QLatin1String("video/")))
        return 2;
    return 0;
}

// A path below the download folder made from the sender's file name: no
// "..", no absolute paths, no characters Symbian/Windows file names can't
// have. Folders are kept ("Trip/day1/a.jpg").
QString Receiver::safeRelativePath(const QString &fileName)
{
    QString name = fileName;
    name.replace(QLatin1Char('\\'), QLatin1Char('/'));
    static const QString bad = QLatin1String("<>:\"|?*");
    QStringList parts;
    foreach (QString part, name.split(QLatin1Char('/'), QString::SkipEmptyParts)) {
        part = part.trimmed();
        if (part == QLatin1String(".") || part == QLatin1String(".."))
            continue;
        for (int i = 0; i < part.size(); ++i) {
            if (part.at(i).unicode() < 32 || bad.contains(part.at(i)))
                part[i] = QLatin1Char('_');
        }
        while (part.endsWith(QLatin1Char('.')) || part.endsWith(QLatin1Char(' ')))
            part.chop(1);
        if (!part.isEmpty())
            parts.append(part);
    }
    return parts.isEmpty() ? QString::fromLatin1("file") : parts.join(QLatin1String("/"));
}

// path, or "name (1).ext", "name (2).ext", ... if it exists already.
QString Receiver::uniquePath(const QString &path)
{
    if (!QFileInfo(path).exists())
        return path;
    const QFileInfo fi(path);
    const QString dir = fi.absolutePath() + QLatin1Char('/');
    const QString base = fi.completeBaseName();
    const QString suffix = fi.suffix().isEmpty() ? QString() : QLatin1Char('.') + fi.suffix();
    for (int i = 1; i < 10000; ++i) {
        const QString candidate = dir + QString::fromLatin1("%1 (%2)%3").arg(base).arg(i).arg(suffix);
        if (!QFileInfo(candidate).exists())
            return candidate;
    }
    return path;
}

QString Receiver::randomToken()
{
    return QUuid::createUuid().toString().mid(1, 36);
}
