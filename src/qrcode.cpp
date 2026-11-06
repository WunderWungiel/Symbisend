#include "qrcode.h"

#include <QByteArray>
#include <QColor>
#include <QDebug>
#include <QPainter>
#include <QUrl>

#include "qrcodegen.h"

namespace {

const int QuietZone = 4;            // modules, on each side
const int DefaultModuleSize = 4;    // pixels, when no size was requested

// Draws the code in the middle of a white square.
QImage draw(const uint8_t *qr, int side)
{
    const int modules = qrcodegen_getSize(qr);
    const int total = modules + 2 * QuietZone;
    if (side <= 0)
        side = total * DefaultModuleSize;
    // Too small a request still gives a code that can be read.
    const int scale = qMax(side / total, 1);
    side = qMax(side, total);

    QImage image(side, side, QImage::Format_RGB32);
    image.fill(QColor(Qt::white).rgb());

    const int origin = (side - modules * scale) / 2;
    QPainter painter(&image);
    for (int y = 0; y < modules; ++y) {
        for (int x = 0; x < modules; ++x) {
            if (qrcodegen_getModule(qr, x, y))
                painter.fillRect(origin + x * scale, origin + y * scale, scale, scale, Qt::black);
        }
    }
    return image;
}

}

QrCodeProvider::QrCodeProvider()
    : QDeclarativeImageProvider(QDeclarativeImageProvider::Image)
{
}

QImage QrCodeProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    const QByteArray text = QUrl::fromPercentEncoding(id.toUtf8()).toUtf8();

    QByteArray qr(qrcodegen_BUFFER_LEN_MAX, Qt::Uninitialized);
    QByteArray temp(qrcodegen_BUFFER_LEN_MAX, Qt::Uninitialized);
    const bool ok = qrcodegen_encodeText(text.constData(),
                                         reinterpret_cast<uint8_t *>(temp.data()),
                                         reinterpret_cast<uint8_t *>(qr.data()),
                                         qrcodegen_Ecc_QUARTILE,
                                         qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX,
                                         qrcodegen_Mask_AUTO, true);

    QImage image;
    if (ok) {
        image = draw(reinterpret_cast<const uint8_t *>(qr.constData()), requestedSize.width());
    } else {
        qWarning() << "QrCode: the text doesn't fit," << text.size() << "bytes";
        image = QImage(1, 1, QImage::Format_RGB32);
        image.fill(QColor(Qt::white).rgb());
    }
    if (size)
        *size = image.size();
    return image;
}
