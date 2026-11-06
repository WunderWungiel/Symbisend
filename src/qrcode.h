#ifndef QRCODE_H
#define QRCODE_H

#include <QDeclarativeImageProvider>

// image://qrcode/<text>, with the text percent-encoded (encodeURIComponent):
// that text as a QR code, black on white whatever the theme, because scanners
// expect that. The white border is at least the 4 modules the standard asks
// for. Modules are a whole number of pixels, so set sourceSize to the size
// shown and smooth to false. Error correction is level Q, like the official app,
// or higher when that fits in the same size.
class QrCodeProvider : public QDeclarativeImageProvider
{
public:
    QrCodeProvider();
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize);
};

#endif // QRCODE_H
