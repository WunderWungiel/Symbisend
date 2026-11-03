#include "clientidentity.h"

#include <QByteArray>
#include <QDebug>
#include <QSettings>
#include <QTime>

static const char kCertKey[] = "identity/certificate";
static const char kPrivateKeyKey[] = "identity/privateKey";

bool ClientIdentity::serverIdentity(QByteArray *certPem, QByteArray *keyPem)
{
    QSettings settings;
    *certPem = settings.value(QLatin1String(kCertKey)).toByteArray();
    *keyPem = settings.value(QLatin1String(kPrivateKeyKey)).toByteArray();
    return !certPem->isEmpty() && !keyPem->isEmpty();
}

#if defined(SYMBISEND_CLIENT_CERT) && !defined(QT_NO_OPENSSL)
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslKey>

// The OpenSSL headers of the Symbian SDK are 0.9.8g; the phone runs the
// patched (1.0.x) library. Only functions exported by both are used, and no
// structs are touched except through the X509_get_notBefore/notAfter macros,
// whose layout is the same in both.
#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>
#include <openssl/x509.h>

static QString opensslError()
{
    const unsigned long e = ERR_get_error();
    if (!e)
        return QLatin1String("(no OpenSSL error)");
    char buf[256];
    ERR_error_string_n(e, buf, sizeof(buf));
    return QString::fromLatin1(buf);
}

static QByteArray bioContents(BIO *bio)
{
    char *data = 0;
    const long n = BIO_get_mem_data(bio, &data);
    return n > 0 && data ? QByteArray(data, int(n)) : QByteArray();
}

static bool generate(QByteArray *certPem, QByteArray *keyPem)
{
    QTime timer;
    timer.start();
    qDebug() << "ClientIdentity: creating a certificate with" << SSLeay_version(SSLEAY_VERSION)
             << "- RAND_status" << RAND_status();

    bool ok = false;
    BIGNUM *e = BN_new();
    RSA *rsa = RSA_new();
    bool rsaOwnedByPkey = false;
    EVP_PKEY *pkey = EVP_PKEY_new();
    X509 *x = X509_new();
    BIO *certBio = BIO_new(BIO_s_mem());
    BIO *keyBio = BIO_new(BIO_s_mem());
    do {
        if (!e || !rsa || !pkey || !x || !certBio || !keyBio)
            break;
        if (!BN_set_word(e, RSA_F4) || !RSA_generate_key_ex(rsa, 2048, e, 0))
            break;
        qDebug() << "ClientIdentity: RSA key generated in" << timer.elapsed() << "ms";
        if (!PEM_write_bio_RSAPrivateKey(keyBio, rsa, 0, 0, 0, 0, 0))
            break;
        if (!EVP_PKEY_assign_RSA(pkey, rsa))
            break;
        rsaOwnedByPkey = true;

        if (!X509_set_version(x, 2)) // X.509 v3: rustls/webpki reject v1
            break;
        unsigned char rnd[4] = { 0, 0, 0, 0 };
        RAND_bytes(rnd, sizeof(rnd));
        const long serial = long(((unsigned long)(rnd[0] & 0x7f) << 24) | (rnd[1] << 16)
                                 | (rnd[2] << 8) | rnd[3]) | 1;
        if (!ASN1_INTEGER_set(X509_get_serialNumber(x), serial))
            break;
        // Fixed, wide validity: neither the phone's nor the receiver's clock
        // can make it "not yet valid" or expired (the server checks it).
        if (!ASN1_UTCTIME_set_string(X509_get_notBefore(x), "000101000000Z")
                || !ASN1_GENERALIZEDTIME_set_string(X509_get_notAfter(x), "40960101000000Z"))
            break;
        if (!X509_set_pubkey(x, pkey))
            break;
        X509_NAME *name = X509_get_subject_name(x);
        if (!X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                                        (const unsigned char *)"LocalSend User", -1, -1, 0))
            break;
        if (!X509_set_issuer_name(x, name))
            break;
        if (!X509_sign(x, pkey, EVP_sha256()))
            break;
        if (!PEM_write_bio_X509(certBio, x))
            break;
        *certPem = bioContents(certBio);
        *keyPem = bioContents(keyBio);
        ok = !certPem->isEmpty() && !keyPem->isEmpty();
    } while (false);

    if (ok)
        qDebug() << "ClientIdentity: certificate created in" << timer.elapsed() << "ms";
    else
        qWarning() << "ClientIdentity: creating the certificate failed:" << opensslError();

    if (keyBio) BIO_free(keyBio);
    if (certBio) BIO_free(certBio);
    if (x) X509_free(x);
    if (pkey) EVP_PKEY_free(pkey);
    if (rsa && !rsaOwnedByPkey) RSA_free(rsa);
    if (e) BN_free(e);
    return ok;
}

static QString fingerprintOf(const QSslCertificate &cert)
{
    // QCryptographicHash has no SHA-256 in Qt 4.
    const QByteArray der = cert.toDer();
    unsigned char md[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char *>(der.constData()), der.size(), md);
    return QString::fromLatin1(
                QByteArray(reinterpret_cast<const char *>(md), SHA256_DIGEST_LENGTH).toHex()).toUpper();
}

QString ClientIdentity::install()
{
    QSettings settings;
    QByteArray certPem = settings.value(QLatin1String(kCertKey)).toByteArray();
    QByteArray keyPem = settings.value(QLatin1String(kPrivateKeyKey)).toByteArray();

    QSslCertificate cert(certPem, QSsl::Pem);
    QSslKey key(keyPem, QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey);
    if (cert.isNull() || key.isNull()) {
        if (!certPem.isEmpty())
            qWarning() << "ClientIdentity: stored certificate is unusable, creating a new one";
        if (!generate(&certPem, &keyPem))
            return QString();
        cert = QSslCertificate(certPem, QSsl::Pem);
        key = QSslKey(keyPem, QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey);
        if (cert.isNull() || key.isNull()) {
            qWarning() << "ClientIdentity: Qt cannot read the new certificate"
                       << "(cert null" << cert.isNull() << ", key null" << key.isNull() << ")";
            return QString();
        }
        settings.setValue(QLatin1String(kCertKey), certPem);
        settings.setValue(QLatin1String(kPrivateKeyKey), keyPem);
    }

    QSslConfiguration config = QSslConfiguration::defaultConfiguration();
    config.setLocalCertificate(cert);
    config.setPrivateKey(key);
    QSslConfiguration::setDefaultConfiguration(config);

    const QString fingerprint = fingerprintOf(cert);
    qDebug() << "ClientIdentity: client certificate installed, fingerprint" << fingerprint;
    return fingerprint;
}

#else // no client certificate support

QString ClientIdentity::install()
{
    qDebug() << "ClientIdentity: built without client certificate support";
    return QString();
}

#endif
