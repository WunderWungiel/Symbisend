#include "peermodel.h"

#include <QDebug>

PeerModel::PeerModel(QObject *parent)
    : QAbstractListModel(parent)
{
    QHash<int, QByteArray> roles;
    roles[AliasRole] = "alias";
    roles[IpRole] = "ip";
    roles[PortRole] = "port";
    roles[ProtocolRole] = "protocol";
    roles[VersionRole] = "version";
    roles[DeviceModelRole] = "deviceModel";
    roles[DeviceTypeRole] = "deviceType";
    roles[FingerprintRole] = "fingerprint";
    roles[SourceRole] = "source";
    roles[LastSeenRole] = "lastSeen";
    setRoleNames(roles);
}

int PeerModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_peers.size();
}

QVariant PeerModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_peers.size())
        return QVariant();
    const Peer &p = m_peers.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case AliasRole: return p.alias;
    case IpRole: return p.ip;
    case PortRole: return p.port;
    case ProtocolRole: return p.protocol;
    case VersionRole: return p.version;
    case DeviceModelRole: return p.deviceModel;
    case DeviceTypeRole: return p.deviceType;
    case FingerprintRole: return p.fingerprint;
    case SourceRole: return p.source;
    case LastSeenRole: return p.lastSeen;
    }
    return QVariant();
}

Peer PeerModel::peerFromMap(const QVariantMap &map, const QString &ip,
                            int fallbackPort, const QString &fallbackProtocol,
                            const QString &source)
{
    Peer p;
    p.fingerprint = map.value(QLatin1String("fingerprint")).toString();
    p.alias = map.value(QLatin1String("alias")).toString();
    p.ip = ip;
    p.port = map.contains(QLatin1String("port"))
            ? map.value(QLatin1String("port")).toInt() : fallbackPort;
    // v1 messages carry neither "version" nor "protocol"/"port"; v1 is plain HTTP only.
    p.version = map.value(QLatin1String("version"), QLatin1String("1.0")).toString();
    if (map.contains(QLatin1String("protocol")))
        p.protocol = map.value(QLatin1String("protocol")).toString();
    else
        p.protocol = isV1(p.version) ? QString::fromLatin1("http") : fallbackProtocol;
    p.deviceModel = map.value(QLatin1String("deviceModel")).toString();
    p.deviceType = map.value(QLatin1String("deviceType")).toString();
    p.source = source;
    p.lastSeen = QDateTime::currentDateTime();
    if (p.port <= 0)
        p.port = fallbackPort;
    if (p.alias.isEmpty())
        p.alias = ip;
    return p;
}

int PeerModel::indexOf(const QString &fingerprint) const
{
    for (int i = 0; i < m_peers.size(); ++i) {
        if (m_peers.at(i).fingerprint == fingerprint)
            return i;
    }
    return -1;
}

void PeerModel::upsert(const Peer &peer)
{
    if (peer.fingerprint.isEmpty()) {
        qWarning() << "Ignoring peer without fingerprint at" << peer.ip;
        return;
    }
    int row = indexOf(peer.fingerprint);
    if (row >= 0) {
        m_peers[row] = peer;
        emit dataChanged(index(row), index(row));
        return;
    }
    qDebug() << "New peer:" << peer.alias << peer.ip << peer.port << peer.protocol
             << "v" + peer.version << "via" << peer.source;
    beginInsertRows(QModelIndex(), m_peers.size(), m_peers.size());
    m_peers.append(peer);
    endInsertRows();
    emit countChanged();
}

void PeerModel::removeOlderThan(const QDateTime &cutoff)
{
    for (int i = m_peers.size() - 1; i >= 0; --i) {
        if (m_peers.at(i).lastSeen < cutoff) {
            qDebug() << "Peer expired:" << m_peers.at(i).alias << m_peers.at(i).ip;
            beginRemoveRows(QModelIndex(), i, i);
            m_peers.removeAt(i);
            endRemoveRows();
            emit countChanged();
        }
    }
}

void PeerModel::clear()
{
    if (m_peers.isEmpty())
        return;
    beginResetModel();
    m_peers.clear();
    endResetModel();
    emit countChanged();
}
