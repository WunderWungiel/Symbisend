# mbedTLS 3.4.1 (Symbian port), compiled into the application.
# The sources are the git submodule upstream/ (see README.md). Only the modules that symbisend_config.h enables are listed.
#
# MBEDTLS_STATIC: the port marks its API EXPORT_C for its own mbedtls.dll;
# with this macro those markers are empty, so nothing is exported from our
# executable and nothing is imported from a system-wide mbedtls.dll.

MBEDTLS_DIR = $$PWD
MBEDTLS_SRC = $$PWD/upstream

!exists($$MBEDTLS_SRC/library/ssl_tls.c) {
    error("mbedTLS sources missing: run git submodule update --init")
}

INCLUDEPATH += $$MBEDTLS_DIR $$MBEDTLS_SRC/include
DEFINES += MBEDTLS_STATIC
# Our configuration instead of include/mbedtls/mbedtls_config.h. sbsv2 mangles
# a quoted macro value, so Symbian gets the <> form, and only through the MMP:
# in DEFINES it would also reach moc's command line, where the shell takes
# < and > as redirections (moc then fails and its output is stale).
symbian:MMP_RULES += "MACRO MBEDTLS_CONFIG_FILE=<symbisend_config.h>"
else:DEFINES += MBEDTLS_CONFIG_FILE=\\\"symbisend_config.h\\\"

MBEDTLS_MODULES = \
    aes asn1parse asn1write base64 \
    bignum bignum_core bignum_mod bignum_mod_raw \
    chacha20 chachapoly cipher cipher_wrap constant_time ctr_drbg \
    ecdh ecdsa ecp ecp_curves entropy entropy_poll error gcm hash_info \
    md oid pem pk pk_wrap pkparse pkwrite platform platform_util poly1305 \
    rsa rsa_alt_helpers sha1 sha256 sha512 \
    ssl_ciphersuites ssl_client ssl_msg ssl_tls ssl_tls12_client ssl_tls12_server \
    version x509 x509_create x509_crt x509write_crt

# __aeabi_idiv/__aeabi_uidiv shims for GCCE.
symbian:MBEDTLS_MODULES += symbian

for(m, MBEDTLS_MODULES):SOURCES += $$MBEDTLS_SRC/library/$${m}.c

symbian {
    # We build against Open C (stdapis), the port's "PIPS" variant: real
    # stdint.h and snprintf instead of its ESTLIB replacements.
    DEFINES += PIPS
    # The system CSPRNG for src/tlsrandom.cpp.
    LIBS += -lrandom
}
win32 {
    # CryptGenRandom in entropy_poll.c.
    LIBS += -ladvapi32
}
