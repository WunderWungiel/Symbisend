#include "httpserver.h"

#include <QDebug>
#include <QStringList>
#include <QTcpSocket>
#include <QElapsedTimer>
#include <QUrl>

#include "json.h"
#include "tlsserver.h"

// The request line + headers. LocalSend clients send well under 1 KB.
static const int kMaxHeaderSize = 16 * 1024;
// Buffered (JSON) bodies, kept in memory. Room for prepare-upload with a few
// thousand files (~200 bytes of metadata each). File data is streamed
// (HttpHandler::streamsBody()) and never held here.
static const int kMaxBodySize = 1024 * 1024;
// A client that sends nothing for this long is dropped (while it sends its
// request, and after the answer until it closes the connection).
static const int kIdleTimeoutMs = 10000;
// The same while a body is streamed: a phone's WLAN can stall for a while.
static const int kStreamIdleTimeoutMs = 30000;
static const int kIdleCheckIntervalMs = 2000;

QString HttpRequest::queryValue(const QString &key, bool *present) const
{
    if (present)
        *present = false;
    foreach (const QString &pair, query.split(QLatin1Char('&'), QString::SkipEmptyParts)) {
        const int eq = pair.indexOf(QLatin1Char('='));
        const QString name = eq >= 0 ? pair.left(eq) : pair;
        if (name != key)
            continue;
        if (present)
            *present = true;
        QByteArray value = eq >= 0 ? pair.mid(eq + 1).toLatin1() : QByteArray();
        value.replace('+', ' ');
        return QUrl::fromPercentEncoding(value);
    }
    return QString();
}

HttpResponse HttpResponse::message(int status, const QString &text)
{
    QVariantMap m;
    m.insert(QLatin1String("message"), text);
    return HttpResponse(status, Json::serialize(m));
}

struct HttpServer::Connection
{
    enum Phase { Headers, Body, Waiting, Answered };
    enum ChunkState { ChunkSize, ChunkData, ChunkDataEnd, Trailer };

    Connection()
        : id(0), socket(0), tls(0), phase(Headers), handler(0), streaming(false),
          chunked(false), chunkState(ChunkSize), remaining(0), bodyBytes(0) {}
    ~Connection() { delete tls; }

    quint32 id;
    QTcpSocket *socket;
    TlsSession *tls;         // https only
    QByteArray buffer;       // received, not processed yet
    Phase phase;
    HttpRequest request;
    HttpHandler *handler;    // 0 until the headers are in
    bool streaming;          // body goes to handler->bodyData()
    bool chunked;            // Transfer-Encoding: chunked
    ChunkState chunkState;
    qint64 remaining;        // body bytes left (Content-Length), or of the current chunk
    QDateTime lastActivity;
    // Streamed bodies: size and duration, for the throughput in the log.
    qint64 bodyBytes;
    QElapsedTimer bodyTimer;
};

HttpServer::HttpServer(QObject *parent)
    : QObject(parent), m_requireClientCert(true), m_nextId(1)
{
    connect(&m_server, SIGNAL(newConnection()), this, SLOT(onNewConnection()));
    m_idleTimer.setInterval(kIdleCheckIntervalMs);
    connect(&m_idleTimer, SIGNAL(timeout()), this, SLOT(onIdleCheck()));
}

HttpServer::~HttpServer()
{
    // The handlers may be gone already: no requestAborted() here.
    foreach (Connection *c, m_byId.values())
        remove(c, false);
}

void HttpServer::addHandler(HttpHandler *handler)
{
    m_handlers.append(handler);
}

bool HttpServer::listen(quint16 port, bool https, bool requireClientCert)
{
    m_tlsConfig.clear();
    m_requireClientCert = requireClientCert;
    if (https) {
        m_tlsConfig = TlsServerConfig::create(requireClientCert);
        if (m_tlsConfig.isNull())
            qWarning() << "HTTP: https is not possible - using http";
    }
    if (!m_server.listen(QHostAddress::Any, port)) {
        qWarning() << "HTTP: cannot listen on port" << port << m_server.errorString();
        m_tlsConfig.clear();
        return false;
    }
    qDebug() << "HTTP: listening on port" << port
             << (!isHttps() ? "(http)" : requireClientCert ? "(https, client certificate required)"
                                                           : "(https, client certificate optional)");
    emit started();
    return true;
}

void HttpServer::close()
{
    if (m_server.isListening())
        qDebug() << "HTTP: server closed";
    m_server.close();
    m_idleTimer.stop();
    // A handler's requestAborted() may drop other requests: look each one up again.
    foreach (quint32 id, m_byId.keys()) {
        if (Connection *c = m_byId.value(id))
            remove(c, true);
    }
    m_tlsConfig.clear();
}

