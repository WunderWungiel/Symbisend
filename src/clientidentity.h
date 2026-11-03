#ifndef CLIENTIDENTITY_H
#define CLIENTIDENTITY_H

#include <QByteArray>
#include <QList>
#include <QString>

class QNetworkReply;
class QSslError;

// Our TLS client certificate.
//
// The official LocalSend server (Rust, rustls) requires mutual TLS: a client
// without a certificate is rejected during the handshake with the TLS 1.3
// alert "certificate_required" (OpenSSL error 1409445C, reason 1116). The
// certificate is also our identity: the server accepts a /register only if
// info.fingerprint equals the SHA-256 of the client certificate (DER,
// uppercase hex), and reports that hash for /prepare-upload.
//
// install() loads the certificate and key from QSettings, creating them on
// first use (RSA-2048, self-signed, CN "LocalSend User", like the official
// app; this takes a few seconds on a phone), and makes them the local
// certificate of QSslConfiguration::defaultConfiguration(), so every HTTPS
// request and QSslSocket presents it. Built with SYMBISEND_CLIENT_CERT (which
// links libcrypto); otherwise it does nothing.
namespace ClientIdentity {
    // Returns the certificate's fingerprint, or an empty string if there is
    // no certificate (then HTTPS peers running the Rust server reject us).
    QString install();
    // The stored certificate and private key (PEM) for our https server
    // (src/tlsserver.cpp, mbedTLS), so peers see the certificate our
    // fingerprint is the hash of. Read from QSettings on every platform: on
    // a desktop build without SYMBISEND_CLIENT_CERT a test pair can be put
    // under identity/certificate and identity/privateKey by hand. False if
    // there is none.
    bool serverIdentity(QByteArray *certPem, QByteArray *keyPem);

#ifndef QT_NO_OPENSSL
    // For a sslErrors() slot, before ignoreSslErrors(): whether the peer's
    // certificate is acceptable. Like the official app it must be in its
    // validity period and correctly signed; with expectedFingerprint set
    // (pinning, as for every request to a device we already know) its SHA-256
    // must also be that one. If not, *why says why: don't ignore the errors,
    // the handshake then fails and the request is never sent.
    bool acceptPeer(const QList<QSslError> &errors, const QString &expectedFingerprint, QString *why);
    // The fingerprint of the certificate a finished https reply came over
    // (empty if none). Checked again after the reply because QNetworkAccessManager
    // reuses kept-alive connections without a new sslErrors().
    QString peerFingerprint(const QNetworkReply *reply);
#endif
}

#endif // CLIENTIDENTITY_H
