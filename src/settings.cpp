// SPDX-FileCopyrightText: 2026 Wunder Wungiel
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings.h"

#include "i18n.h"
#include <QDebug>
#include <QDir>
#include <QHostAddress>
#include <QSettings>
#include <QTimer>

#ifdef Q_OS_SYMBIAN
#include <AknCommonDialogsDynMem.h>
#else
#include <QDesktopServices>
#include <QFileDialog>
#endif

static const char *kAdvancedSettingsKey = "general/advancedSettings";
static const char *kThemeKey = "general/theme";
static const char *kLanguageKey = "general/language";
static const char *kQuickSaveKey = "receive/quickSave";
static const char *kQuickSaveFromFavoritesKey = "receive/quickSaveFromFavorites";
static const char *kRequirePinKey = "receive/requirePin";
static const char *kPinKey = "receive/pin";
static const char *kDownloadFolderKey = "transfer/downloadFolder";
static const char *kSaveMediaToGalleryKey = "receive/saveMediaToGallery";
static const char *kAutoFinishKey = "receive/autoFinish";
static const char *kSaveToHistoryKey = "receive/saveToHistory";
static const char *kReceiveViaLinkAutoAcceptKey = "receive/viaLinkAutoAccept";
static const char *kVerifyChecksumsKey = "receive/verifyChecksums";
static const char *kCreateChecksumsKey = "send/createChecksums";
static const char *kPortKey = "network/port";
static const char *kMulticastGroupKey = "network/multicastGroup";
static const char *kDiscoveryTimeoutKey = "network/discoveryTimeout";
static const char *kEncryptionKey = "network/encryption";

static const char *kDefaultMulticastGroup = "224.0.0.167";
static const int kDefaultDiscoveryTimeoutMs = 3000;
static const int kMaxDiscoveryTimeoutMs = 60000;

// Anything else would leave the multicast listener failing for ever. Gives
// the address in its usual form, or "" if it isn't one.
static QString cleanMulticastGroup(const QString &text)
{
    QHostAddress address;
    if (!address.setAddress(text.trimmed())
            || address.protocol() != QAbstractSocket::IPv4Protocol
            || (address.toIPv4Address() >> 28) != 0xE)
        return QString();
    return address.toString();
}

Settings::Settings(QObject *parent)
    : QObject(parent)
{
    QSettings s;
    m_advancedSettings = s.value(QLatin1String(kAdvancedSettingsKey), false).toBool();
    m_theme = s.value(QLatin1String(kThemeKey), QLatin1String("dark")).toString();
    if (m_theme != QLatin1String("dark") && m_theme != QLatin1String("light"))
        m_theme = QLatin1String("dark");
    m_language = s.value(QLatin1String(kLanguageKey), QLatin1String("system")).toString();
    if (m_language.isEmpty())
        m_language = QLatin1String("system");
    m_quickSave = s.value(QLatin1String(kQuickSaveKey), false).toBool();
    m_quickSaveFromFavorites = s.value(QLatin1String(kQuickSaveFromFavoritesKey), false).toBool();
    m_requirePin = s.value(QLatin1String(kRequirePinKey), false).toBool();
    m_pin = s.value(QLatin1String(kPinKey)).toString();
    if (m_requirePin && m_pin.isEmpty()) {
        qWarning() << "Settings: requirePin was on without a PIN, turning it off";
        m_requirePin = false;
        s.setValue(QLatin1String(kRequirePinKey), false);
    }
    m_saveMediaToGallery = s.value(QLatin1String(kSaveMediaToGalleryKey), true).toBool();
    m_autoFinish = s.value(QLatin1String(kAutoFinishKey), false).toBool();
    m_saveToHistory = s.value(QLatin1String(kSaveToHistoryKey), true).toBool();
    m_receiveViaLinkAutoAccept = s.value(QLatin1String(kReceiveViaLinkAutoAcceptKey), false).toBool();
    m_verifyChecksums = s.value(QLatin1String(kVerifyChecksumsKey), true).toBool();
    m_createChecksums = s.value(QLatin1String(kCreateChecksumsKey), false).toBool();
    m_port = s.value(QLatin1String(kPortKey), int(DefaultPort)).toInt();
    if (m_port < 1 || m_port > 65535)
        m_port = DefaultPort;
    m_multicastGroup = cleanMulticastGroup(s.value(QLatin1String(kMulticastGroupKey)).toString());
    if (m_multicastGroup.isEmpty())
        m_multicastGroup = defaultMulticastGroup();
    m_discoveryTimeout = s.value(QLatin1String(kDiscoveryTimeoutKey), kDefaultDiscoveryTimeoutMs).toInt();
    if (m_discoveryTimeout < 1 || m_discoveryTimeout > kMaxDiscoveryTimeoutMs)
        m_discoveryTimeout = kDefaultDiscoveryTimeoutMs;
    m_encryption = s.value(QLatin1String(kEncryptionKey), true).toBool();
    qDebug() << "Settings: advanced" << m_advancedSettings
             << "theme" << m_theme << "language" << m_language
             << "quickSave" << m_quickSave
             << "quickSaveFromFavorites" << m_quickSaveFromFavorites << "requirePin" << m_requirePin
             << "saveMediaToGallery" << m_saveMediaToGallery
             << "autoFinish" << m_autoFinish << "saveToHistory" << m_saveToHistory
             << "receiveViaLinkAutoAccept" << m_receiveViaLinkAutoAccept
             << "verifyChecksums" << m_verifyChecksums
             << "createChecksums" << m_createChecksums
             << "port" << m_port << "multicastGroup" << m_multicastGroup
             << "discoveryTimeout" << m_discoveryTimeout
             << "encryption" << m_encryption;

    m_custom = s.value(QLatin1String(kDownloadFolderKey)).toString();
    updateDownloadFolder();
}

