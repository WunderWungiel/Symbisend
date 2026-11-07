// SPDX-FileCopyrightText: 2026 Wunder Wungiel
// SPDX-License-Identifier: GPL-3.0-or-later

#include "peermodel.h"

#include <QDebug>
#include <QLocale>

#include "favoritesmodel.h"
#include "i18n.h"

// Same cap as LocalSend's discovery store (MAX_LOGS).
static const int MaxPeerLogs = 100;

PeerModel::PeerModel(QObject *parent)
    : QAbstractListModel(parent), m_favorites(0)
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
    roles[DisplayNameRole] = "displayName";
    roles[FavoriteRole] = "favorite";
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
    case DisplayNameRole: {
        // Like the official app: a favourite's name replaces the device's own.
        const QString favorite = m_favorites ? m_favorites->alias(p.fingerprint) : QString();
        return favorite.isEmpty() ? p.alias : favorite;
    }
    case FavoriteRole: return m_favorites && m_favorites->contains(p.fingerprint);
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
    p.version = map.value(QLatin1String("version")).toString();
    p.protocol = map.value(QLatin1String("protocol"), fallbackProtocol).toString();
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
    PeerLog log;
    log.time = QDateTime::currentDateTime();
    log.protocol = peer.protocol;
    log.host = peer.ip;

    int row = indexOf(peer.fingerprint);
    if (row >= 0) {
        QList<PeerLog> logs = m_peers.at(row).logs;
        log.discovered = false;
        logs.append(log);
        while (logs.size() > MaxPeerLogs)
            logs.removeFirst();
        m_peers[row] = peer;
        m_peers[row].logs = logs;
        emit dataChanged(index(row), index(row));
        emit peerChanged(peer.fingerprint);
        return;
    }
    qDebug() << "New peer:" << peer.alias << peer.ip << peer.port << peer.protocol
             << "v" + peer.version << "via" << peer.source;
    beginInsertRows(QModelIndex(), m_peers.size(), m_peers.size());
    m_peers.append(peer);
    log.discovered = true;
    m_peers.last().logs.clear();
    m_peers.last().logs.append(log);
    endInsertRows();
    emit countChanged();
    emit peerChanged(peer.fingerprint);
}

void PeerModel::watchFavorites(FavoritesModel *favorites)
{
    m_favorites = favorites;
    connect(favorites, SIGNAL(favoritesChanged(QString)), this, SLOT(onFavoritesChanged(QString)));
}

void PeerModel::onFavoritesChanged(const QString &fingerprint)
{
    const int row = indexOf(fingerprint);
    if (row >= 0)
        emit dataChanged(index(row), index(row));
}

bool PeerModel::find(const QString &fingerprint, Peer *out) const
{
    const int row = indexOf(fingerprint);
    if (row < 0)
        return false;
    *out = m_peers.at(row);
    return true;
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

QVariantMap PeerModel::peer(const QString &fingerprint) const
{
    QVariantMap map;
    int row = indexOf(fingerprint);
    if (row < 0)
        return map;
    const Peer &p = m_peers.at(row);
    map.insert(QLatin1String("alias"), p.alias);
    map.insert(QLatin1String("ip"), p.ip);
    map.insert(QLatin1String("port"), p.port);
    map.insert(QLatin1String("protocol"), p.protocol);
    map.insert(QLatin1String("version"), p.version);
    map.insert(QLatin1String("deviceModel"), p.deviceModel);
    map.insert(QLatin1String("deviceType"), p.deviceType);
    map.insert(QLatin1String("fingerprint"), p.fingerprint);
    map.insert(QLatin1String("source"), p.source);
    map.insert(QLatin1String("lastSeen"), p.lastSeen);

    // Same wording as LocalSend's device details page.
    QVariantList logs;
    const QLocale locale = QLocale::system();
    for (int i = 0; i < p.logs.size(); ++i) {
        const PeerLog &l = p.logs.at(i);
        QVariantMap entry;
        entry.insert(QLatin1String("time"), locale.toString(l.time.time(), QLocale::ShortFormat));
        entry.insert(QLatin1String("text"),
                     I18n::text(l.discovered ? "deviceDetailsPage.logs.discovered"
                                             : "deviceDetailsPage.logs.updated",
                                I18n::args("protocol", l.protocol.toUpper(), "host", l.host)));
        logs.append(entry);
    }
    map.insert(QLatin1String("logs"), logs);
    return map;
}
