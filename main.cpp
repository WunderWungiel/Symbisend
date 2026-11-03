#include <QtGui/QApplication>
#include <QtDeclarative/QDeclarativeContext>
#include <QDateTime>
#include <QDebug>
#include <QUrl>

#include "qmlapplicationviewer.h"
#include "src/logger.h"
#include "src/deviceinfo.h"
#include "src/peermodel.h"
#include "src/networkmanager.h"
#include "src/discovery.h"
#include "src/settings.h"
#include "src/picker.h"
#include "src/selectionmodel.h"
#include "src/sender.h"
#include "src/receiver.h"
#include "src/webshare.h"
#include "src/tlsrandom.h"

Q_DECL_EXPORT int main(int argc, char *argv[])
{
    QScopedPointer<QApplication> app(createApplication(argc, argv));
    app->setOrganizationName(QLatin1String("Symbisend"));
    app->setApplicationName(QLatin1String("Symbisend"));
    qsrand(QDateTime::currentDateTime().toTime_t() ^ quintptr(app.data()));

    Logger::install();
    qDebug() << "Symbisend starting, Qt runtime" << qVersion() << "(built with" << QT_VERSION_STR << ")";
    qDebug() << "Log file:" << Logger::instance()->logFilePath();
    TlsRandom::init();

    Settings settings;
    Picker picker;
    SelectionModel selection;
    // Everything picked on the Send page goes into the selection.
    QObject::connect(&picker, SIGNAL(filesPicked(QStringList)), &selection, SLOT(addFiles(QStringList)));
    QObject::connect(&picker, SIGNAL(folderPicked(QString)), &selection, SLOT(addFolder(QString)));
    DeviceInfo device;
    PeerModel peers;
    NetworkManager network;
    // Also picks http/https for the server (settings.encryption, webShare.encrypted).
    Discovery discovery(&device, &peers, &network, &settings);
    Sender sender(&device, &peers, &selection, &network);
    // Serves / and /i18n.json (the browser pages) on discovery's HTTP server.
    WebShare webShare(&discovery, &settings, &network, device.port());
    // Serves prepare-upload/upload/cancel on discovery's HTTP server.
    Receiver receiver(discovery.httpServer(), &settings, &network, &webShare);
    qDebug() << "Identity:" << device.alias() << "fingerprint" << device.fingerprint();

    QmlApplicationViewer viewer;
    QDeclarativeContext *ctx = viewer.rootContext();
    ctx->setContextProperty(QLatin1String("logger"), Logger::instance());
    ctx->setContextProperty(QLatin1String("myDevice"), &device);
    ctx->setContextProperty(QLatin1String("peerModel"), &peers);
    ctx->setContextProperty(QLatin1String("network"), &network);
    ctx->setContextProperty(QLatin1String("discovery"), &discovery);
    ctx->setContextProperty(QLatin1String("settings"), &settings);
    ctx->setContextProperty(QLatin1String("picker"), &picker);
    ctx->setContextProperty(QLatin1String("selection"), &selection);
    ctx->setContextProperty(QLatin1String("sender"), &sender);
    ctx->setContextProperty(QLatin1String("receiver"), &receiver);
    ctx->setContextProperty(QLatin1String("webShare"), &webShare);

    viewer.setOrientation(QmlApplicationViewer::ScreenOrientationAuto);
    viewer.setSource(QUrl(QLatin1String("qrc:/qml/Symbisend/main.qml")));
    viewer.showExpanded();

    network.connectNetwork();

    return app->exec();
}
