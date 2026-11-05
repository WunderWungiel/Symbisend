# mbedTLS (upstream, 3.6 LTS), compiled into the application.
# The sources are the git submodule upstream/ (see README.md). Only the modules
# that symbisend_config.h enables are listed.

MBEDTLS_DIR = $$PWD
MBEDTLS_SRC = $$PWD/upstream

!exists($$MBEDTLS_SRC/library/ssl_tls.c) {
    error("mbedTLS sources missing: run git submodule update --init")
}

INCLUDEPATH += $$MBEDTLS_DIR $$MBEDTLS_SRC/include
# Our configuration instead of include/mbedtls/mbedtls_config.h. sbsv2 mangles
# a quoted macro value, so Symbian gets the <> form, and only through the MMP:
# in DEFINES it would also reach moc's command line, where the shell takes
# < and > as redirections (moc then fails and its output is stale).
symbian:MMP_RULES += "MACRO MBEDTLS_CONFIG_FILE=<symbisend_config.h>"
else:DEFINES += MBEDTLS_CONFIG_FILE=\\\"symbisend_config.h\\\"

MBEDTLS_MODULES = \
    aes asn1parse asn1write base64 \
    bignum bignum_core bignum_mod bignum_mod_raw block_cipher \
    chacha20 chachapoly cipher cipher_wrap constant_time ctr_drbg \
    ecdh ecdsa ecp ecp_curves ecp_curves_new entropy entropy_poll error gcm \
    md oid pem pk pk_ecc pk_wrap pkparse pkwrite platform platform_util poly1305 \
    psa_util rsa rsa_alt_helpers sha1 sha256 sha512 \
    ssl_ciphersuites ssl_client ssl_msg ssl_tls ssl_tls12_client ssl_tls12_server \
    version x509 x509_create x509_crt x509write x509write_crt

for(m, MBEDTLS_MODULES):SOURCES += $$MBEDTLS_SRC/library/$${m}.c

SOURCES += $$MBEDTLS_DIR/platform_glue.c

# mbedTLS is C99 and these compilers default to gnu89.
symbian {
    # An MMP option reaches the C++ sources as well; there it only warns.
    MMP_RULES += "OPTION GCCE -std=gnu99"
    # The system CSPRNG for src/tlsrandom.cpp.
    LIBS += -lrandom
} else {
    QMAKE_CFLAGS += -std=gnu99
}
unix:!symbian {
    # clock_gettime() in platform_util.c, on old glibc (Harmattan).
    LIBS += -lrt
}
win32 {
    # CryptGenRandom in platform_glue.c.
    LIBS += -ladvapi32
}
