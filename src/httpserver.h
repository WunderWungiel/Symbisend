#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include <QObject>
#include <QDateTime>
#include <QHash>
#include <QHostAddress>
#include <QList>
#include <QSharedPointer>
#include <QTcpServer>
#include <QTimer>

class QTcpSocket;
class TlsServerConfig;

struct HttpRequest
{
    HttpRequest() : id(0) {}
    // Identifies the request (= its connection) for HttpServer::respond()/drop().
    quint32 id;
    QString method;
    QString path;      // without query string
    QString query;
    QHash<QByteArray, QByteArray> headers; // lower-case names
    QByteArray body;   // empty for streamed bodies
    QHostAddress remoteAddress;
    // https: the SHA-256 (uppercase hex) of the client's certificate, which
    // TLS has proven it holds; over https this, not the fingerprint in a
    // JSON body, is the client's identity. Empty for http, or for a client
    // without a certificate (browsers, while not required).
    QString peerFingerprint;

    // Percent-decoded value of a query parameter ("" if missing). present, if
    // given, tells a missing parameter from an empty one.
    QString queryValue(const QString &key, bool *present = 0) const;
};

struct HttpResponse
{
    // Status 0 means "no answer yet" (see HttpHandler).
    explicit HttpResponse(int status = 200, const QByteArray &body = QByteArray())
        : status(status), contentType("application/json"), body(body) {}
    // {"message": text} with that status.
    static HttpResponse message(int status, const QString &text);

    int status;
    QByteArray contentType;
    QByteArray body;
};

// Serves one group of routes. A handler may answer later (status 0 from
// handleRequest(), then HttpServer::respond()), and may take the body in
// pieces as it arrives instead of buffered (streamsBody()), for file uploads.
class HttpHandler
{
public:
    virtual ~HttpHandler() {}

    // Whether this handler serves the request (headers known, body not yet).
    virtual bool handles(const HttpRequest &request) const = 0;

    // Whether the body goes to bodyData() in pieces instead of being buffered
    // (at most 1 MB) into request.body.
    virtual bool streamsBody(const HttpRequest &request) const { Q_UNUSED(request); return false; }

    // Streamed bodies only: called once right after the headers with empty
    // data (to accept or reject the request before any of the body is read),
    // then for every piece. Return status 0 to go on; any other response is
    // sent at once and the rest of the body is ignored.
    virtual HttpResponse bodyData(const HttpRequest &request, const QByteArray &data)
    { Q_UNUSED(request); Q_UNUSED(data); return HttpResponse(0); }

    // The complete request (request.body is empty if it was streamed).
    // Return the answer, or status 0 and answer later with respond().
    virtual HttpResponse handleRequest(const HttpRequest &request) = 0;

    // A request this handler hasn't answered yet (body still streaming, or
    // answer pending) lost its connection: closed by the client, timed out,
    // or HttpServer::close(). respond() for it does nothing any more.
    virtual void requestAborted(const HttpRequest &request) { Q_UNUSED(request); }
};

// Very small HTTP/1.1 server: one request per connection, Content-Length
// or chunked bodies. Headers up to 16 KB; buffered bodies up to 1 MB,
// streamed ones (see HttpHandler::streamsBody()) of any size. Serves 64
// connections at once, 8 per IP; further ones are closed right away.
//
// Speaks plain http or https (TLS 1.2 via mbedTLS, see src/tlsserver.h, with
// our certificate from ClientIdentity; clients are asked for theirs, see
// HttpRequest::peerFingerprint).
// Handlers see no difference.
class HttpServer : public QObject
{
    Q_OBJECT
public:
    explicit HttpServer(QObject *parent = 0);
    ~HttpServer();

    // Handlers are asked in the order added; nobody serving a path = 404.
    void addHandler(HttpHandler *handler);

    // If https is asked for but impossible (no certificate, or mbedTLS
    // rejects it), listens with plain http instead; isHttps() tells which.
    // requireClientCert (https only): refuse clients without a certificate,
    // like the official app unless browsers must connect.
    bool listen(quint16 port, bool https, bool requireClientCert);
    // Stops listening and drops all client connections (unanswered requests
    // are reported to their handlers as aborted).
    void close();
    bool isListening() const { return m_server.isListening(); }
    // Tries to connect to our own port at this address. Qt 4 doesn't report a
    // listening socket that died, so this is how to find out; a failure gives
    // probeFailed(). Does nothing while clients are connected, which shows
    // that it works, and whose TLS handshakes could delay the answer.
    void probe(const QHostAddress &address);
    // Whether the server (while listening) speaks https.
    bool isHttps() const { return !m_tlsConfig.isNull(); }
    // Whether the running https server refuses clients without a certificate.
    bool clientCertRequired() const { return isHttps() && m_requireClientCert; }

    // Answers a request whose handler returned status 0. Does nothing if the
    // request was answered already or its connection is gone.
    void respond(quint32 requestId, const HttpResponse &response);
    // Closes a request's connection without an answer (and without
    // requestAborted()).
    void drop(quint32 requestId);

signals:
    // listen() succeeded.
    void started();
    // See probe().
    void probeFailed();

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();
    void onIdleCheck();
    void onProbeConnected();
    void onProbeError();
    void onProbeTimeout();

private:
    struct Connection;

    // Writes to the client, through TLS on https connections.
    void write(Connection *c, const QByteArray &data);
    void process(Connection *c);
    bool parseHeaders(Connection *c);
    bool readBody(Connection *c);
    bool takeBodyPiece(Connection *c, QByteArray *piece, bool *complete);
    void sendResponse(Connection *c, const HttpResponse &response);
    void remove(Connection *c, bool notifyHandler);
    bool alive(quint32 id) const { return m_byId.contains(id); }
    bool mayConnect(const QHostAddress &peer);
    void endProbe();

    QTcpServer m_server;
    // Set while listening with https; every connection gets a TlsSession on it.
    QSharedPointer<TlsServerConfig> m_tlsConfig;
    bool m_requireClientCert;
    QList<HttpHandler *> m_handlers;
    QHash<QTcpSocket *, Connection *> m_bySocket;
    QHash<quint32, Connection *> m_byId;
    quint32 m_nextId;
    QTimer m_idleTimer;
    bool m_refusing;     // refusals are logged once until a connection is accepted
    QTcpSocket *m_probe; // while probe() runs
    QTimer m_probeTimer;
};

#endif // HTTPSERVER_H
