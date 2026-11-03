#include "deviceinfo.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QSettings>
#include <QStringList>

#include "clientidentity.h"
#include "verification.h"

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
    : QObject(parent), m_https(false)
{
    QSettings settings;
    m_alias = settings.value(QLatin1String("identity/alias")).toString();
    if (m_alias.isEmpty()) {
        m_alias = randomAlias();
        settings.setValue(QLatin1String("identity/alias"), m_alias);
    }
    m_fingerprint = settings.value(QLatin1String("identity/fingerprint")).toString();
    // With TLS the fingerprint must be the hash of our client certificate
    // (the official server checks it), so that one wins.
    const QString certFingerprint = ClientIdentity::install();
    if (!certFingerprint.isEmpty() && certFingerprint != m_fingerprint) {
        qDebug() << "DeviceInfo: fingerprint is now the client certificate's";
        m_fingerprint = certFingerprint;
        settings.setValue(QLatin1String("identity/fingerprint"), m_fingerprint);
    }
    if (m_fingerprint.isEmpty()) {
        m_fingerprint = randomFingerprint();
        settings.setValue(QLatin1String("identity/fingerprint"), m_fingerprint);
    }
    // Like the official app: the manufacturer on phones ("Samsung", ...),
    // the OS on desktops ("Windows", ...).
#if defined(Q_OS_SYMBIAN) || defined(MEEGO_EDITION_HARMATTAN)
    m_deviceModel = QLatin1String("Nokia");
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
    qDebug() << "DeviceInfo: alias set to" << m_alias;
    QSettings().setValue(QLatin1String("identity/alias"), m_alias);
    emit aliasChanged();
}

void DeviceInfo::randomizeAlias()
{
    QString a;
    do {
        a = randomAlias();
    } while (a == m_alias);
    qDebug() << "DeviceInfo: new random alias" << a;
    setAlias(a);
}

QVariantMap DeviceInfo::verification(const QString &peerFingerprint) const
{
    const QString text = Verification::combined(m_fingerprint, peerFingerprint);
    const QStringList icons = Verification::icons(text);
    qDebug() << "DeviceInfo: verify" << peerFingerprint.left(16) + "..." << "->"
             << icons.join(QLatin1String(" "));
    QVariantMap m;
    m.insert(QLatin1String("text"), text);
    m.insert(QLatin1String("icons"), icons);
    return m;
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
