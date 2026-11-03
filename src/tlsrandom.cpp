#include "tlsrandom.h"

#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>

#ifdef Q_OS_SYMBIAN
#include <e32base.h>
#include <random.h>
#endif

// Included last: on Symbian, mbedtls/build_info.h undefines EXPORT_C and
// IMPORT_C (MBEDTLS_STATIC) for the rest of the translation unit.
extern "C" {
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/error.h"
#include "mbedtls/version.h"
}

namespace {

enum State { NotSeeded, Seeded, Failed };

State g_state = NotSeeded;
mbedtls_entropy_context g_entropy;
mbedtls_ctr_drbg_context g_drbg;

#ifdef Q_OS_SYMBIAN
// Strong source: the system CSPRNG.
int systemRandomPoll(void *, unsigned char *output, size_t length, size_t *olen)
{
    TPtr8 buffer(output, int(length), int(length));
    TRAPD(err, TRandom::SecureRandomL(buffer));
    if (err != KErrNone) {
        // Leaves with KErrNotSecure (not in the SDK headers) shortly after
        // boot, while the generator is still collecting samples. RandomL()
        // reads the same generator without that check.
        static bool warned = false;
        if (!warned) {
            warned = true;
            qWarning() << "TLS: TRandom::SecureRandomL left with" << err
                       << "(phone booted recently?), using TRandom::RandomL";
        }
        TRAP(err, TRandom::RandomL(buffer));
    }
    if (err != KErrNone) {
        qWarning() << "TLS: TRandom::RandomL failed:" << err;
        *olen = 0;
        return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
    }
    *olen = length;
    return 0;
}
#endif

// Weak extra source: the clock, millisecond and timer resolution.
int clockPoll(void *, unsigned char *output, size_t length, size_t *olen)
{
    quint64 values[2];
    values[0] = quint64(QDateTime::currentMSecsSinceEpoch());
    QElapsedTimer t;
    t.start();
    values[1] = quint64(t.msecsSinceReference()) ^ (quint64(qrand()) << 32);
    size_t n = qMin(length, sizeof(values));
    memcpy(output, values, n);
    *olen = n;
    return 0;
}

} // namespace

bool TlsRandom::init()
{
    if (g_state != NotSeeded)
        return g_state == Seeded;

    qDebug() << "TLS:" << versionString();

    mbedtls_entropy_init(&g_entropy);
    mbedtls_ctr_drbg_init(&g_drbg);

    int ret = 0;
#ifdef Q_OS_SYMBIAN
    ret = mbedtls_entropy_add_source(&g_entropy, systemRandomPoll, 0, 32,
                                     MBEDTLS_ENTROPY_SOURCE_STRONG);
    if (ret != 0)
        qWarning() << "TLS: adding the TRandom entropy source failed:" << errorString(ret);
#endif
    if (ret == 0) {
        ret = mbedtls_entropy_add_source(&g_entropy, clockPoll, 0, 1,
                                         MBEDTLS_ENTROPY_SOURCE_WEAK);
        if (ret != 0)
            qWarning() << "TLS: adding the clock entropy source failed:" << errorString(ret);
    }
    if (ret == 0) {
        static const char personalization[] = "Symbisend TLS";
        ret = mbedtls_ctr_drbg_seed(&g_drbg, mbedtls_entropy_func, &g_entropy,
                                    reinterpret_cast<const unsigned char *>(personalization),
                                    sizeof(personalization) - 1);
        if (ret != 0)
            qWarning() << "TLS: seeding the random generator failed:" << errorString(ret);
    }

    if (ret != 0) {
        mbedtls_ctr_drbg_free(&g_drbg);
        mbedtls_entropy_free(&g_entropy);
        g_state = Failed;
        return false;
    }

    // Smoke test: draw a few bytes.
    unsigned char probe[16];
    ret = mbedtls_ctr_drbg_random(&g_drbg, probe, sizeof(probe));
    if (ret != 0) {
        qWarning() << "TLS: drawing random bytes failed:" << errorString(ret);
        g_state = Failed;
        return false;
    }
    g_state = Seeded;
    qDebug() << "TLS: random generator seeded";
    return true;
}

bool TlsRandom::ready()
{
    return g_state == Seeded;
}

int TlsRandom::random(void *context, unsigned char *output, size_t length)
{
    if (g_state != Seeded || context != &g_drbg)
        return MBEDTLS_ERR_CTR_DRBG_ENTROPY_SOURCE_FAILED;
    return mbedtls_ctr_drbg_random(context, output, length);
}

void *TlsRandom::context()
{
    return &g_drbg;
}

QString TlsRandom::versionString()
{
    return QLatin1String("mbedTLS ") + QLatin1String(MBEDTLS_VERSION_STRING);
}

QString TlsRandom::errorString(int code)
{
    char buffer[160];
    buffer[0] = 0;
    mbedtls_strerror(code, buffer, sizeof(buffer));
    QString hex = QString::number(code < 0 ? -code : code, 16);
    return QString::fromLatin1("%1 (%2%3)")
            .arg(QString::fromLatin1(buffer), QLatin1String(code < 0 ? "-0x" : "0x"), hex);
}