bool Settings::store(const char *key, bool *member, bool value)
{
    if (*member == value)
        return false;
    *member = value;
    QSettings().setValue(QLatin1String(key), value);
    qDebug() << "Settings:" << key << "=" << value;
    return true;
}

bool Settings::store(const char *key, QString *member, const QString &value)
{
    if (*member == value)
        return false;
    *member = value;
    QSettings().setValue(QLatin1String(key), value);
    qDebug() << "Settings:" << key << "=" << value;
    return true;
}

void Settings::restoreDefaults()
{
    qDebug() << "Settings: restoring the defaults";
    setAdvancedSettings(false);
    setTheme(QLatin1String("dark"));
    setLanguage(QLatin1String("system"));
    setQuickSave(false);
    setQuickSaveFromFavorites(false);
    setRequirePin(false);
    setPin(QString());
    resetDownloadFolder();
    setSaveMediaToGallery(true);
    setAutoFinish(false);
    setSaveToHistory(true);
    setReceiveViaLinkAutoAccept(false);
    setVerifyChecksums(true);
    setCreateChecksums(false);
    setPort(DefaultPort);
    setMulticastGroup(defaultMulticastGroup());
    setDiscoveryTimeout(kDefaultDiscoveryTimeoutMs);
    setEncryption(true);
}

void Settings::setAdvancedSettings(bool on)
{
    if (store(kAdvancedSettingsKey, &m_advancedSettings, on))
        emit advancedSettingsChanged();
}

void Settings::setTheme(const QString &theme)
{
    if (theme != QLatin1String("dark") && theme != QLatin1String("light")) {
        qWarning() << "Settings: unknown theme" << theme;
        return;
    }
    if (store(kThemeKey, &m_theme, theme))
        emit themeChanged();
}

void Settings::setLanguage(const QString &language)
{
    if (language.isEmpty())
        return;
    if (store(kLanguageKey, &m_language, language))
        emit languageChanged();
}

void Settings::setQuickSave(bool on)
{
    if (store(kQuickSaveKey, &m_quickSave, on))
        emit quickSaveChanged();
}

void Settings::setQuickSaveFromFavorites(bool on)
{
    if (store(kQuickSaveFromFavoritesKey, &m_quickSaveFromFavorites, on))
        emit quickSaveFromFavoritesChanged();
}

void Settings::setRequirePin(bool on)
{
    if (on && m_pin.isEmpty()) {
        qWarning() << "Settings: requirePin needs a PIN first, ignored";
        return;
    }
    if (!store(kRequirePinKey, &m_requirePin, on))
        return;
    emit requirePinChanged();
    if (!on && !m_pin.isEmpty()) {
        m_pin.clear();
        QSettings().remove(QLatin1String(kPinKey));
        qDebug() << "Settings: PIN cleared";
        emit pinChanged();
    }
}

