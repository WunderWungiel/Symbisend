// SPDX-FileCopyrightText: 2026 Wunder Wungiel
// SPDX-License-Identifier: GPL-3.0-or-later

#include "tlsserver.h"

#include <QDebug>
#include <QElapsedTimer>
#include <string.h>

#include "clientidentity.h"
#include "tlsrandom.h"
#include "verification.h"

extern "C" {
#include "mbedtls/error.h"
#include "mbedtls/md.h"
#include "mbedtls/pk.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"
}

// Plaintext read per mbedtls_ssl_read() call: one full TLS record.
static const int kReadChunk = 16384;
// Handshake steps run on the UI thread: the slow ones are worth a log line.
static const int kLogHandshakeStepMs = 20;

struct TlsServerConfigPrivate
{
    mbedtls_ssl_config conf;
    mbedtls_x509_crt cert;
    mbedtls_pk_context key;
    bool requireClientCert;
};

// mbedTLS parses PEM only from a NUL-terminated buffer whose length counts
// the NUL.
static QByteArray pemBuffer(const QByteArray &pem)
{
    QByteArray b = pem;
    if (!b.endsWith('\0'))
        b.append('\0');
    return b;
}

TlsServerConfig::TlsServerConfig()
    : d(new TlsServerConfigPrivate)
{
    mbedtls_ssl_config_init(&d->conf);
    mbedtls_x509_crt_init(&d->cert);
    mbedtls_pk_init(&d->key);
}

TlsServerConfig::~TlsServerConfig()
{
    mbedtls_ssl_config_free(&d->conf);
    mbedtls_x509_crt_free(&d->cert);
    mbedtls_pk_free(&d->key);
    delete d;
}

