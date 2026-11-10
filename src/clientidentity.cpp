// SPDX-FileCopyrightText: 2026 Wunder Wungiel
// SPDX-License-Identifier: GPL-3.0-or-later

#include "clientidentity.h"

#include <QByteArray>
#include <QDebug>
#include <QSettings>
#include <QTime>

#include "verification.h"

#ifndef QT_NO_OPENSSL
#include <QNetworkReply>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslError>
#endif

static const char kCertKey[] = "identity/certificate";
static const char kPrivateKeyKey[] = "identity/privateKey";

bool ClientIdentity::serverIdentity(QByteArray *certPem, QByteArray *keyPem)
{
    QSettings settings;
    *certPem = settings.value(QLatin1String(kCertKey)).toByteArray();
    *keyPem = settings.value(QLatin1String(kPrivateKeyKey)).toByteArray();
    return !certPem->isEmpty() && !keyPem->isEmpty();
}

#ifndef QT_NO_OPENSSL
bool ClientIdentity::acceptPeer(const QList<QSslError> &errors, const QString &expectedFingerprint,
                                QString *why)
{
    // Like the official app's verify_cert_from_der: the certificate must be in
    // its validity period and correctly self-signed. That it is self-signed,
    // untrusted, or not issued for the IP is normal for LocalSend.
    QSslCertificate cert;
    foreach (const QSslError &e, errors) {
        switch (e.error()) {
        case QSslError::CertificateSignatureFailed:
        case QSslError::CertificateNotYetValid:
        case QSslError::CertificateExpired:
            *why = e.errorString();
            return false;
        default:
            break;
        }
        if (cert.isNull() && !e.certificate().isNull())
            cert = e.certificate();
    }
    if (expectedFingerprint.isEmpty())
        return true;
    if (cert.isNull()) {
        *why = QLatin1String("no certificate to check the fingerprint against");
        return false;
    }
    const QString actual = Verification::fingerprintOfDer(cert.toDer());
    if (actual != expectedFingerprint.toUpper()) {
        *why = QString::fromLatin1("certificate fingerprint mismatch: expected %1, got %2")
                .arg(expectedFingerprint.toUpper(), actual);
        return false;
    }
    return true;
}

QString ClientIdentity::peerFingerprint(const QNetworkReply *reply)
{
    return Verification::fingerprintOfDer(reply->sslConfiguration().peerCertificate().toDer());
}
#endif

#if defined(SYMBISEND_CLIENT_CERT) && !defined(QT_NO_OPENSSL)
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslKey>

#include "tlsrandom.h"

// Created with mbedTLS (compiled into the app), not the system libcrypto: Open C's
// libcrypto differs between firmwares, and an export the phone's copy lacks
// (EVP_sha256 is ordinal 2631, near the end of the SDK's table) makes the
// loader refuse to start the app with KErrNotSupported ("Feature not supported").
extern "C" {
#include <mbedtls/pk.h>
#include <mbedtls/rsa.h>
#include <mbedtls/x509_crt.h>
}

static bool generate(QByteArray *certPem, QByteArray *keyPem)
{
    QTime timer;
    timer.start();
    if (!TlsRandom::init()) {
        qWarning() << "ClientIdentity: no random number generator, cannot create a certificate";
        return false;
    }
    qDebug() << "ClientIdentity: creating a certificate with" << TlsRandom::versionString();

    int ret = 0;
    const char *step = "";
    mbedtls_pk_context key;
    mbedtls_x509write_cert crt;
    mbedtls_pk_init(&key);
    mbedtls_x509write_crt_init(&crt);
    // PEM of an RSA-2048 key is about 1.7 kB, of the certificate about 1.1 kB.
    QByteArray keyBuf(8192, '\0');
    QByteArray certBuf(8192, '\0');
    do {
        step = "pk_setup";
        if ((ret = mbedtls_pk_setup(&key, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA))) != 0)
            break;
        step = "rsa_gen_key";
        if ((ret = mbedtls_rsa_gen_key(mbedtls_pk_rsa(key), TlsRandom::random, TlsRandom::context(),
                                       2048, 65537)) != 0)
            break;
        qDebug() << "ClientIdentity: RSA key generated in" << timer.elapsed() << "ms";
        // PKCS#1 "BEGIN RSA PRIVATE KEY", as Qt's QSslKey and src/tlsserver.cpp read it.
        step = "pk_write_key_pem";
        if ((ret = mbedtls_pk_write_key_pem(&key, (unsigned char *)keyBuf.data(), keyBuf.size())) != 0)
            break;

        // X.509 v3 (rustls/webpki reject v1), self-signed, SHA-256.
        mbedtls_x509write_crt_set_version(&crt, MBEDTLS_X509_CRT_VERSION_3);
        mbedtls_x509write_crt_set_md_alg(&crt, MBEDTLS_MD_SHA256);
        mbedtls_x509write_crt_set_subject_key(&crt, &key);
        mbedtls_x509write_crt_set_issuer_key(&crt, &key);
        step = "set_subject_name";
        if ((ret = mbedtls_x509write_crt_set_subject_name(&crt, "CN=LocalSend User")) != 0)
            break;
        step = "set_issuer_name";
        if ((ret = mbedtls_x509write_crt_set_issuer_name(&crt, "CN=LocalSend User")) != 0)
            break;
        // A positive 31-bit random serial (never 0).
        unsigned char serial[4] = { 0, 0, 0, 0 };
        TlsRandom::random(TlsRandom::context(), serial, sizeof(serial));
        serial[0] &= 0x7f;
        serial[3] |= 1;
        step = "set_serial";
        if ((ret = mbedtls_x509write_crt_set_serial_raw(&crt, serial, sizeof(serial))) != 0)
            break;
        // Fixed, wide validity: neither the phone's nor the receiver's clock
        // can make it "not yet valid" or expired (the server checks it).
        // mbedTLS writes UTCTime before 2050, GeneralizedTime after.
        step = "set_validity";
        if ((ret = mbedtls_x509write_crt_set_validity(&crt, "20000101000000", "40960101000000")) != 0)
            break;
        // mbedTLS always writes the v3 extensions field; without an extension
        // it is an empty SEQUENCE, which the X.509 grammar (SIZE 1..MAX) and
        // strict parsers reject. The subject key identifier changes nothing else.
        step = "set_subject_key_identifier";
        if ((ret = mbedtls_x509write_crt_set_subject_key_identifier(&crt)) != 0)
            break;
        step = "crt_pem";
        if ((ret = mbedtls_x509write_crt_pem(&crt, (unsigned char *)certBuf.data(), certBuf.size(),
                                             TlsRandom::random, TlsRandom::context())) != 0)
            break;
        *keyPem = QByteArray(keyBuf.constData());
        *certPem = QByteArray(certBuf.constData());
    } while (false);

    mbedtls_x509write_crt_free(&crt);
    mbedtls_pk_free(&key);
    keyBuf.fill('\0');

    const bool ok = ret == 0 && !certPem->isEmpty() && !keyPem->isEmpty();
    if (ok)
        qDebug() << "ClientIdentity: certificate created in" << timer.elapsed() << "ms";
    else
        qWarning() << "ClientIdentity: creating the certificate failed at" << step << ":"
                   << TlsRandom::errorString(ret);
    return ok;
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

    const QString fingerprint = Verification::fingerprintOfDer(cert.toDer());
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