void HttpServer::respond(quint32 requestId, const HttpResponse &response)
{
    Connection *c = m_byId.value(requestId);
    if (!c || c->phase != Connection::Waiting) {
        qDebug() << "HTTP: answer for request" << requestId << "dropped (connection gone)";
        return;
    }
    sendResponse(c, response);
}

void HttpServer::drop(quint32 requestId)
{
    if (Connection *c = m_byId.value(requestId)) {
        qDebug() << "HTTP: dropping request" << requestId << c->request.path;
        remove(c, false);
    }
}

void HttpServer::onNewConnection()
{
    while (QTcpSocket *socket = m_server.nextPendingConnection()) {
        Connection *c = new Connection;
        c->id = m_nextId++;
        if (m_nextId == 0)
            m_nextId = 1;
        c->socket = socket;
        c->lastActivity = QDateTime::currentDateTime();
        if (m_tlsConfig)
            c->tls = new TlsSession(m_tlsConfig);
        m_bySocket.insert(socket, c);
        m_byId.insert(c->id, c);
        connect(socket, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
        connect(socket, SIGNAL(disconnected()), this, SLOT(onDisconnected()));
    }
    if (!m_idleTimer.isActive())
        m_idleTimer.start();
}

void HttpServer::onDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (Connection *c = m_bySocket.value(socket))
        remove(c, true);
}

// Forgets a connection; tells its handler if the request is still open.
void HttpServer::remove(Connection *c, bool notifyHandler)
{
    m_bySocket.remove(c->socket);
    m_byId.remove(c->id);
    c->socket->disconnect(this);
    c->socket->abort();
    c->socket->deleteLater();
    const bool open = c->phase == Connection::Waiting
            || (c->phase == Connection::Body && c->streaming);
    if (notifyHandler && open && c->handler) {
        qDebug() << "HTTP: request" << c->id << c->request.path << "aborted before it was answered";
        c->handler->requestAborted(c->request);
    }
    delete c;
    if (m_byId.isEmpty())
        m_idleTimer.stop();
}

// Drops clients that stop sending in the middle of a request or never close
// the connection, so they cannot pile up. Requests waiting for our answer
// (e.g. prepare-upload until the user accepts) never time out here.
void HttpServer::onIdleCheck()
{
    const QDateTime now = QDateTime::currentDateTime();
    QList<quint32> stale;
    foreach (Connection *c, m_byId) {
        if (c->phase == Connection::Waiting)
            continue;
        const int timeout = c->phase == Connection::Body && c->streaming
                ? kStreamIdleTimeoutMs : kIdleTimeoutMs;
        if (c->lastActivity.msecsTo(now) >= timeout)
            stale.append(c->id);
    }
    foreach (quint32 id, stale) {
        if (Connection *c = m_byId.value(id)) {
            qDebug() << "HTTP: dropping idle connection from" << c->socket->peerAddress().toString()
                     << c->request.path;
            remove(c, true);
        }
    }
}

void HttpServer::onReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    Connection *c = m_bySocket.value(socket);
    if (!c)
        return;
    c->lastActivity = QDateTime::currentDateTime();
    QByteArray data = socket->readAll();
    if (c->tls) {
        const bool wasDone = c->tls->handshakeDone();
        // Also after our answer: the client's close_notify goes through here.
        data = c->tls->feed(data);
        const QByteArray out = c->tls->takeOutgoing();
        if (!out.isEmpty())
            socket->write(out);
        if (c->tls->failed()) {
            socket->flush(); // the alert, if any, before abort() drops it
            if (c->phase != Connection::Answered)
                qWarning() << "HTTP: TLS with" << socket->peerAddress().toString() << "failed:"
                           << c->tls->errorString();
            remove(c, true);
            return;
        }
        if (!wasDone && c->tls->handshakeDone())
            qDebug() << "HTTP: TLS handshake with" << socket->peerAddress().toString() << "done:"
                     << c->tls->version() << c->tls->ciphersuite() << "client certificate"
                     << (c->tls->peerFingerprint().isEmpty() ? QString::fromLatin1("none")
                                                             : c->tls->peerFingerprint().left(16) + "...");
    }
    if (c->phase == Connection::Waiting || c->phase == Connection::Answered)
        return; // nothing more is expected from this client
    if (data.isEmpty())
        return;
    if (c->buffer.isEmpty())
        c->buffer = data; // no copy
    else
        c->buffer.append(data);
    process(c);
}