QSharedPointer<TlsServerConfig> TlsServerConfig::create(bool requireClientCert)
{
    QSharedPointer<TlsServerConfig> none;
    if (!TlsRandom::ready()) {
        qWarning() << "TLS: no random generator, cannot serve https";
        return none;
    }
    QByteArray certPem, keyPem;
    if (!ClientIdentity::serverIdentity(&certPem, &keyPem)) {
        qWarning() << "TLS: no certificate, cannot serve https";
        return none;
    }

    QSharedPointer<TlsServerConfig> config(new TlsServerConfig);
    TlsServerConfigPrivate *d = config->d;
    d->requireClientCert = requireClientCert;

    const QByteArray cert = pemBuffer(certPem);
    int ret = mbedtls_x509_crt_parse(&d->cert, reinterpret_cast<const unsigned char *>(cert.constData()),
                                     size_t(cert.size()));
    if (ret != 0) {
        qWarning() << "TLS: cannot read our certificate:" << TlsRandom::errorString(ret);
        return none;
    }
    // ClientIdentity wrote a PKCS#1 "BEGIN RSA PRIVATE KEY" without a password.
    const QByteArray key = pemBuffer(keyPem);
    ret = mbedtls_pk_parse_key(&d->key, reinterpret_cast<const unsigned char *>(key.constData()),
                               size_t(key.size()), 0, 0, TlsRandom::random, TlsRandom::context());
    if (ret != 0) {
        qWarning() << "TLS: cannot read our private key:" << TlsRandom::errorString(ret);
        return none;
    }

    ret = mbedtls_ssl_config_defaults(&d->conf, MBEDTLS_SSL_IS_SERVER, MBEDTLS_SSL_TRANSPORT_STREAM,
                                      MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) {
        qWarning() << "TLS: mbedtls_ssl_config_defaults failed:" << TlsRandom::errorString(ret);
        return none;
    }
    // Ask every client for its certificate, like the official app: over https
    // a LocalSend device's identity is the hash of it. OPTIONAL, not REQUIRED:
    // REQUIRED fails without a CA chain, and there is none (all LocalSend
    // certificates are self-signed). TlsSession checks the certificate itself
    // after the handshake, and rejects a client without one if required.
    mbedtls_ssl_conf_authmode(&d->conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
    mbedtls_ssl_conf_min_tls_version(&d->conf, MBEDTLS_SSL_VERSION_TLS1_2);
    mbedtls_ssl_conf_max_tls_version(&d->conf, MBEDTLS_SSL_VERSION_TLS1_2);
    mbedtls_ssl_conf_rng(&d->conf, TlsRandom::random, TlsRandom::context());
    ret = mbedtls_ssl_conf_own_cert(&d->conf, &d->cert, &d->key);
    if (ret != 0) {
        qWarning() << "TLS: mbedtls_ssl_conf_own_cert failed:" << TlsRandom::errorString(ret);
        return none;
    }
    return config;
}

struct TlsSessionPrivate
{
    QSharedPointer<TlsServerConfig> config;
    mbedtls_ssl_context ssl;
    QByteArray in;    // received ciphertext; mbedTLS has read up to inPos
    int inPos;
    QByteArray out;   // ciphertext for the socket
    bool setupOk;
    bool handshakeDone;
    bool failed;
    bool peerClosed;
    QString error;
    QString peerFingerprint;
    bool requireClientCert; // from the config, which only TlsSession may read

    void fail(const char *what, int code)
    {
        failed = true;
        error = QString::fromLatin1("%1: %2").arg(QLatin1String(what)).arg(TlsRandom::errorString(code));
    }
};

extern "C" {

static int tlsSend(void *ctx, const unsigned char *buf, size_t len)
{
    TlsSessionPrivate *d = static_cast<TlsSessionPrivate *>(ctx);
    d->out.append(reinterpret_cast<const char *>(buf), int(len));
    return int(len);
}

static int tlsRecv(void *ctx, unsigned char *buf, size_t len)
{
    TlsSessionPrivate *d = static_cast<TlsSessionPrivate *>(ctx);
    const int available = d->in.size() - d->inPos;
    if (available <= 0)
        return MBEDTLS_ERR_SSL_WANT_READ;
    const int n = qMin(available, int(len));
    memcpy(buf, d->in.constData() + d->inPos, size_t(n));
    d->inPos += n;
    return n;
}

} // extern "C"

// The official app (crypto/cert.rs, verify_cert_from_der) accepts a client
// certificate that is in its validity period and correctly self-signed.
static QString certificateProblem(const mbedtls_x509_crt *crt)
{
    // sig_md, sig_pk, sig_opts and sig are private in mbedTLS 3 (no getters);
    // MBEDTLS_PRIVATE() is its own way of reaching them.
    if (mbedtls_x509_time_is_past(&crt->valid_to))
        return QLatin1String("certificate expired");
    if (mbedtls_x509_time_is_future(&crt->valid_from))
        return QLatin1String("certificate not yet valid");
    const mbedtls_md_info_t *md = mbedtls_md_info_from_type(crt->MBEDTLS_PRIVATE(sig_md));
    unsigned char hash[MBEDTLS_MD_MAX_SIZE];
    if (!md || mbedtls_md(md, crt->tbs.p, crt->tbs.len, hash) != 0)
        return QLatin1String("unsupported signature hash");
    const int ret = mbedtls_pk_verify_ext(crt->MBEDTLS_PRIVATE(sig_pk), crt->MBEDTLS_PRIVATE(sig_opts),
                                          const_cast<mbedtls_pk_context *>(&crt->pk),
                                          crt->MBEDTLS_PRIVATE(sig_md), hash, mbedtls_md_get_size(md),
                                          crt->MBEDTLS_PRIVATE(sig).p, crt->MBEDTLS_PRIVATE(sig).len);
    if (ret != 0)
        return QLatin1String("bad self-signature: ") + TlsRandom::errorString(ret);
    return QString();
}

// Right after the handshake: take the client's certificate fingerprint, or
// refuse the connection with an alert (no certificate while one is required,
// or an invalid one).
static bool checkPeer(TlsSessionPrivate *d)
{
    const mbedtls_x509_crt *crt = mbedtls_ssl_get_peer_cert(&d->ssl);
    if (!crt) {
        if (!d->requireClientCert)
            return true;
        mbedtls_ssl_send_alert_message(&d->ssl, MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                                       MBEDTLS_SSL_ALERT_MSG_CERT_REQUIRED);
        d->failed = true;
        d->error = QLatin1String("client sent no certificate");
        return false;
    }
    const QString problem = certificateProblem(crt);
    if (!problem.isEmpty()) {
        mbedtls_ssl_send_alert_message(&d->ssl, MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                                       MBEDTLS_SSL_ALERT_MSG_BAD_CERT);
        d->failed = true;
        d->error = QLatin1String("client certificate rejected: ") + problem;
        return false;
    }
    d->peerFingerprint = Verification::fingerprintOfDer(
                QByteArray(reinterpret_cast<const char *>(crt->raw.p), int(crt->raw.len)));
    return true;
}

TlsSession::TlsSession(const QSharedPointer<TlsServerConfig> &config)
    : d(new TlsSessionPrivate)
{
    d->config = config;
    d->inPos = 0;
    d->setupOk = false;
    d->handshakeDone = false;
    d->failed = false;
    d->peerClosed = false;
    d->requireClientCert = config->d->requireClientCert;
    mbedtls_ssl_init(&d->ssl);
    const int ret = mbedtls_ssl_setup(&d->ssl, &config->d->conf);
    if (ret != 0) {
        d->fail("mbedtls_ssl_setup", ret); // usually out of memory
        return;
    }
    d->setupOk = true;
    mbedtls_ssl_set_bio(&d->ssl, d, tlsSend, tlsRecv, 0);
}

TlsSession::~TlsSession()
{
    mbedtls_ssl_free(&d->ssl);
    delete d;
}

QByteArray TlsSession::feed(const QByteArray &ciphertext)
{
    QByteArray plain;
    if (d->failed || d->peerClosed)
        return plain;
    if (d->inPos >= d->in.size()) {
        d->in = ciphertext; // the usual case: everything was consumed, no copy
        d->inPos = 0;
    } else {
        d->in.append(ciphertext);
    }

    if (!d->handshakeDone) {
        QElapsedTimer timer;
        timer.start();
        const int ret = mbedtls_ssl_handshake(&d->ssl);
        const qint64 ms = timer.elapsed();
        if (ms >= kLogHandshakeStepMs)
            qDebug() << "TLS: handshake step took" << ms << "ms";
        if (ret == 0) {
            if (!checkPeer(d))
                return plain;
            d->handshakeDone = true;
        } else if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            d->fail("handshake", ret);
            return plain;
        }
    }

    while (d->handshakeDone) {
        const int old = plain.size();
        plain.resize(old + kReadChunk);
        const int ret = mbedtls_ssl_read(&d->ssl, reinterpret_cast<unsigned char *>(plain.data() + old),
                                         size_t(kReadChunk));
        plain.resize(old + qMax(ret, 0));
        if (ret > 0)
            continue;
        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
            break;
        if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || ret == 0) {
            d->peerClosed = true;
            break;
        }
        d->fail("read", ret);
        break;
    }

    // Keep only what mbedTLS has not read yet.
    if (d->inPos >= d->in.size()) {
        d->in.clear();
        d->inPos = 0;
    } else if (d->inPos > 0) {
        d->in.remove(0, d->inPos);
        d->inPos = 0;
    }
    return plain;
}

