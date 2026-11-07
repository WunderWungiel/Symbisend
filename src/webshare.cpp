// SPDX-FileCopyrightText: 2026 Wunder Wungiel
// SPDX-License-Identifier: GPL-3.0-or-later

#include "webshare.h"

#include <QDebug>
#include <QFile>
#include <QVariantMap>

#include "deviceinfo.h"
#include "discovery.h"
#include "i18n.h"
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

// The page's strings in the app's language, filled like the official
// server does (I18n::webStrings()).
QByteArray WebShare::i18nJson()
{
    return I18n::instance() ? I18n::instance()->webStrings() : QByteArray("{}");
}
