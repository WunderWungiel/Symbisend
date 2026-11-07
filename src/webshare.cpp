// SPDX-FileCopyrightText: 2026 Wunder Wungiel
// SPDX-License-Identifier: GPL-3.0-or-later

#include "webshare.h"

#include <QDebug>
#include <QFile>
#include <QVariantMap>

#include "deviceinfo.h"
#include "discovery.h"
#include "json.h"
#include "networkmanager.h"
#include "settings.h"

WebShare::WebShare(Discovery *discovery, Settings *settings, NetworkManager *network,
                   DeviceInfo *device, QObject *parent)
    : QObject(parent),
      m_discovery(discovery), m_settings(settings), m_network(network), m_device(device),
      m_receiveActive(false), m_encrypted(false)
{
    m_discovery->httpServer()->addHandler(this);
    connect(m_network, SIGNAL(onlineChanged()), this, SIGNAL(urlChanged()));
    // The scheme follows the server (restarted as http/https).
    connect(m_discovery, SIGNAL(statusChanged()), this, SIGNAL(urlChanged()));
}

QString WebShare::url() const
{
    if (!m_network->isOnline() || m_network->localAddress().isNull())
        return QString();
    return QString::fromLatin1("%1://%2:%3")
            .arg(QLatin1String(m_discovery->isServerHttps() ? "https" : "http"))
            .arg(m_network->localAddressString()).arg(m_device->port());
}

void WebShare::setEncrypted(bool on)
{
    if (on == m_encrypted)
        return;
    m_encrypted = on;
    qDebug() << "WebShare: encryption" << (on ? "on" : "off");
    emit encryptedChanged();
    if (m_receiveActive)
        m_discovery->setLinkEncryption(true, m_encrypted);
}

void WebShare::setPin(const QString &pin)
{
    if (pin == m_pin)
        return;
    m_pin = pin;
    qDebug() << "WebShare:" << (m_pin.isEmpty() ? "PIN off" : "PIN set");
    emit pinChanged();
}

void WebShare::startReceive()
{
    if (m_receiveActive)
        return;
    // Like the official app: the link starts with the receive PIN from
    // settings and can change it without touching the settings.
    setPin(m_settings->requirePin() ? m_settings->pin() : QString());
    // Like the official app: every link starts unencrypted.
    setEncrypted(false);
    m_receiveActive = true;
    m_discovery->setLinkEncryption(true, m_encrypted);
    qDebug() << "WebShare: receive via link on," << (url().isEmpty() ? QString::fromLatin1("offline") : url())
             << "auto accept" << m_settings->receiveViaLinkAutoAccept();
    emit receiveActiveChanged();
}

void WebShare::stop()
{
    if (!m_receiveActive)
        return;
    m_receiveActive = false;
    setPin(QString());
    // Back to settings.encryption.
    m_discovery->setLinkEncryption(false, false);
    setEncrypted(false);
    qDebug() << "WebShare: receive via link off";
    emit receiveActiveChanged();
}

// ---------------------------------------------------------------- HTTP

bool WebShare::handles(const HttpRequest &request) const
{
    return request.path == QLatin1String("/") || request.path == QLatin1String("/i18n.json");
}

HttpResponse WebShare::handleRequest(const HttpRequest &request)
{
    if (request.method != QLatin1String("GET"))
        return HttpResponse::message(405, QLatin1String("Method not allowed"));
    if (request.path == QLatin1String("/i18n.json"))
        return HttpResponse(200, i18nJson());
    const QString agent = QString::fromUtf8(request.headers.value("user-agent"));
    if (!m_receiveActive) {
        qDebug() << "WebShare:" << request.remoteAddress.toString() << "opened / (no link active)";
        return page(403, QLatin1String(":/web/error-403.html"));
    }
    qDebug() << "WebShare:" << request.remoteAddress.toString() << "opened the upload page" << agent;
    return page(200, QLatin1String(":/web/upload.html"));
}

HttpResponse WebShare::page(int status, const QString &resource)
{
    QFile f(resource);
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "WebShare: missing resource" << resource;
        return HttpResponse::message(500, QLatin1String("Page missing"));
    }
    HttpResponse response(status, f.readAll());
    response.contentType = "text/html; charset=utf-8";
    return response;
}

// The official server's default strings (WebI18n::default() in web.rs);
// the app fills them from its translations, we have none yet.
QByteArray WebShare::i18nJson()
{
    QVariantMap m;
    m.insert(QLatin1String("waiting"), QString::fromUtf8("Waiting for response\xE2\x80\xA6"));
    m.insert(QLatin1String("enterPin"), QLatin1String("Enter PIN"));
    m.insert(QLatin1String("invalidPin"), QLatin1String("Invalid PIN"));
    m.insert(QLatin1String("tooManyAttempts"), QLatin1String("Too many attempts"));
    m.insert(QLatin1String("rejected"), QLatin1String("Rejected"));
    m.insert(QLatin1String("uploadRejected"), QLatin1String("The recipient has rejected the request."));
    m.insert(QLatin1String("busy"), QLatin1String("The recipient is busy with another request."));
    m.insert(QLatin1String("files"), QLatin1String("Files"));
    m.insert(QLatin1String("fileName"), QLatin1String("File name"));
    m.insert(QLatin1String("size"), QLatin1String("Size"));
    m.insert(QLatin1String("dropHint"), QLatin1String("Place items to share."));
    return Json::serialize(m);
}
