#include "tlsserver.h"

#include <QDebug>
#include <string.h>

#include "clientidentity.h"
#include "tlsrandom.h"

// Included last: on Symbian, mbedtls/build_info.h undefines EXPORT_C and
// IMPORT_C (MBEDTLS_STATIC) for the rest of the translation unit.
extern "C" {
#include "mbedtls/error.h"
#include "mbedtls/pk.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"
}

// Plaintext read per mbedtls_ssl_read() call: one full TLS record.
static const int kReadChunk = 16384;

struct TlsServerConfigPrivate
{
    mbedtls_ssl_config conf;
    mbedtls_x509_crt cert;
    mbedtls_pk_context key;
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

QSharedPointer<TlsServerConfig> TlsServerConfig::create()
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

    const QByteArray cert = pemBuffer(certPem);
    int ret = mbedtls_x509_crt_parse(&d->cert, reinterpret_cast<const unsigned char *>(cert.constData()),
                                     size_t(cert.size()));
    if (ret != 0) {
        qWarning() << "TLS: cannot read our certificate:" << TlsRandom::errorString(ret);
        return none;
    }
    // libcrypto wrote a PKCS#1 "BEGIN RSA PRIVATE KEY" without a password.
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
    // No client certificate is asked for, so browsers don't prompt for one.
    // (LocalSend clients send theirs only when asked; we don't check it.)
    mbedtls_ssl_conf_authmode(&d->conf, MBEDTLS_SSL_VERIFY_NONE);
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

TlsSession::TlsSession(const QSharedPointer<TlsServerConfig> &config)
    : d(new TlsSessionPrivate)
{
    d->config = config;
    d->inPos = 0;
    d->setupOk = false;
    d->handshakeDone = false;
    d->failed = false;
    d->peerClosed = false;
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
        const int ret = mbedtls_ssl_handshake(&d->ssl);
        if (ret == 0) {
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

QString TlsSession::version() const
{
    return d->setupOk ? QString::fromLatin1(mbedtls_ssl_get_version(&d->ssl)) : QString();
}

QString TlsSession::ciphersuite() const
{
    return d->setupOk ? QString::fromLatin1(mbedtls_ssl_get_ciphersuite(&d->ssl)) : QString();
}
