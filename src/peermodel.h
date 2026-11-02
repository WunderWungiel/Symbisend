#ifndef PEERMODEL_H
#define PEERMODEL_H

#include <QAbstractListModel>
#include <QDateTime>
#include <QList>
#include <QVariantMap>

// One confirmation of a peer (like LocalSend's DeviceLog): the first one
// discovered it, every later one updated it.
struct PeerLog
{
    QDateTime time;
    bool discovered;
    QString protocol;      // "http" or "https"
    QString host;          // IP the confirmation came from
};

struct Peer
{
    QString fingerprint;
    QString alias;
    QString ip;
    int port;
    QString protocol;      // "http" or "https"
    QString version;
    QString deviceModel;
    QString deviceType;
    QString source;        // how we found it: "register", "scan", "multicast"
    QDateTime lastSeen;
    QList<PeerLog> logs;   // oldest first, capped; kept across upserts
};

// List of discovered LocalSend devices, keyed by fingerprint.
class PeerModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        AliasRole = Qt::UserRole + 1,
        IpRole,
        PortRole,
        ProtocolRole,
        VersionRole,
        DeviceModelRole,
        DeviceTypeRole,
        FingerprintRole,
        SourceRole,
        LastSeenRole
    };

    explicit PeerModel(QObject *parent = 0);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;

    // Builds a Peer from LocalSend JSON (info/register/announce) + network data.
    static Peer peerFromMap(const QVariantMap &map, const QString &ip,
                            int fallbackPort, const QString &fallbackProtocol,
                            const QString &source);
    // Protocol v1 peers (no "version" field) use the /api/localsend/v1/ routes over http.
    static bool isV1(const QString &version) { return version.startsWith(QLatin1String("1.")); }

    void upsert(const Peer &peer);
    void removeOlderThan(const QDateTime &cutoff);
    Q_INVOKABLE void clear();

    // All data of one peer for the device details page, or an empty map when
    // the fingerprint is unknown. See docs/qml-api.md.
    Q_INVOKABLE QVariantMap peer(const QString &fingerprint) const;

signals:
    void countChanged();
    // A peer was added or re-confirmed (its data or logs changed).
    void peerChanged(const QString &fingerprint);

private:
    int indexOf(const QString &fingerprint) const;
    QList<Peer> m_peers;
};

#endif // PEERMODEL_H
