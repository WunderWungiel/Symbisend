# The QML and images are compiled into the binary (qml.qrc), so no loose
# folders are deployed. List every new file under qml/Symbisend in qml.qrc.
DEPLOYMENTFOLDERS =

# Additional import path used to resolve QML modules in Creator's code model
QML_IMPORT_PATH =

QT += network script

symbian:TARGET.UID3 = 0xE62546C5

# Smart Installer package's UID
# This UID is from the protected range and therefore the package will
# fail to install if self-signed. By default qmake uses the unprotected
# range value if unprotected UID is defined for the application and
# 0x2002CCCF value if protected UID is given to the application
#symbian:DEPLOYMENT.installer_header = 0x2002CCCF

# Allow network access on Symbian
symbian:TARGET.CAPABILITY += NetworkServices

# Native Avkon file/folder pickers (AknCommonDialogsDynMem)
symbian:LIBS += -lcommondialogs
# Native information notes (CAknInformationNote)
symbian:LIBS += -lavkon -leikdlg -leikctl

# Gallery (Photos) fetcher for the Send page's Media button
symbian:LIBS += -lmgfetch -lbafl
# History page's Open folder: starts the File manager app (src/historymodel.cpp)
symbian:LIBS += -lapgrfx -lapparc -lcone -lws32
# Received files keep the time of the original (src/receiver.cpp)
symbian:LIBS += -lefsrv
# The fetcher talks to the Photos metadata/MPX/thumbnail servers, which refuse
# clients without these capabilities (MGFetch::RunL leaves with -46
# otherwise). ReadUserData/WriteUserData alone were not enough on a Belle
# 808. ReadDeviceData, WriteDeviceData and SwEvent are developer-certificate
# capabilities: the self-signed .sis then only installs on phones that accept
# unsigned packages (patched installserver).
symbian:TARGET.CAPABILITY += ReadUserData WriteUserData UserEnvironment LocalServices     ReadDeviceData WriteDeviceData SwEvent

symbian:TARGET.EPOCHEAPSIZE = 0x20000 0x2000000

# TLS client certificate (src/clientidentity.cpp), created with OpenSSL's
# libcrypto, the library Qt's SSL uses too (Open C on Symbian).
symbian|unix {
    DEFINES += SYMBISEND_CLIENT_CERT
    symbian:LIBS += -llibcrypto
    else:LIBS += -lcrypto
}

# Speed up launching on MeeGo/Harmattan when using applauncherd daemon
CONFIG += qdeclarative-boostable

# Qt Quick Components for Symbian
symbian:CONFIG += qt-components

INCLUDEPATH += src

# mbedTLS, compiled in (no system-wide TLS library needed)
include(external/mbedtls/mbedtls.pri)

SOURCES += main.cpp \
    src/logger.cpp \
    src/json.cpp \
    src/deviceinfo.cpp \
    src/peermodel.cpp \
    src/networkmanager.cpp \
    src/httpserver.cpp \
    src/multicastreceiver.cpp \
    src/discovery.cpp \
    src/settings.cpp \
    src/picker.cpp \
    src/selectionmodel.cpp \
    src/historymodel.cpp \
    src/favoritesmodel.cpp \
    src/sender.cpp \
    src/receiver.cpp \
    src/receivefilesmodel.cpp \
    src/webshare.cpp \
    src/clientidentity.cpp \
    src/tlsrandom.cpp \
    src/tlsserver.cpp \
    src/sha256hash.cpp \
    src/verification.cpp

HEADERS += \
    src/logger.h \
    src/json.h \
    src/deviceinfo.h \
    src/peermodel.h \
    src/networkmanager.h \
    src/httpserver.h \
    src/multicastreceiver.h \
    src/discovery.h \
    src/settings.h \
    src/picker.h \
    src/selectionmodel.h \
    src/historymodel.h \
    src/favoritesmodel.h \
    src/sender.h \
    src/receiver.h \
    src/receivefilesmodel.h \
    src/webshare.h \
    src/clientidentity.h \
    src/tlsrandom.h \
    src/tlsserver.h \
    src/sha256hash.h \
    src/verification.h

# Pages served to web browsers (official LocalSend assets, see web/README.md)
RESOURCES += web.qrc

# QML UI and its images, loaded from qrc:/qml/Symbisend/
RESOURCES += qml.qrc

# LocalSend's Verify icons (fingerprint-icons/README.md)
RESOURCES += fingerprint-icons.qrc

# Please do not modify the following two lines. Required for deployment.
include(qmlapplicationviewer/qmlapplicationviewer.pri)
qtcAddDeployment()

OTHER_FILES += \
    qtc_packaging/debian_harmattan/rules \
    qtc_packaging/debian_harmattan/README \
    qtc_packaging/debian_harmattan/manifest.aegis \
    qtc_packaging/debian_harmattan/copyright \
    qtc_packaging/debian_harmattan/control \
    qtc_packaging/debian_harmattan/compat \
    qtc_packaging/debian_harmattan/changelog