QByteArray TlsSession::takeOutgoing()
{
    QByteArray o = d->out;
    d->out.clear();
    return o;
}

QByteArray TlsSession::encrypt(const QByteArray &plaintext)
{
    if (!d->handshakeDone || d->failed)
        return QByteArray();
    int pos = 0;
    while (pos < plaintext.size()) {
        // Writes at most one record (MBEDTLS_SSL_OUT_CONTENT_LEN) per call.
        const int ret = mbedtls_ssl_write(&d->ssl,
                                          reinterpret_cast<const unsigned char *>(plaintext.constData() + pos),
                                          size_t(plaintext.size() - pos));
        // tlsSend() never blocks, so anything else (WANT_* included) is an error.
        if (ret <= 0) {
            d->fail("write", ret);
            break;
        }
        pos += ret;
    }
    return takeOutgoing();
}

QByteArray TlsSession::closeNotify()
{
    if (d->handshakeDone && !d->failed)
        mbedtls_ssl_close_notify(&d->ssl);
    return takeOutgoing();
}

bool TlsSession::handshakeDone() const { return d->handshakeDone; }
bool TlsSession::failed() const { return d->failed; }
bool TlsSession::peerClosed() const { return d->peerClosed; }
QString TlsSession::errorString() const { return d->error; }
QString TlsSession::peerFingerprint() const { return d->peerFingerprint; }

QString TlsSession::version() const
{
    return d->setupOk ? QString::fromLatin1(mbedtls_ssl_get_version(&d->ssl)) : QString();
}

QString TlsSession::ciphersuite() const
{
    return d->setupOk ? QString::fromLatin1(mbedtls_ssl_get_ciphersuite(&d->ssl)) : QString();
}
