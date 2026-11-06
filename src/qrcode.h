#ifndef QRCODE_H
#define QRCODE_H

#include <QDeclarativeImageProvider>

// image://qrcode/<rrggbb>/<text>, with the text percent-encoded
// (encodeURIComponent): that text as a QR code in that colour, on a transparent
// background, like the official app, which draws it in the theme's text colour.
// A transparent border of at least the 4 modules the standard asks for keeps it
// clear of what surrounds the image. Modules are a whole number of pixels, so
// set sourceSize to the size shown and smooth to false. Error correction is
// level Q, like the official app,
// or higher when that fits in the same size.
class QrCodeProvider : public QDeclarativeImageProvider
{
public:
    QrCodeProvider();
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize);
};

#endif // QRCODE_H
