#ifndef CLIENTIDENTITY_H
#define CLIENTIDENTITY_H

#include <QByteArray>
#include <QString>

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
}

#endif // CLIENTIDENTITY_H