void HttpServer::write(Connection *c, const QByteArray &data)
{
    if (c->tls)
        c->socket->write(c->tls->encrypt(data));
    else
        c->socket->write(data);
}

void HttpServer::process(Connection *c)
{
    const quint32 id = c->id;
    if (c->phase == Connection::Headers && !parseHeaders(c))
        return;
    if (alive(id) && c->phase == Connection::Body)
        readBody(c);
}

// Returns true once the headers are parsed and the body may be read.
bool HttpServer::parseHeaders(Connection *c)
{
    const int headerEnd = c->buffer.indexOf("\r\n\r\n");
    if (headerEnd < 0) {
        if (c->buffer.size() > kMaxHeaderSize)
            sendResponse(c, HttpResponse::message(431, QLatin1String("Request headers too large")));
        return false;
    }

    HttpRequest &req = c->request;
    req.id = c->id;
    req.remoteAddress = c->socket->peerAddress();
    req.peerFingerprint = c->tls ? c->tls->peerFingerprint() : QString();
    QList<QByteArray> lines = c->buffer.left(headerEnd).split('\n');
    c->buffer.remove(0, headerEnd + 4);
    QList<QByteArray> requestLine = lines.takeFirst().trimmed().split(' ');
    if (requestLine.size() < 2) {
        sendResponse(c, HttpResponse(400));
        return false;
    }
    req.method = QString::fromLatin1(requestLine.at(0)).toUpper();
    const QString target = QString::fromLatin1(requestLine.at(1));
    const int q = target.indexOf(QLatin1Char('?'));
    req.path = q >= 0 ? target.left(q) : target;
    req.query = q >= 0 ? target.mid(q + 1) : QString();

    foreach (const QByteArray &line, lines) {
        const int colon = line.indexOf(':');
        if (colon <= 0)
            continue;
        req.headers.insert(line.left(colon).trimmed().toLower(), line.mid(colon + 1).trimmed());
    }

    qDebug() << "HTTP:" << req.method << req.path << "from" << req.remoteAddress.toString();

    c->chunked = req.headers.value("transfer-encoding").toLower().contains("chunked");
    if (!c->chunked) {
        bool ok = false;
        c->remaining = req.headers.value("content-length", "0").toLongLong(&ok);
        if (!ok || c->remaining < 0) {
            sendResponse(c, HttpResponse::message(400, QLatin1String("Invalid Content-Length")));
            return false;
        }
    }

    foreach (HttpHandler *h, m_handlers) {
        if (h->handles(req)) {
            c->handler = h;
            break;
        }
    }
    if (!c->handler) {
        sendResponse(c, HttpResponse::message(404, QLatin1String("Not found")));
        return false;
    }
    c->streaming = c->handler->streamsBody(req);
    if (!c->streaming && !c->chunked && c->remaining > kMaxBodySize) {
        sendResponse(c, HttpResponse::message(413, QLatin1String("Request too large")));
        return false;
    }

    c->phase = Connection::Body;
    if (c->streaming) {
        c->bodyTimer.start();
        // Let the handler check the request before any body is read.
        const quint32 id = c->id;
        const HttpResponse r = c->handler->bodyData(req, QByteArray());
        if (!alive(id))
            return false;
        if (r.status != 0) {
            sendResponse(c, r);
            return false;
        }
    }
    if (req.headers.value("expect").toLower() == "100-continue")
        write(c, "HTTP/1.1 100 Continue\r\n\r\n");
    return true;
}

// Feeds everything received so far to the body; once it is complete, hands
// the request to the handler.
bool HttpServer::readBody(Connection *c)
{
    const quint32 id = c->id;
    for (;;) {
        QByteArray piece;
        bool complete = false;
        if (!takeBodyPiece(c, &piece, &complete)) {
            sendResponse(c, HttpResponse::message(400, QLatin1String("Invalid chunked body")));
            return false;
        }
        if (!piece.isEmpty()) {
            if (c->streaming) {
                c->bodyBytes += piece.size();
                const HttpResponse r = c->handler->bodyData(c->request, piece);
                if (!alive(id))
                    return false;
                if (r.status != 0) {
                    sendResponse(c, r);
                    return false;
                }
            } else {
                c->request.body.append(piece);
                if (c->request.body.size() > kMaxBodySize) {
                    sendResponse(c, HttpResponse::message(413, QLatin1String("Request too large")));
                    return false;
                }
            }
        }
        if (complete)
            break;
        if (piece.isEmpty())
            return false; // wait for more data
    }

    if (c->streaming && c->bodyBytes >= 1024 * 1024) {
        // To compare https with http on the phone.
        const qint64 ms = qMax(c->bodyTimer.elapsed(), qint64(1));
        qDebug() << "HTTP: received" << c->bodyBytes / 1024 << "KB in" << ms << "ms ="
                 << int(c->bodyBytes * 1000 / 1024 / ms) << "KB/s" << (c->tls ? "(https)" : "(http)");
    }
    c->phase = Connection::Waiting;
    c->buffer.clear();
    const HttpResponse r = c->handler->handleRequest(c->request);
    if (!alive(id))
        return false;
    if (r.status != 0)
        sendResponse(c, r);
    else
        qDebug() << "HTTP: request" << id << c->request.path << "will be answered later";
    return true;
}

