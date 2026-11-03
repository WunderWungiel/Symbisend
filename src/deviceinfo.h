#ifndef DEVICEINFO_H
#define DEVICEINFO_H

#include <QObject>
#include <QVariantMap>

// Our own LocalSend identity (alias, fingerprint, ...), persisted in QSettings.
class DeviceInfo : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString alias READ alias WRITE setAlias NOTIFY aliasChanged)
    Q_PROPERTY(QString fingerprint READ fingerprint CONSTANT)
    Q_PROPERTY(QString deviceModel READ deviceModel CONSTANT)
    Q_PROPERTY(int port READ port CONSTANT)

public:
    static const int DefaultPort = 53317;
    static const char *ProtocolVersion;

    explicit DeviceInfo(QObject *parent = 0);

    QString alias() const { return m_alias; }
    void setAlias(const QString &alias);
    // Picks a new random "Adjective Fruit" alias (different from the current one).
    Q_INVOKABLE void randomizeAlias();
    // LocalSend's Verify page for a peer: { text, icons } (see verification.h).
    // Empty text and icons if either fingerprint is unknown.
    Q_INVOKABLE QVariantMap verification(const QString &peerFingerprint) const;
    QString fingerprint() const { return m_fingerprint; }
    QString deviceModel() const { return m_deviceModel; }
    QString deviceType() const { return QLatin1String("mobile"); }
    int port() const { return DefaultPort; }
    // What our server speaks right now (Discovery sets it on every start).
    QString protocol() const { return QLatin1String(m_https ? "https" : "http"); }
    void setHttps(bool on) { m_https = on; }

    // Body for /register requests and responses (v2).
    QVariantMap toRegisterMap() const;
    // Body for UDP multicast announcements / replies.
    QVariantMap toAnnounceMap(bool announce) const;

signals:
    void aliasChanged();

private:
    QString m_alias;
    QString m_fingerprint;
    QString m_deviceModel;
    bool m_https;
};

#endif // DEVICEINFO_H
