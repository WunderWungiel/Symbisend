#include "verification.h"

#include <QBuffer>
#include <QByteArray>
#include <QDebug>
#include <QFile>
#include <QImage>
#include <QImageReader>

extern "C" {
#include "mbedtls/sha256.h"
}

namespace {

// LocalSend's iconAlphabet, in the same order (the order is the alphabet).
const char *const IconAlphabet[] = {
    "ac_unit", "accessibility", "agriculture", "alarm", "album",
    "anchor", "android", "apartment", "apple", "architecture",
    "attach_file", "attach_money", "audiotrack", "back_hand", "backpack",
    "badge", "bakery_dining", "balance", "bathtub", "battery_full",
    "bento", "biotech", "blender", "bluetooth", "bolt",
    "bookmark", "brush", "bug_report", "build", "cable",
    "cake", "calculate", "calendar_today", "campaign", "casino",
    "cast", "castle", "celebration", "cell_tower", "chair",
    "change_history", "chat", "checkroom", "church", "circle",
    "cloud", "coffee_maker", "colorize", "computer", "confirmation_number",
    "content_cut", "conveyor_belt", "cookie", "coronavirus", "cottage",
    "credit_card", "cruelty_free", "cyclone", "delete", "diamond",
    "directions_bike", "directions_boat", "directions_bus", "directions_car", "door_front_door",
    "earbuds", "eco", "edit", "egg", "elevator",
    "email", "emoji_events", "emoji_nature", "engineering", "explore",
    "extension", "face", "factory", "fastfood", "favorite",
    "fence", "festival", "fingerprint", "fire_extinguisher", "fireplace",
    "fitness_center", "flag", "flashlight_on", "flight", "flutter_dash",
    "forklift", "format_paint", "gavel", "grass", "groups",
    "handshake", "hardware", "headphones", "healing", "hearing",
    "hexagon", "history_edu", "hive", "home", "hourglass_empty",
    "houseboat", "hub", "ice_skating", "icecream", "inventory_2",
    "iron", "kebab_dining", "key", "keyboard", "king_bed",
    "kitchen", "landscape", "layers", "light", "lightbulb",
    "link", "liquor", "local_bar", "local_cafe", "local_drink",
    "local_florist", "local_gas_station", "local_laundry_service", "local_mall", "local_pizza",
    "local_shipping", "local_taxi", "location_city", "lock", "luggage",
    "map", "markunread_mailbox", "masks", "medication", "memory",
    "menu", "menu_book", "mic", "microwave", "military_tech",
    "monitor_heart", "mood", "mosque", "motorcycle", "mouse",
    "movie", "museum", "newspaper", "nightlight", "notifications",
    "oil_barrel", "outdoor_grill", "outlet", "palette", "park",
    "pentagon", "person", "pets", "phishing", "phone",
    "photo_camera", "piano", "pie_chart", "place", "pool",
    "power", "precision_manufacturing", "print", "propane_tank", "psychology",
    "public", "push_pin", "qr_code", "radar", "radio",
    "ramen_dining", "receipt", "recycling", "redeem", "refresh",
    "restaurant", "rocket", "room_service", "route", "router",
    "sailing", "satellite_alt", "savings", "school", "science",
    "search", "settings", "shelves", "shield", "shopping_basket",
    "shopping_cart", "shower", "signpost", "sim_card", "smart_toy",
    "smartphone", "smoking_rooms", "soap", "solar_power", "speaker",
    "sports_esports", "sports_football", "sports_golf", "sports_hockey", "sports_motorsports",
    "sports_soccer", "sports_tennis", "square", "stadium", "stairs",
    "star", "store", "straighten", "stroller", "table_restaurant",
    "tag", "temple_buddhist", "theater_comedy", "theaters", "thermostat",
    "thumb_up", "toggle_on", "toll", "tornado", "toys",
    "traffic", "train", "tune", "umbrella", "usb",
    "vaccines", "videocam", "view_in_ar", "visibility", "voicemail",
    "volcano", "wallet", "warehouse", "warning", "watch",
    "water_drop", "waves", "wb_sunny", "weekend", "whatshot",
    "wifi", "wind_power", "window", "wine_bar", "work"
};

const int AlphabetSize = int(sizeof(IconAlphabet) / sizeof(IconAlphabet[0])); // 260

// ceil(128 bits * ln 2 / ln 260): 16 digits, like the official app's
// convertFingerprintToAlphabet for a 32-character hex string.
const int IconCount = 16;
const int HashBytes = 16;

const int DefaultIconSize = 48;

}

