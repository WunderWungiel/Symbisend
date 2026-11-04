#ifndef SHA256HASH_H
#define SHA256HASH_H

#include <QByteArray>
#include <QString>

// SHA-256 over data that arrives in pieces (QCryptographicHash has none in
// Qt 4.7). Named so that its object file can't clash with mbedTLS's sha256.c.
class Sha256Hash
{
public:
    Sha256Hash();
    ~Sha256Hash();

    void update(const QByteArray &data);
    // Lowercase hex, like the official app; empty if hashing failed. Ends the
    // hash: call it once.
    QString result();

private:
    Sha256Hash(const Sha256Hash &);
    Sha256Hash &operator=(const Sha256Hash &);

    void *m_context; // mbedtls_sha256_context: see the note in the .cpp file
    bool m_ok;
};

#endif // SHA256HASH_H
