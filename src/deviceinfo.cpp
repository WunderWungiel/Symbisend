#include "deviceinfo.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QSettings>
#include <QStringList>

const char *DeviceInfo::ProtocolVersion = "2.0";

static QString randomAlias()
{
    static const char *adjectives[] = {
        "Adorable", "Beautiful", "Big", "Bright", "Clean", "Clever", "Cool", "Cute",
        "Cunning", "Determined", "Energetic", "Efficient", "Fantastic", "Fast", "Fine",
        "Fresh", "Good", "Gorgeous", "Great", "Handsome", "Hot", "Kind", "Lovely",
        "Mystic", "Neat", "Nice", "Patient", "Pretty", "Powerful", "Rich", "Secret",
        "Smart", "Solid", "Special", "Strategic", "Strong", "Tidy", "Wise"
    };
    static const char *fruits[] = {
        "Apple", "Avocado", "Banana", "Blackberry", "Blueberry", "Broccoli", "Carrot",
        "Cherry", "Coconut", "Grape", "Lemon", "Lettuce", "Mango", "Melon", "Mushroom",
        "Onion", "Orange", "Papaya", "Peach", "Pear", "Pineapple", "Potato", "Pumpkin",
        "Raspberry", "Strawberry", "Tomato"
    };
    const int na = sizeof(adjectives) / sizeof(adjectives[0]);
    const int nf = sizeof(fruits) / sizeof(fruits[0]);
    return QString::fromLatin1("%1 %2")
            .arg(QLatin1String(adjectives[qrand() % na]))
            .arg(QLatin1String(fruits[qrand() % nf]));
}

static QString randomFingerprint()
{
    // In HTTP mode LocalSend only needs a unique random string.
    QByteArray seed = QByteArray::number(QDateTime::currentDateTime().toTime_t());
    for (int i = 0; i < 32; ++i)
        seed.append(char(qrand() & 0xff));
    return QString::fromLatin1(QCryptographicHash::hash(seed, QCryptographicHash::Sha1).toHex());
}

DeviceInfo::DeviceInfo(QObject *parent)
    : QObject(parent)
{
    QSettings settings;
    m_alias = settings.value(QLatin1String("identity/alias")).toString();
    if (m_alias.isEmpty()) {
        m_alias = randomAlias();
        settings.setValue(QLatin1String("identity/alias"), m_alias);
    }
    m_fingerprint = settings.value(QLatin1String("identity/fingerprint")).toString();
    if (m_fingerprint.isEmpty()) {
        m_fingerprint = randomFingerprint();
        settings.setValue(QLatin1String("identity/fingerprint"), m_fingerprint);
    }
#if defined(Q_OS_SYMBIAN)
    m_deviceModel = QLatin1String("Symbian");
#elif defined(MEEGO_EDITION_HARMATTAN)
    m_deviceModel = QLatin1String("MeeGo");
#else
    m_deviceModel = QLatin1String("Qt");
#endif
}

void DeviceInfo::setAlias(const QString &alias)
{
    QString a = alias.trimmed();
    if (a.isEmpty() || a == m_alias)
        return;
    m_alias = a;
    QSettings().setValue(QLatin1String("identity/alias"), m_alias);
    emit aliasChanged();
}

QVariantMap DeviceInfo::toRegisterMap() const
{
    QVariantMap m;
    m.insert(QLatin1String("alias"), m_alias);
    m.insert(QLatin1String("version"), QLatin1String(ProtocolVersion));
    m.insert(QLatin1String("deviceModel"), m_deviceModel);
    m.insert(QLatin1String("deviceType"), deviceType());
    m.insert(QLatin1String("fingerprint"), m_fingerprint);
    m.insert(QLatin1String("port"), port());
    m.insert(QLatin1String("protocol"), protocol());
    m.insert(QLatin1String("download"), false);
    return m;
}

QVariantMap DeviceInfo::toAnnounceMap(bool announce) const
{
    QVariantMap m = toRegisterMap();
    m.insert(QLatin1String("announce"), announce);
    m.insert(QLatin1String("announcement"), announce); // v1 compatibility
    return m;
}