void Settings::setPin(const QString &pin)
{
    if (pin == m_pin)
        return;
    if (pin.isEmpty() && m_requirePin) {
        qWarning() << "Settings: empty PIN ignored while requirePin is on";
        return;
    }
    m_pin = pin;
    // Not through store(): keep the PIN itself out of the log.
    QSettings().setValue(QLatin1String(kPinKey), m_pin);
    qDebug() << "Settings: PIN set," << m_pin.length() << "characters";
    emit pinChanged();
}

void Settings::setSaveMediaToGallery(bool on)
{
    if (store(kSaveMediaToGalleryKey, &m_saveMediaToGallery, on))
        emit saveMediaToGalleryChanged();
}

void Settings::setAutoFinish(bool on)
{
    if (store(kAutoFinishKey, &m_autoFinish, on))
        emit autoFinishChanged();
}

void Settings::setSaveToHistory(bool on)
{
    if (store(kSaveToHistoryKey, &m_saveToHistory, on))
        emit saveToHistoryChanged();
}

void Settings::setReceiveViaLinkAutoAccept(bool on)
{
    if (store(kReceiveViaLinkAutoAcceptKey, &m_receiveViaLinkAutoAccept, on))
        emit receiveViaLinkAutoAcceptChanged();
}

void Settings::setVerifyChecksums(bool on)
{
    if (store(kVerifyChecksumsKey, &m_verifyChecksums, on))
        emit verifyChecksumsChanged();
}

void Settings::setCreateChecksums(bool on)
{
    if (store(kCreateChecksumsKey, &m_createChecksums, on))
        emit createChecksumsChanged();
}

void Settings::setPort(int port)
{
    if (port < 1 || port > 65535) {
        qWarning() << "Settings: port" << port << "is out of range, ignored";
        return;
    }
    if (port == m_port)
        return;
    m_port = port;
    QSettings().setValue(QLatin1String(kPortKey), port);
    qDebug() << "Settings: port" << port;
    emit portChanged();
}

QString Settings::defaultMulticastGroup() const
{
    return QLatin1String(kDefaultMulticastGroup);
}

void Settings::setMulticastGroup(const QString &group)
{
    const QString clean = cleanMulticastGroup(group);
    if (clean.isEmpty()) {
        qWarning() << "Settings:" << group << "is not a multicast address, ignored";
        return;
    }
    if (store(kMulticastGroupKey, &m_multicastGroup, clean))
        emit multicastGroupChanged();
}

void Settings::setDiscoveryTimeout(int ms)
{
    if (ms < 1 || ms > kMaxDiscoveryTimeoutMs) {
        qWarning() << "Settings: discovery timeout" << ms << "is out of range, ignored";
        return;
    }
    if (ms == m_discoveryTimeout)
        return;
    m_discoveryTimeout = ms;
    QSettings().setValue(QLatin1String(kDiscoveryTimeoutKey), ms);
    qDebug() << "Settings: discovery timeout" << ms << "ms";
    emit discoveryTimeoutChanged();
}

void Settings::setEncryption(bool on)
{
    if (store(kEncryptionKey, &m_encryption, on))
        emit encryptionChanged();
}

QString Settings::downloadFolderNative() const
{
    return QDir::toNativeSeparators(m_downloadFolder);
}

QString Settings::defaultDownloadFolder()
{
#ifdef Q_OS_SYMBIAN
    // Prefer the memory card (F:), then mass memory (E:), then phone memory.
    const char *candidates[] = { "F:/download/Symbisend", "E:/download/Symbisend",
                                 "C:/Data/download/Symbisend" };
    for (unsigned i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        QString path = QString::fromLatin1(candidates[i]);
        if (QDir(path.left(3)).exists() && QDir().mkpath(path))
            return path;
        qDebug() << "Settings: default download folder" << path << "not usable";
    }
    return QLatin1String("C:/Data/download/Symbisend");
#else
    return QDir::homePath() + QLatin1String("/Downloads/Symbisend");
#endif
}