namespace Verification {

QString fingerprintOfDer(const QByteArray &der)
{
    if (der.isEmpty())
        return QString();
    unsigned char digest[32];
    if (mbedtls_sha256(reinterpret_cast<const unsigned char *>(der.constData()),
                       size_t(der.size()), digest, 0) != 0) {
        qWarning() << "Verification: SHA-256 failed";
        return QString();
    }
    return QString::fromLatin1(QByteArray(reinterpret_cast<const char *>(digest), 32).toHex()).toUpper();
}

QString combined(const QString &myFingerprint, const QString &peerFingerprint)
{
    if (myFingerprint.isEmpty() || peerFingerprint.isEmpty())
        return QString();
    // QString's operator< compares UTF-16 code units, like Dart's compareTo.
    return myFingerprint < peerFingerprint ? myFingerprint + peerFingerprint
                                           : peerFingerprint + myFingerprint;
}

QStringList icons(const QString &combined)
{
    QStringList result;
    if (combined.isEmpty())
        return result;

    const QByteArray utf8 = combined.toUtf8();
    unsigned char digest[32];
    if (mbedtls_sha256(reinterpret_cast<const unsigned char *>(utf8.constData()),
                       size_t(utf8.size()), digest, 0) != 0) {
        qWarning() << "Verification: SHA-256 failed";
        return result;
    }

    // The 128-bit number, big-endian, divided by 260 again and again; each
    // remainder is the next digit from the right.
    unsigned char number[HashBytes];
    for (int i = 0; i < HashBytes; ++i)
        number[i] = digest[i];

    int digits[IconCount];
    for (int d = IconCount - 1; d >= 0; --d) {
        int remainder = 0;
        for (int i = 0; i < HashBytes; ++i) {
            const int current = remainder * 256 + number[i];
            number[i] = static_cast<unsigned char>(current / AlphabetSize);
            remainder = current % AlphabetSize;
        }
        digits[d] = remainder;
    }
    for (int d = 0; d < IconCount; ++d)
        result.append(QLatin1String(IconAlphabet[digits[d]]));
    return result;
}

}

VerifyIconProvider::VerifyIconProvider()
    : QDeclarativeImageProvider(QDeclarativeImageProvider::Image)
{
}

QImage VerifyIconProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    // "<name>/<rrggbb>"
    const QString name = id.section(QLatin1Char('/'), 0, 0);
    QString color = id.section(QLatin1Char('/'), 1, 1);
    if (color.size() != 6)
        color = QLatin1String("000000");

    QImage image;
    QFile file(QString::fromLatin1(":/fingerprint-icons/%1.svg").arg(name));
    if (name.isEmpty() || name.contains(QLatin1Char('.')) || !file.open(QIODevice::ReadOnly)) {
        qWarning() << "Verification: no icon" << id;
    } else {
        // The paths have no fill of their own (black); give the root one.
        QByteArray svg = file.readAll();
        svg.replace("<svg ", "<svg fill=\"#" + color.toLatin1() + "\" ");

        QBuffer buffer(&svg);
        buffer.open(QIODevice::ReadOnly);
        QImageReader reader(&buffer, "svg");
        const int w = requestedSize.width() > 0 ? requestedSize.width() : DefaultIconSize;
        const int h = requestedSize.height() > 0 ? requestedSize.height() : w;
        reader.setScaledSize(QSize(w, h));
        image = reader.read();
        if (image.isNull())
            qWarning() << "Verification: cannot draw icon" << id << reader.errorString();
    }

    if (image.isNull()) {
        // Keep the grid in shape: an empty square of the requested size.
        const int w = requestedSize.width() > 0 ? requestedSize.width() : DefaultIconSize;
        image = QImage(w, w, QImage::Format_ARGB32_Premultiplied);
        image.fill(0);
    }
    if (size)
        *size = image.size();
    return image;
}
