// SPDX-FileCopyrightText: 2026 Wunder Wungiel
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FAVORITESMODEL_H
#define FAVORITESMODEL_H

#include <QAbstractListModel>
#include <QList>
#include <QVariantMap>

class PeerModel;

// One favourite device, like LocalSend's FavoriteDevice. customAlias: the
// user typed the name; otherwise it follows the device's own alias.
struct Favorite
{
    QString fingerprint;
    QString alias;
    bool customAlias;
    QString ip;
    int port;
};

// Favourite devices, keyed by fingerprint, persisted in QSettings
// ("favorites" array). See docs/qml-api.md.
class FavoritesModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        FingerprintRole = Qt::UserRole + 1,
        AliasRole,
        CustomAliasRole,
        IpRole,
        PortRole
    };

    explicit FavoritesModel(QObject *parent = 0);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;

    Q_INVOKABLE bool contains(const QString &fingerprint) const;
    // Adds the device, or updates it if it is a favourite already. alias ""
    // (or deviceAlias, the device's own alias) = automatic; with deviceAlias
    // "" too the stored name is kept until the device is discovered again. Returns false (nothing stored)
    // for an empty fingerprint, an invalid IPv4 address or a port outside
    // 1-65535.
    Q_INVOKABLE bool add(const QString &fingerprint, const QString &alias,
                         const QString &deviceAlias, const QString &ip, int port);
    Q_INVOKABLE void remove(const QString &fingerprint);
    // { fingerprint, alias, customAlias, ip, port }, or {} if not a favourite.
    Q_INVOKABLE QVariantMap favorite(const QString &fingerprint) const;

    const QList<Favorite> &favorites() const { return m_favorites; }
    // Copies the favourite into *out; false if it isn't one.
    bool find(const QString &fingerprint, Favorite *out) const;
    // The name to show for this device: the favourite's, else "".
    QString alias(const QString &fingerprint) const;

    // Automatic names follow the alias of the discovered devices in peers.
    void watchPeers(PeerModel *peers);

private slots:
    void onPeerChanged(const QString &fingerprint);

signals:
    void countChanged();
    // A favourite was added, changed or removed.
    void favoritesChanged(const QString &fingerprint);

private:
    int indexOf(const QString &fingerprint) const;
    void load();
    void save() const;

    PeerModel *m_peers;
    QList<Favorite> m_favorites;
};

#endif // FAVORITESMODEL_H
