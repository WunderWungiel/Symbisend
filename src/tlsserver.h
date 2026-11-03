#ifndef TLSSERVER_H
#define TLSSERVER_H

#include <QByteArray>
#include <QSharedPointer>
#include <QString>

// The server side of TLS for HttpServer, on the mbedTLS compiled into the app
// (the phone's OpenSSL 0.9.8 has no TLS 1.2 / ECDHE, which rustls and current
// browsers require). TLS 1.2 only, ECDHE-RSA with AES-GCM or ChaCha20-Poly1305.
//
// Every client is asked for its certificate (a LocalSend device's identity
// over https is that certificate's SHA-256, see peerFingerprint()). As in the
// official app it is mandatory unless browsers have to connect ("Receive via
// link"), and one that is sent must be in its validity period and correctly
// self-signed.
//
// mbedTLS never touches the socket: TlsSession works on byte buffers, and
// HttpServer moves them between the session and its QTcpSocket.

struct TlsServerConfigPrivate;

// One per server start: our certificate and key, and the TLS settings.
// Shared by all sessions of that server (a session keeps it alive).
class TlsServerConfig
{
public:
    // Loads the certificate and key from ClientIdentity::serverIdentity().
    // Returns null (and logs why) if there is none or mbedTLS rejects it.
    // requireClientCert: refuse clients without a certificate (alert
    // certificate_required right after the handshake).
    static QSharedPointer<TlsServerConfig> create(bool requireClientCert);
    ~TlsServerConfig();

private:
    TlsServerConfig();
    Q_DISABLE_COPY(TlsServerConfig)
    friend class TlsSession;
    TlsServerConfigPrivate *d;
};

struct TlsSessionPrivate;

// One per connection.
class TlsSession
{
public:
    explicit TlsSession(const QSharedPointer<TlsServerConfig> &config);
    ~TlsSession();

    // Takes bytes received from the socket; runs the handshake and returns
    // the application data decrypted so far (often empty). Afterwards check
    // failed(), and write takeOutgoing() to the socket.
    QByteArray feed(const QByteArray &ciphertext);
    // Bytes to write to the socket (handshake records, alerts).
    QByteArray takeOutgoing();
    // Encrypts application data; the result goes to the socket. Empty unless
    // the handshake is done.
    QByteArray encrypt(const QByteArray &plaintext);
    // The close_notify alert, to send before closing the connection.
    QByteArray closeNotify();

    bool handshakeDone() const;
    // A fatal error: the connection has to be dropped.
    bool failed() const;
    // The client sent close_notify (a normal end, not an error).
    bool peerClosed() const;
    QString errorString() const;
    // The SHA-256 (uppercase hex) of the client's certificate, after the
    // handshake. Empty if it sent none (only possible if not required).
    QString peerFingerprint() const;
    // "TLSv1.2", "TLS-ECDHE-RSA-WITH-AES-128-GCM-SHA256" (after the handshake).
    QString version() const;
    QString ciphersuite() const;

private:
    Q_DISABLE_COPY(TlsSession)
    TlsSessionPrivate *d;
};

#endif // TLSSERVER_H
