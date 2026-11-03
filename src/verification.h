#ifndef VERIFICATION_H
#define VERIFICATION_H

#include <QDeclarativeImageProvider>
#include <QString>
#include <QStringList>

// LocalSend's Verify page (app/lib/pages/verify_page.dart,
// app/lib/util/fingerprint_alphabet.dart): both devices show the same 16
// icons and the same text when the fingerprints they see are the real ones.
namespace Verification {

// Both fingerprints sorted and joined (Dart's [a, b]..sort() then join()).
// This is the "Text" shown.
QString combined(const QString &myFingerprint, const QString &peerFingerprint);

// The 16 icon names for a combined fingerprint: SHA-256 of its UTF-8 bytes,
// the first 128 bits written in base 260 (the icon alphabet), most
// significant digit first. Names are files in fingerprint-icons/.
QStringList icons(const QString &combined);

}

// image://verify/<name>/<rrggbb>: fingerprint-icons/<name>.svg drawn in that
// colour (they are black; QML 1 cannot tint an Image itself).
class VerifyIconProvider : public QDeclarativeImageProvider
{
public:
    VerifyIconProvider();
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize);
};

#endif // VERIFICATION_H
