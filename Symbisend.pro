# Add more folders to ship with the application, here
folder_01.source = qml/Symbisend
folder_01.target = qml
DEPLOYMENTFOLDERS = folder_01

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

symbian:TARGET.EPOCHEAPSIZE = 0x20000 0x2000000

# Speed up launching on MeeGo/Harmattan when using applauncherd daemon
CONFIG += qdeclarative-boostable

# Qt Quick Components for Symbian
symbian:CONFIG += qt-components

INCLUDEPATH += src

SOURCES += main.cpp \
    src/logger.cpp \
    src/json.cpp \
    src/deviceinfo.cpp \
    src/peermodel.cpp \
    src/networkmanager.cpp \
    src/httpserver.cpp \
    src/multicastreceiver.cpp \
    src/discovery.cpp

HEADERS += \
    src/logger.h \
    src/json.h \
    src/deviceinfo.h \
    src/peermodel.h \
    src/networkmanager.h \
    src/httpserver.h \
    src/multicastreceiver.h \
    src/discovery.h

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
