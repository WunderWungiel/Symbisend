#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include <QObject>
#include <QDateTime>
#include <QHash>
#include <QHostAddress>
#include <QTcpServer>
#include <QTimer>

class QTcpSocket;

struct HttpRequest
{
    QString method;
    QString path;      // without query string
    QString query;
    QHash<QByteArray, QByteArray> headers; // lower-case names
    QByteArray body;
    QHostAddress remoteAddress;
};

struct HttpResponse
{
    HttpResponse() : status(200), contentType("application/json") {}
    int status;
    QByteArray contentType;
    QByteArray body;
};

class HttpHandler
{
public:
    virtual ~HttpHandler() {}
    virtual HttpResponse handleRequest(const HttpRequest &request) = 0;
};

// Very small HTTP/1.1 server: one request per connection, Content-Length
// bodies only (enough for LocalSend's JSON API endpoints).
class HttpServer : public QObject
{
    Q_OBJECT
public:
    explicit HttpServer(HttpHandler *handler, QObject *parent = 0);

    bool listen(quint16 port);
    // Stops listening and drops all client connections.
    void close();
    bool isListening() const { return m_server.isListening(); }

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();
    void onIdleCheck();

private:
    bool tryParse(QTcpSocket *socket);
    void sendResponse(QTcpSocket *socket, const HttpResponse &response);

    QTcpServer m_server;
    HttpHandler *m_handler;
    QHash<QTcpSocket *, QByteArray> m_buffers;     // requests still being received
    QHash<QTcpSocket *, QDateTime> m_connectedAt;  // every open client connection
    QTimer m_idleTimer;
};

#endif // HTTPSERVER_H
