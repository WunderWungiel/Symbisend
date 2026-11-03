#ifndef TLSRANDOM_H
#define TLSRANDOM_H

#include <QString>
#include <stddef.h>

// The random number generator for everything mbedTLS does in the app (key
// generation, TLS handshakes): one CTR-DRBG, seeded from mbedTLS's entropy
// collector.
//
// Strong entropy source:
//  - Symbian: the system CSPRNG (TRandom::SecureRandomL, random.lib). The
//    Symbian port's own mbedtls_hardware_poll() uses rand() and is not
//    compiled in (MBEDTLS_ENTROPY_HARDWARE_ALT is off).
//  - Elsewhere: mbedTLS's platform source (/dev/urandom, CryptGenRandom).
// The clock is mixed in on every platform as a weak extra input.
//
// Everything here runs on the GUI thread; there is no locking.
namespace TlsRandom {
    // Seeds the DRBG (once; later calls return the first result) and logs
    // the outcome together with the mbedTLS version.
    bool init();
    bool ready();

    // mbedTLS-style f_rng callback; pass context() as p_rng.
    // Returns MBEDTLS_ERR_CTR_DRBG_ENTROPY_SOURCE_FAILED if init() failed.
    int random(void *context, unsigned char *output, size_t length);
    void *context();

    // "mbedTLS 3.4.1"
    QString versionString();
    // mbedtls_strerror() text plus the code, e.g. "SSL - ... (-0x7780)".
    QString errorString(int code);
}

#endif // TLSRANDOM_H
