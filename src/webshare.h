#ifndef WEBSHARE_H
#define WEBSHARE_H

#include <QObject>
#include <QString>

#include "httpserver.h"

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
// Our server speaks plain http, so the link is http://<our IP>:53317 (the
// official app also uses http by default for links: browsers can't verify
// the self-signed certificate). "Share via link" (the download page) is not
// implemented yet. See docs/qml-api.md.
class WebShare : public QObject, public HttpHandler
{
    Q_OBJECT
    Q_PROPERTY(bool receiveActive READ isReceiveActive NOTIFY receiveActiveChanged)
    // "http://192.168.1.5:53317", or "" while offline.
    Q_PROPERTY(QString url READ url NOTIFY urlChanged)
    // The PIN browsers (and other senders) must give while the link is
    // active; "" = none. Starts as the receive PIN from settings.
    Q_PROPERTY(QString pin READ pin WRITE setPin NOTIFY pinChanged)

public:
    WebShare(HttpServer *server, Settings *settings, NetworkManager *network,
             int port, QObject *parent = 0);

    bool isReceiveActive() const { return m_receiveActive; }
    QString url() const;
    QString pin() const { return m_pin; }
    void setPin(const QString &pin);

    Q_INVOKABLE void startReceive();
    Q_INVOKABLE void stop();

    // HttpHandler
    bool handles(const HttpRequest &request) const;
    HttpResponse handleRequest(const HttpRequest &request);

signals:
    void receiveActiveChanged();
    void urlChanged();
    void pinChanged();

private:
    static HttpResponse page(int status, const QString &resource);
    static QByteArray i18nJson();

    Settings *m_settings;
    NetworkManager *m_network;
    int m_port;
    bool m_receiveActive;
    QString m_pin;
};

#endif // WEBSHARE_H
