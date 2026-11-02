#include "httpserver.h"

#include <QDebug>
#include <QTcpSocket>

static const int kMaxRequestSize = 64 * 1024;

HttpServer::HttpServer(HttpHandler *handler, QObject *parent)
    : QObject(parent), m_handler(handler)
{
    connect(&m_server, SIGNAL(newConnection()), this, SLOT(onNewConnection()));
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
    QList<QTcpSocket *> sockets = m_buffers.keys();
    m_buffers.clear();
    foreach (QTcpSocket *socket, sockets)
        socket->abort(); // deleted via disconnected() -> onDisconnected()
}

void HttpServer::onNewConnection()
{
    while (QTcpSocket *socket = m_server.nextPendingConnection()) {
        m_buffers.insert(socket, QByteArray());
        connect(socket, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
        connect(socket, SIGNAL(disconnected()), this, SLOT(onDisconnected()));
    }
}

void HttpServer::onDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket)
        return;
    m_buffers.remove(socket);
    socket->deleteLater();
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

    int contentLength = req.headers.value("content-length", "0").toInt();
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
