// SPDX-FileCopyrightText: 2026 Wunder Wungiel
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef WEBSHARE_H
#define WEBSHARE_H

#include <QObject>
#include <QString>

#include "httpserver.h"

class DeviceInfo;
class Discovery;
class NetworkManager;
class Settings;

// The pages our server shows to web browsers, like the official app's
// "web share" (packages/core/src/http/server/web.rs):
//
//  - "Receive via link" (receiveActive): GET / serves the official upload
//    page (web/upload.html). The browser then sends files through the normal
//    v2 prepare-upload/upload endpoints, i.e. through Receiver, which uses
//    this object's pin and settings.receiveViaLinkAutoAccept meanwhile.
//  - otherwise GET / is the official 403 page.
//  - GET /i18n.json: the page's strings (the official English defaults).
//
// Encryption: like the official app, the link starts as plain http
// (browsers can't verify our self-signed certificate and warn about it).
// While the link is active, `encrypted` (not persisted, off every time the
// link starts) decides whether the server speaks https, instead of
// settings.encryption; Discovery restarts the server when that changes, and
// again when the link stops. "Share via link" (the download page) is not
// implemented yet. See docs/qml-api.md.
class WebShare : public QObject, public HttpHandler
{
    Q_OBJECT
    Q_PROPERTY(bool receiveActive READ isReceiveActive NOTIFY receiveActiveChanged)
    // "http://192.168.1.5:53317", or "" while offline.
    // "https://..." while the server speaks https.
    Q_PROPERTY(QString url READ url NOTIFY urlChanged)
    // While active: the server speaks https. Off whenever the link starts.
    Q_PROPERTY(bool encrypted READ isEncrypted WRITE setEncrypted NOTIFY encryptedChanged)
    // The PIN browsers (and other senders) must give while the link is
    // active; "" = none. Starts as the receive PIN from settings.
    Q_PROPERTY(QString pin READ pin WRITE setPin NOTIFY pinChanged)

public:
    WebShare(Discovery *discovery, Settings *settings, NetworkManager *network,
             DeviceInfo *device, QObject *parent = 0);

    bool isReceiveActive() const { return m_receiveActive; }
    QString url() const;
    QString pin() const { return m_pin; }
    void setPin(const QString &pin);
    bool isEncrypted() const { return m_encrypted; }
    void setEncrypted(bool on);

    Q_INVOKABLE void startReceive();
    Q_INVOKABLE void stop();

    // HttpHandler
    bool handles(const HttpRequest &request) const;
    HttpResponse handleRequest(const HttpRequest &request);

signals:
    void receiveActiveChanged();
    void urlChanged();
    void pinChanged();
    void encryptedChanged();

private:
    static HttpResponse page(int status, const QString &resource);
    static QByteArray i18nJson();

    Discovery *m_discovery;
    Settings *m_settings;
    NetworkManager *m_network;
    DeviceInfo *m_device;
    bool m_receiveActive;
    QString m_pin;
    bool m_encrypted;
};

#endif // WEBSHARE_H
