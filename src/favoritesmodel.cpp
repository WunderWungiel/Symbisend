// SPDX-FileCopyrightText: 2026 Wunder Wungiel
// SPDX-License-Identifier: GPL-3.0-or-later

#include "favoritesmodel.h"

#include <QAbstractSocket>
#include <QDebug>
#include <QHostAddress>
#include <QSettings>

#include "peermodel.h"

static const char *kArrayKey = "favorites";

FavoritesModel::FavoritesModel(QObject *parent)
    : QAbstractListModel(parent), m_peers(0)
{
    QHash<int, QByteArray> roles;
    roles[FingerprintRole] = "fingerprint";
    roles[AliasRole] = "alias";
    roles[CustomAliasRole] = "customAlias";
    roles[IpRole] = "ip";
    roles[PortRole] = "port";
    setRoleNames(roles);
    load();
}

int FavoritesModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_favorites.size();
}

QVariant FavoritesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_favorites.size())
        return QVariant();
    const Favorite &f = m_favorites.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case AliasRole: return f.alias;
    case FingerprintRole: return f.fingerprint;
    case CustomAliasRole: return f.customAlias;
    case IpRole: return f.ip;
    case PortRole: return f.port;
    }
    return QVariant();
}

int FavoritesModel::indexOf(const QString &fingerprint) const
{
    for (int i = 0; i < m_favorites.size(); ++i) {
        if (m_favorites.at(i).fingerprint == fingerprint)
            return i;
    }
    return -1;
}

bool FavoritesModel::contains(const QString &fingerprint) const
{
    return !fingerprint.isEmpty() && indexOf(fingerprint) >= 0;
}

bool FavoritesModel::add(const QString &fingerprint, const QString &alias,
                         const QString &deviceAlias, const QString &ip, int port)
{
    const QString cleanIp = ip.trimmed();
    QHostAddress address;
    if (fingerprint.isEmpty()) {
        qWarning() << "Favorites: no fingerprint, not added";
        return false;
    }
    if (!address.setAddress(cleanIp) || address.protocol() != QAbstractSocket::IPv4Protocol) {
        qWarning() << "Favorites: invalid IP address" << cleanIp;
        return false;
    }
    if (port < 1 || port > 65535) {
        qWarning() << "Favorites: invalid port" << port;
        return false;
    }

    const int row = indexOf(fingerprint);
    Favorite f;
    f.fingerprint = fingerprint;
    const QString name = alias.trimmed();
    // Like the official app: only a name that differs from the device's own
    // is custom; otherwise the favourite follows the device's alias.
    f.customAlias = !name.isEmpty() && name != deviceAlias;
    f.alias = f.customAlias ? name : deviceAlias;
    // Automatic name while the device's alias is unknown: keep the stored
    // one (or the address) until the device is discovered again.
    if (f.alias.isEmpty())
        f.alias = row >= 0 ? m_favorites.at(row).alias : address.toString();
    f.ip = address.toString();
    f.port = port;

    if (row >= 0) {
        m_favorites[row] = f;
        emit dataChanged(index(row), index(row));
    } else {
        beginInsertRows(QModelIndex(), m_favorites.size(), m_favorites.size());
        m_favorites.append(f);
        endInsertRows();
        emit countChanged();
    }
    qDebug() << "Favorites:" << (row >= 0 ? "updated" : "added") << f.alias
             << (f.customAlias ? "(custom name)" : "(auto name)") << f.ip << f.port;
    save();
    emit favoritesChanged(fingerprint);
    return true;
}

void FavoritesModel::remove(const QString &fingerprint)
{
    const int row = indexOf(fingerprint);
    if (row < 0)
        return;
    qDebug() << "Favorites: removed" << m_favorites.at(row).alias;
    beginRemoveRows(QModelIndex(), row, row);
    m_favorites.removeAt(row);
    endRemoveRows();
    emit countChanged();
    save();
    emit favoritesChanged(fingerprint);
}

QVariantMap FavoritesModel::favorite(const QString &fingerprint) const
{
    QVariantMap map;
    const int row = indexOf(fingerprint);
    if (row < 0)
        return map;
    const Favorite &f = m_favorites.at(row);
    map.insert(QLatin1String("fingerprint"), f.fingerprint);
    map.insert(QLatin1String("alias"), f.alias);
    map.insert(QLatin1String("customAlias"), f.customAlias);
    map.insert(QLatin1String("ip"), f.ip);
    map.insert(QLatin1String("port"), f.port);
    return map;
}

bool FavoritesModel::find(const QString &fingerprint, Favorite *out) const
{
    const int row = indexOf(fingerprint);
    if (row < 0)
        return false;
    *out = m_favorites.at(row);
    return true;
}

QString FavoritesModel::alias(const QString &fingerprint) const
{
    const int row = indexOf(fingerprint);
    return row >= 0 ? m_favorites.at(row).alias : QString();
}

void FavoritesModel::watchPeers(PeerModel *peers)
{
    m_peers = peers;
    connect(peers, SIGNAL(peerChanged(QString)), this, SLOT(onPeerChanged(QString)));
}

void FavoritesModel::onPeerChanged(const QString &fingerprint)
{
    const int row = indexOf(fingerprint);
    Peer peer;
    if (row < 0 || !m_peers || !m_peers->find(fingerprint, &peer))
        return;
    const QString &alias = peer.alias;
    Favorite &f = m_favorites[row];
    if (f.customAlias || f.alias == alias)
        return;
    qDebug() << "Favorites:" << f.alias << "is now called" << alias;
    f.alias = alias;
    emit dataChanged(index(row), index(row));
    save();
    emit favoritesChanged(fingerprint);
}

void FavoritesModel::load()
{
    QSettings s;
    const int n = s.beginReadArray(QLatin1String(kArrayKey));
    for (int i = 0; i < n; ++i) {
        s.setArrayIndex(i);
        Favorite f;
        f.fingerprint = s.value(QLatin1String("fingerprint")).toString();
        f.alias = s.value(QLatin1String("alias")).toString();
        f.customAlias = s.value(QLatin1String("customAlias"), false).toBool();
        f.ip = s.value(QLatin1String("ip")).toString();
        f.port = s.value(QLatin1String("port"), 53317).toInt();
        if (f.fingerprint.isEmpty() || indexOf(f.fingerprint) >= 0)
            continue;
        m_favorites.append(f);
    }
    s.endArray();
    qDebug() << "Favorites:" << m_favorites.size() << "loaded";
}

void FavoritesModel::save() const
{
    QSettings s;
    s.remove(QLatin1String(kArrayKey)); // drop entries beyond the new size
    s.beginWriteArray(QLatin1String(kArrayKey), m_favorites.size());
    for (int i = 0; i < m_favorites.size(); ++i) {
        const Favorite &f = m_favorites.at(i);
        s.setArrayIndex(i);
        s.setValue(QLatin1String("fingerprint"), f.fingerprint);
        s.setValue(QLatin1String("alias"), f.alias);
        s.setValue(QLatin1String("customAlias"), f.customAlias);
        s.setValue(QLatin1String("ip"), f.ip);
        s.setValue(QLatin1String("port"), f.port);
    }
    s.endArray();
}
