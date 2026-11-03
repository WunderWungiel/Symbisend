#include "settings.h"

#include <QDebug>
#include <QDir>
#include <QSettings>
#include <QTimer>

#ifdef Q_OS_SYMBIAN
#include <AknCommonDialogsDynMem.h>
#else
#include <QFileDialog>
#endif

static const char *kThemeKey = "general/theme";
static const char *kLanguageKey = "general/language";
static const char *kQuickSaveKey = "receive/quickSave";
static const char *kRequirePinKey = "receive/requirePin";
static const char *kPinKey = "receive/pin";
static const char *kDownloadFolderKey = "transfer/downloadFolder";
static const char *kSaveMediaToGalleryKey = "receive/saveMediaToGallery";
static const char *kAutoFinishKey = "receive/autoFinish";
static const char *kSaveToHistoryKey = "receive/saveToHistory";
static const char *kReceiveViaLinkAutoAcceptKey = "receive/viaLinkAutoAccept";

Settings::Settings(QObject *parent)
    : QObject(parent)
{
    QSettings s;
    m_theme = s.value(QLatin1String(kThemeKey), QLatin1String("dark")).toString();
    if (m_theme != QLatin1String("dark") && m_theme != QLatin1String("light"))
        m_theme = QLatin1String("dark");
    m_language = s.value(QLatin1String(kLanguageKey), QLatin1String("system")).toString();
    if (m_language.isEmpty())
        m_language = QLatin1String("system");
    m_quickSave = s.value(QLatin1String(kQuickSaveKey), false).toBool();
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
    qDebug() << "Settings: theme" << m_theme << "language" << m_language
             << "quickSave" << m_quickSave << "requirePin" << m_requirePin
             << "saveMediaToGallery" << m_saveMediaToGallery
             << "autoFinish" << m_autoFinish << "saveToHistory" << m_saveToHistory
             << "receiveViaLinkAutoAccept" << m_receiveViaLinkAutoAccept;

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

QString Settings::downloadFolderNative() const
{
    return QDir::toNativeSeparators(m_downloadFolder);
}

QString Settings::defaultDownloadFolder()
{
#ifdef Q_OS_SYMBIAN
    // Prefer the memory card (F:), then mass memory (E:), then phone memory.
    const char *candidates[] = { "F:/Symbisend", "E:/Symbisend", "C:/Data/Symbisend" };
    for (unsigned i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        QString path = QString::fromLatin1(candidates[i]);
        if (QDir(path.left(3)).exists() && QDir().mkpath(path))
            return path;
        qDebug() << "Settings: default download folder" << path << "not usable";
    }
    return QLatin1String("C:/Data/Symbisend");
#else
    return QDir::homePath() + QLatin1String("/Downloads/Symbisend");
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
    // Store as a clean Qt path without trailing slash ("F:/Music"), but keep
    // a bare drive root as "F:/".
    QString p = QDir::cleanPath(QDir::fromNativeSeparators(path));
    if (p.length() == 2 && p.at(1) == QLatin1Char(':'))
        p += QLatin1Char('/');

    bool wasDefault = m_custom.isEmpty();
    m_custom = p;
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
    qDebug() << "Settings: opening folder picker at" << m_downloadFolder;
    QString picked;

#ifdef Q_OS_SYMBIAN
    QString start = QDir::toNativeSeparators(m_downloadFolder);
    if (!start.endsWith(QLatin1Char('\\')))
        start += QLatin1Char('\\');
    TPtrC startPtr(reinterpret_cast<const TUint16 *>(start.utf16()), start.length());

    QString title = QLatin1String("Download folder");
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
        return;
    }
    if (ok)
        picked = QString::fromUtf16(folder.Ptr(), folder.Length());
#else
    picked = QFileDialog::getExistingDirectory(0, QLatin1String("Download folder"),
                                               m_downloadFolder);
#endif

    if (picked.isEmpty()) {
        qDebug() << "Settings: folder picker cancelled";
        return;
    }
    qDebug() << "Settings: picked folder" << picked;
    setCustomDownloadFolder(picked);
}