QString Settings::mediaFolder(bool video) const
{
    const QString sub = QLatin1String(video ? "Videos/Symbisend" : "Images/Symbisend");
#ifdef Q_OS_SYMBIAN
    // Same drive as the download folder; on C: user files live under C:/Data.
    QString drive = m_downloadFolder.left(2).toUpper();
    if (drive.size() != 2 || drive.at(1) != QLatin1Char(':'))
        drive = QLatin1String("C:");
    if (drive == QLatin1String("C:"))
        drive = QLatin1String("C:/Data");
    return drive + QLatin1Char('/') + sub;
#else
    const QString base = QDesktopServices::storageLocation(
                video ? QDesktopServices::MoviesLocation : QDesktopServices::PicturesLocation);
    return base.isEmpty() ? m_downloadFolder + QLatin1Char('/') + sub
                          : QDir::fromNativeSeparators(base) + QLatin1String("/Symbisend");
#endif
}

void Settings::updateDownloadFolder()
{
    QString folder;
    if (!m_custom.isEmpty()) {
        if (QDir(m_custom).exists() || QDir().mkpath(m_custom))
            folder = m_custom;
        else
            // E.g. the memory card was removed. Keep the choice for next time.
            qWarning() << "Settings: chosen download folder" << m_custom
                       << "is not available, using the default";
    }
    if (folder.isEmpty())
        folder = defaultDownloadFolder();

    if (folder != m_downloadFolder) {
        m_downloadFolder = folder;
        qDebug() << "Settings: download folder" << m_downloadFolder
                 << (m_custom.isEmpty() ? "(default)" : "(chosen)");
        emit downloadFolderChanged();
    }
}

void Settings::setCustomDownloadFolder(const QString &path)
{
    bool wasDefault = m_custom.isEmpty();
    m_custom = path;
    QSettings().setValue(QLatin1String(kDownloadFolderKey), m_custom);
    updateDownloadFolder();
    if (wasDefault)
        emit downloadFolderChanged(); // downloadFolderIsDefault changed
}

void Settings::resetDownloadFolder()
{
    if (m_custom.isEmpty())
        return;
    qDebug() << "Settings: download folder reset to default";
    m_custom.clear();
    QSettings().remove(QLatin1String(kDownloadFolderKey));
    updateDownloadFolder();
    emit downloadFolderChanged(); // downloadFolderIsDefault changed
}

void Settings::pickDownloadFolder()
{
    // The native dialog runs a nested event loop; don't start it from inside
    // the QML click handler.
    QTimer::singleShot(0, this, SLOT(runFolderPicker()));
}

void Settings::runFolderPicker()
{
    const QString picked = chooseFolder(m_downloadFolder, I18n::text("settingsTab.receive.destination"));
    if (!picked.isEmpty())
        setCustomDownloadFolder(picked);
}

QString Settings::chooseFolder(const QString &startFolder, const QString &title)
{
    qDebug() << "Settings: opening folder picker at" << startFolder;
    QString picked;

#ifdef Q_OS_SYMBIAN
    QString start = QDir::toNativeSeparators(startFolder);
    if (!start.endsWith(QLatin1Char('\\')))
        start += QLatin1Char('\\');
    TPtrC startPtr(reinterpret_cast<const TUint16 *>(start.utf16()), start.length());

    TPtrC titlePtr(reinterpret_cast<const TUint16 *>(title.utf16()), title.length());

    TInt medias = AknCommonDialogsDynMem::EMemoryTypePhone
            | AknCommonDialogsDynMem::EMemoryTypeInternalMassStorage
            | AknCommonDialogsDynMem::EMemoryTypeMMCExternal;

    TFileName folder;
    TBool ok = EFalse;
    // Resource ids 0 = the dialogs' default resources.
    TRAPD(err, ok = AknCommonDialogsDynMem::RunFolderSelectDlgLD(
                medias, folder, startPtr, 0, 0, titlePtr));
    if (err != KErrNone) {
        qWarning() << "Settings: folder picker failed, error" << err;
        return QString();
    }
    if (ok)
        picked = QString::fromUtf16(folder.Ptr(), folder.Length());
#else
    picked = QFileDialog::getExistingDirectory(0, title, startFolder);
#endif

    if (picked.isEmpty()) {
        qDebug() << "Settings: folder picker cancelled";
        return QString();
    }
    qDebug() << "Settings: picked folder" << picked;
    QString result = QDir::cleanPath(QDir::fromNativeSeparators(picked));
    if (result.length() == 2 && result.at(1) == QLatin1Char(':'))
        result += QLatin1Char('/');
    return result;
}
