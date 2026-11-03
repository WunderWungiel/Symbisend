/*
 * mbedTLS configuration for Symbisend, selected with MBEDTLS_CONFIG_FILE
 * (see mbedtls.pri). The upstream include/mbedtls/mbedtls_config.h is not
 * used.
 *
 * What we need:
 *  - a TLS 1.2 server (LocalSend's https server) that rustls and current
 *    browsers accept: ECDHE-RSA with AES-GCM or ChaCha20-Poly1305;
 *  - a TLS 1.2 client, for a later move of outgoing requests;
 *  - RSA key generation and self-signed X.509 certificates
 *    (src/clientidentity.cpp);
 *  - SHA-256 for the certificate fingerprint.
 *
 * Entropy: on Symbian there is no platform entropy source in mbedTLS, so
 * src/tlsrandom.cpp adds the system CSPRNG (TRandom) itself. Elsewhere
 * mbedTLS reads /dev/urandom or CryptGenRandom.
 */
#ifndef SYMBISEND_MBEDTLS_CONFIG_H
#define SYMBISEND_MBEDTLS_CONFIG_H

/* --- System ------------------------------------------------------------ */
#define MBEDTLS_HAVE_ASM
#define MBEDTLS_HAVE_TIME
#define MBEDTLS_HAVE_TIME_DATE
#define MBEDTLS_PLATFORM_C

#if defined(__SYMBIAN32__)
#define MBEDTLS_NO_PLATFORM_ENTROPY
#endif

/* --- Hashes, ciphers, DRBG --------------------------------------------- */
#define MBEDTLS_MD_C
#define MBEDTLS_SHA224_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_SHA384_C
#define MBEDTLS_SHA512_C
#define MBEDTLS_SHA1_C          /* X.509 / TLS 1.2 signature algorithms peers may offer */

#define MBEDTLS_CIPHER_C
#define MBEDTLS_AES_C
#define MBEDTLS_GCM_C
#define MBEDTLS_CHACHA20_C
#define MBEDTLS_POLY1305_C
#define MBEDTLS_CHACHAPOLY_C

#define MBEDTLS_ENTROPY_C
#define MBEDTLS_CTR_DRBG_C

/* --- Public key -------------------------------------------------------- */
#define MBEDTLS_BIGNUM_C
#define MBEDTLS_GENPRIME
#define MBEDTLS_RSA_C
#define MBEDTLS_PKCS1_V15
#define MBEDTLS_PKCS1_V21

#define MBEDTLS_ECP_C
#define MBEDTLS_ECDH_C
#define MBEDTLS_ECDSA_C         /* verifying ECDSA-signed peer certificates */
#define MBEDTLS_ASN1_PARSE_C    /* (needed by ECDSA too) */
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_ECP_DP_SECP384R1_ENABLED
#define MBEDTLS_ECP_DP_CURVE25519_ENABLED
#define MBEDTLS_ECP_NIST_OPTIM

#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_OID_C
#define MBEDTLS_BASE64_C
#define MBEDTLS_PEM_PARSE_C
#define MBEDTLS_PEM_WRITE_C
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C
#define MBEDTLS_PK_WRITE_C

/* --- X.509 ------------------------------------------------------------- */
#define MBEDTLS_X509_USE_C
#define MBEDTLS_X509_CRT_PARSE_C
#define MBEDTLS_X509_CREATE_C
#define MBEDTLS_X509_CRT_WRITE_C

/* --- TLS --------------------------------------------------------------- */
#define MBEDTLS_SSL_TLS_C
#define MBEDTLS_SSL_SRV_C
#define MBEDTLS_SSL_CLI_C
#define MBEDTLS_SSL_PROTO_TLS1_2
#define MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED
#define MBEDTLS_SSL_EXTENDED_MASTER_SECRET
#define MBEDTLS_SSL_SERVER_NAME_INDICATION
#define MBEDTLS_SSL_KEEP_PEER_CERTIFICATE

/* Peers may send full-size (16 KB) records; we choose our own record size. */
#define MBEDTLS_SSL_IN_CONTENT_LEN  16384
#define MBEDTLS_SSL_OUT_CONTENT_LEN 4096

/* --- Diagnostics ------------------------------------------------------- */
#define MBEDTLS_ERROR_C         /* mbedtls_strerror() for the log */
#define MBEDTLS_VERSION_C

#endif /* SYMBISEND_MBEDTLS_CONFIG_H */
