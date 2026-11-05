/*
 * What upstream mbedTLS needs beyond its own sources on platforms it doesn't
 * cover. Each function is selected in symbisend_config.h through one of
 * upstream's own *_ALT options; compiled by mbedtls.pri.
 */
#include "mbedtls/build_info.h"

#if defined(MBEDTLS_PLATFORM_ZEROIZE_ALT)
#include "mbedtls/platform_util.h"

/* Symbian: upstream's version keeps memset() in a static function pointer, and
 * elf2e32 refuses a data reference to a DLL import ("Import relocation does
 * not refer to code segment"). Windows: the Qt SDK's MinGW has no
 * SecureZeroMemory. */
void mbedtls_platform_zeroize(void *buf, size_t len)
{
    volatile unsigned char *p = (volatile unsigned char *) buf;
    while (len--)
        *p++ = 0;
}
#endif

#if defined(MBEDTLS_PLATFORM_MS_TIME_ALT)
#include "mbedtls/platform_time.h"

#include <time.h>

/* Symbian: Open C doesn't define _POSIX_VERSION, so upstream has no
 * implementation here. Only the TLS 1.3 and session ticket code calls it. */
mbedtls_ms_time_t mbedtls_ms_time(void)
{
    return (mbedtls_ms_time_t) time(NULL) * 1000;
}
#endif

#if defined(MBEDTLS_ENTROPY_HARDWARE_ALT)
#include "mbedtls/entropy.h"

#include <windows.h>
#include <wincrypt.h>

/* Windows (the desktop test harness): upstream uses bcrypt.h, which the Qt
 * SDK's MinGW doesn't have. */
int mbedtls_hardware_poll(void *data, unsigned char *output, size_t len, size_t *olen)
{
    HCRYPTPROV provider;
    BOOL ok;

    (void) data;
    if (!CryptAcquireContext(&provider, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
        return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
    ok = CryptGenRandom(provider, (DWORD) len, output);
    CryptReleaseContext(provider, 0);
    if (!ok)
        return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
    *olen = len;
    return 0;
}
#endif
