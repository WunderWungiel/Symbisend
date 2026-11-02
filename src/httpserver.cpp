#include "httpserver.h"

#include <QDebug>
#include <QTcpSocket>

static const int kMaxRequestSize = 64 * 1024;
// A client gets this long to send its request and to close the connection.
static const int kConnectionTimeoutMs = 10000;
static const int kIdleCheckIntervalMs = 2000;

HttpServer::HttpServer(HttpHandler *handler, QObject *parent)
    : QObject(parent), m_handler(handler)
{
    connect(&m_server, SIGNAL(newConnection()), this, SLOT(onNewConnection()));
    m_idleTimer.setInterval(kIdleCheckIntervalMs);
    connect(&m_idleTimer, SIGNAL(timeout()), this, SLOT(onIdleCheck()));
}

bool HttpServer::listen(quint16 port)
{
    if (!m_server.listen(QHostAddress::Any, port)) {
        qWarning() << "HTTP: cannot listen on port" << port << m_server.errorString();
        return false;
    }
    qDebug() << "HTTP: listening on port" << port;
    return true;
}

void HttpServer::close()
{
    if (m_server.isListening())
        qDebug() << "HTTP: server closed";
    m_server.close();
    m_idleTimer.stop();
    QList<QTcpSocket *> sockets = m_connectedAt.keys();
    m_buffers.clear();
    m_connectedAt.clear();
    foreach (QTcpSocket *socket, sockets) {
        socket->disconnect(this);
        socket->abort();
        socket->deleteLater();
    }
}

void HttpServer::onNewConnection()
{
    while (QTcpSocket *socket = m_server.nextPendingConnection()) {
        m_buffers.insert(socket, QByteArray());
        m_connectedAt.insert(socket, QDateTime::currentDateTime());
        connect(socket, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
        connect(socket, SIGNAL(disconnected()), this, SLOT(onDisconnected()));
    }
    if (!m_idleTimer.isActive())
        m_idleTimer.start();
}

void HttpServer::onDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket)
        return;
    m_buffers.remove(socket);
    m_connectedAt.remove(socket);
    socket->deleteLater();
}

// Drops clients that never finish their request or never close the
// connection, so they cannot pile up.
void HttpServer::onIdleCheck()
{
    const QDateTime now = QDateTime::currentDateTime();
    QList<QTcpSocket *> stale;
    for (QHash<QTcpSocket *, QDateTime>::const_iterator it = m_connectedAt.constBegin();
         it != m_connectedAt.constEnd(); ++it) {
        if (it.value().msecsTo(now) >= kConnectionTimeoutMs)
            stale.append(it.key());
    }
    foreach (QTcpSocket *socket, stale) {
        qDebug() << "HTTP: dropping idle connection from" << socket->peerAddress().toString();
        m_buffers.remove(socket);
        m_connectedAt.remove(socket);
        socket->disconnect(this);
        socket->abort();
        socket->deleteLater();
    }
    if (m_connectedAt.isEmpty())
        m_idleTimer.stop();
}

void HttpServer::onReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket || !m_buffers.contains(socket))
        return;
    m_buffers[socket].append(socket->readAll());
    if (m_buffers[socket].size() > kMaxRequestSize) {
        HttpResponse r;
        r.status = 413;
        r.body = "{\"message\":\"Request too large\"}";
        sendResponse(socket, r);
        return;
    }
    tryParse(socket);
}

bool HttpServer::tryParse(QTcpSocket *socket)
{
    QByteArray &buf = m_buffers[socket];
    int headerEnd = buf.indexOf("\r\n\r\n");
    if (headerEnd < 0)
        return false;

    HttpRequest req;
    req.remoteAddress = socket->peerAddress();
    QList<QByteArray> lines = buf.left(headerEnd).split('\n');
    QList<QByteArray> requestLine = lines.takeFirst().trimmed().split(' ');
    if (requestLine.size() < 2) {
        HttpResponse r;
        r.status = 400;
        sendResponse(socket, r);
        return true;
    }
    req.method = QString::fromLatin1(requestLine.at(0)).toUpper();
    QString target = QString::fromLatin1(requestLine.at(1));
    int q = target.indexOf(QLatin1Char('?'));
    req.path = q >= 0 ? target.left(q) : target;
    req.query = q >= 0 ? target.mid(q + 1) : QString();

    foreach (const QByteArray &line, lines) {
        int colon = line.indexOf(':');
        if (colon <= 0)
            continue;
        req.headers.insert(line.left(colon).trimmed().toLower(), line.mid(colon + 1).trimmed());
    }

    bool lengthOk = false;
    int contentLength = req.headers.value("content-length", "0").toInt(&lengthOk);
    if (!lengthOk || contentLength < 0 || contentLength > kMaxRequestSize) {
        HttpResponse r;
        r.status = 400;
        r.body = "{\"message\":\"Invalid Content-Length\"}";
        sendResponse(socket, r);
        return true;
    }
    if (buf.size() < headerEnd + 4 + contentLength)
        return false; // wait for the rest of the body
    req.body = buf.mid(headerEnd + 4, contentLength);

    qDebug() << "HTTP:" << req.method << req.path << "from" << req.remoteAddress.toString();
    HttpResponse response = m_handler->handleRequest(req);
    sendResponse(socket, response);
    return true;
}

static QByteArray reasonPhrase(int status)
{
    switch (status) {
    case 200: return "OK";
    case 204: return "No Content";
    case 400: return "Bad Request";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 413: return "Payload Too Large";
    default: return status >= 500 ? "Internal Server Error" : "Unknown";
    }
}

void HttpServer::sendResponse(QTcpSocket *socket, const HttpResponse &response)
{
    QByteArray out;
    out.append("HTTP/1.1 " + QByteArray::number(response.status) + " " + reasonPhrase(response.status) + "\r\n");
    if (!response.body.isEmpty())
        out.append("Content-Type: " + response.contentType + "\r\n");
    out.append("Content-Length: " + QByteArray::number(response.body.size()) + "\r\n");
    out.append("Connection: close\r\n\r\n");
    out.append(response.body);
    socket->write(out);
    // Stop parsing anything else this client sends; socket is deleted on disconnect.
    m_buffers.remove(socket);
    socket->disconnectFromHost();
}
