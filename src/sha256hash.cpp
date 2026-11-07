// SPDX-FileCopyrightText: 2026 Wunder Wungiel
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sha256hash.h"

#include <QDebug>

extern "C" {
#include "mbedtls/sha256.h"
}

static mbedtls_sha256_context *context(void *p)
{
    return static_cast<mbedtls_sha256_context *>(p);
}

Sha256Hash::Sha256Hash()
    : m_context(new mbedtls_sha256_context), m_ok(true)
{
    mbedtls_sha256_init(context(m_context));
    if (mbedtls_sha256_starts(context(m_context), 0) != 0)
        m_ok = false;
}

Sha256Hash::~Sha256Hash()
{
    mbedtls_sha256_free(context(m_context));
    delete context(m_context);
}

void Sha256Hash::update(const QByteArray &data)
{
    if (m_ok && mbedtls_sha256_update(context(m_context),
                                      reinterpret_cast<const unsigned char *>(data.constData()),
                                      size_t(data.size())) != 0)
        m_ok = false;
}

QString Sha256Hash::result()
{
    unsigned char digest[32];
    if (!m_ok || mbedtls_sha256_finish(context(m_context), digest) != 0) {
        qWarning() << "Sha256Hash: SHA-256 failed";
        m_ok = false;
        return QString();
    }
    return QString::fromLatin1(QByteArray(reinterpret_cast<const char *>(digest), 32).toHex());
}