// Takes the next piece of body data out of the buffer (possibly empty: more
// data needed). *complete once the whole body was taken. False if the
// chunked framing is broken.
bool HttpServer::takeBodyPiece(Connection *c, QByteArray *piece, bool *complete)
{
    *complete = false;
    if (!c->chunked) {
        if (c->remaining > 0 && !c->buffer.isEmpty()) {
            if (c->buffer.size() <= c->remaining) {
                *piece = c->buffer; // the usual case: no copy
                c->buffer.clear();
            } else {
                *piece = c->buffer.left(int(c->remaining));
                c->buffer.clear(); // one request per connection: the rest is junk
            }
            c->remaining -= piece->size();
        }
        *complete = c->remaining == 0;
        return true;
    }

    for (;;) {
        switch (c->chunkState) {
        case Connection::ChunkSize: {
            const int eol = c->buffer.indexOf("\r\n");
            if (eol < 0)
                return c->buffer.size() <= 1024; // a size line is short
            QByteArray line = c->buffer.left(eol);
            c->buffer.remove(0, eol + 2);
            const int ext = line.indexOf(';');
            if (ext >= 0)
                line.truncate(ext);
            bool ok = false;
            const qint64 size = line.trimmed().toLongLong(&ok, 16);
            if (!ok || size < 0)
                return false;
            if (size == 0) {
                c->chunkState = Connection::Trailer;
            } else {
                c->remaining = size;
                c->chunkState = Connection::ChunkData;
            }
            break;
        }
        case Connection::ChunkData: {
            if (c->buffer.isEmpty())
                return true;
            if (c->buffer.size() <= c->remaining) {
                *piece = c->buffer;
                c->buffer.clear();
            } else {
                *piece = c->buffer.left(int(c->remaining));
                c->buffer.remove(0, piece->size());
            }
            c->remaining -= piece->size();
            if (c->remaining == 0)
                c->chunkState = Connection::ChunkDataEnd;
            return true;
        }
        case Connection::ChunkDataEnd:
            if (c->buffer.size() < 2)
                return true;
            if (!c->buffer.startsWith("\r\n"))
                return false;
            c->buffer.remove(0, 2);
            c->chunkState = Connection::ChunkSize;
            break;
        case Connection::Trailer: {
            const int eol = c->buffer.indexOf("\r\n");
            if (eol < 0)
                return c->buffer.size() <= kMaxHeaderSize;
            c->buffer.remove(0, eol + 2);
            if (eol == 0) {
                *complete = true;
                return true;
            }
            break;
        }
        }
    }
}

static QByteArray reasonPhrase(int status)
{
    switch (status) {
    case 100: return "Continue";
    case 200: return "OK";
    case 204: return "No Content";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 409: return "Conflict";
    case 413: return "Payload Too Large";
    case 422: return "Unprocessable Entity";
    case 429: return "Too Many Requests";
    case 431: return "Request Header Fields Too Large";
    default: return status >= 500 ? "Internal Server Error" : "Unknown";
    }
}

void HttpServer::sendResponse(Connection *c, const HttpResponse &response)
{
    if (response.status != 200)
        qDebug() << "HTTP: answering" << response.status << "to" << c->request.method
                 << c->request.path;
    QByteArray out;
    out.append("HTTP/1.1 " + QByteArray::number(response.status) + " " + reasonPhrase(response.status) + "\r\n");
    if (!response.body.isEmpty())
        out.append("Content-Type: " + response.contentType + "\r\n");
    out.append("Content-Length: " + QByteArray::number(response.body.size()) + "\r\n");
    out.append("Connection: close\r\n\r\n");
    out.append(response.body);
    write(c, out);
    if (c->tls)
        c->socket->write(c->tls->closeNotify());
    // Ignore anything else this client sends; the connection is forgotten
    // once the client closes it (or after the idle timeout).
    c->phase = Connection::Answered;
    c->buffer.clear();
    c->lastActivity = QDateTime::currentDateTime();
    c->socket->disconnectFromHost();
}
