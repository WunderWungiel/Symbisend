#ifndef SETTINGS_H
#define SETTINGS_H

#include <QObject>
#include <QString>

// User-facing app settings, persisted in QSettings.
//
// downloadFolder: where received files go. Unless the user picked a folder,
// it is the default for the drives present right now (F: > E: > C:/Data),
// recomputed on every launch so inserting a memory card is picked up.
//
// quickSave, quickSaveFromFavorites, requirePin/pin, downloadFolder and receiveViaLinkAutoAccept are
// used by Receiver; autoFinish by ReceiveDialog.qml and SendDialog.qml; the
// other receive settings are stored only for now.
class Settings : public QObject
{
    Q_OBJECT
    // General
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)

    // Receive
    Q_PROPERTY(bool quickSave READ quickSave WRITE setQuickSave NOTIFY quickSaveChanged)
    // Like quickSave, but only for senders in `favorites`.
    Q_PROPERTY(bool quickSaveFromFavorites READ quickSaveFromFavorites WRITE setQuickSaveFromFavorites NOTIFY quickSaveFromFavoritesChanged)
    // requirePin can only be on with a non-empty pin; turning it off clears the pin.
    Q_PROPERTY(bool requirePin READ requirePin WRITE setRequirePin NOTIFY requirePinChanged)
    Q_PROPERTY(QString pin READ pin WRITE setPin NOTIFY pinChanged)
    // Native-separator path for display, e.g. "F:\Symbisend".
    Q_PROPERTY(QString downloadFolder READ downloadFolderNative NOTIFY downloadFolderChanged)
    Q_PROPERTY(bool downloadFolderIsDefault READ downloadFolderIsDefault NOTIFY downloadFolderChanged)
    Q_PROPERTY(bool saveMediaToGallery READ saveMediaToGallery WRITE setSaveMediaToGallery NOTIFY saveMediaToGalleryChanged)
    Q_PROPERTY(bool autoFinish READ autoFinish WRITE setAutoFinish NOTIFY autoFinishChanged)
    Q_PROPERTY(bool saveToHistory READ saveToHistory WRITE setSaveToHistory NOTIFY saveToHistoryChanged)
    // While "Receive via link" is on, accept every request at once.
    Q_PROPERTY(bool receiveViaLinkAutoAccept READ receiveViaLinkAutoAccept WRITE setReceiveViaLinkAutoAccept NOTIFY receiveViaLinkAutoAcceptChanged)

    // Send
    // Hash every file before offering it, so that the receiver can check it
    // (official app: on by default; off here, it is slow on a phone).
    Q_PROPERTY(bool createChecksums READ createChecksums WRITE setCreateChecksums NOTIFY createChecksumsChanged)

    // Network
    // Our server speaks https (default) instead of http. Discovery restarts
    // the server when it changes (except while "Receive via link" is on,
    // which has its own toggle, webShare.encrypted).
    Q_PROPERTY(bool encryption READ encryption WRITE setEncryption NOTIFY encryptionChanged)

public:
    explicit Settings(QObject *parent = 0);

    // "dark" (default) or "light".
    QString theme() const { return m_theme; }
    void setTheme(const QString &theme);
    // "system" (default) or a language code ("en"). No translations yet.
    QString language() const { return m_language; }
    void setLanguage(const QString &language);

    bool quickSave() const { return m_quickSave; }
    void setQuickSave(bool on);
    bool quickSaveFromFavorites() const { return m_quickSaveFromFavorites; }
    void setQuickSaveFromFavorites(bool on);
    bool requirePin() const { return m_requirePin; }
    void setRequirePin(bool on);
    // Any text, not only digits. Setting an empty pin is ignored while
    // requirePin is on (turn requirePin off instead).
    QString pin() const { return m_pin; }
    void setPin(const QString &pin);
    bool saveMediaToGallery() const { return m_saveMediaToGallery; }
    void setSaveMediaToGallery(bool on);
    bool autoFinish() const { return m_autoFinish; }
    void setAutoFinish(bool on);
    bool saveToHistory() const { return m_saveToHistory; }
    void setSaveToHistory(bool on);
    bool receiveViaLinkAutoAccept() const { return m_receiveViaLinkAutoAccept; }
    void setReceiveViaLinkAutoAccept(bool on);
    bool createChecksums() const { return m_createChecksums; }
    void setCreateChecksums(bool on);
    bool encryption() const { return m_encryption; }
    void setEncryption(bool on);

    // Qt-style path ("F:/Symbisend") for C++ code that writes files.
    QString downloadFolder() const { return m_downloadFolder; }
    QString downloadFolderNative() const;
    bool downloadFolderIsDefault() const { return m_custom.isEmpty(); }
    // Where received images (video false) or videos go when
    // saveMediaToGallery is on, Qt-style. Symbian: the phone's standard media
    // folders on the download folder's drive ("F:/Images/Symbisend",
    // "E:/Videos/Symbisend", "C:/Data/Images/Symbisend"), which the Photos app
    // indexes like the camera's. Elsewhere: the system pictures/movies folder.
    QString mediaFolder(bool video) const;

    // Opens the native folder picker (asynchronously: returns at once, the
    // property changes when the user confirms a folder).
    Q_INVOKABLE void pickDownloadFolder();
    // Forgets the user's choice and goes back to the automatic default.
    Q_INVOKABLE void resetDownloadFolder();

    // The native folder picker, starting at `start` (Qt separators). Blocks in
    // a nested event loop, so never call it from a QML click handler. Returns
    // a clean Qt path without trailing slash ("F:/Music", but "F:/" for a
    // bare drive), or "" if cancelled.
    static QString chooseFolder(const QString &start, const QString &title);

signals:
    void themeChanged();
    void languageChanged();
    void quickSaveChanged();
    void quickSaveFromFavoritesChanged();
    void requirePinChanged();
    void pinChanged();
    void downloadFolderChanged();
    void saveMediaToGalleryChanged();
    void autoFinishChanged();
    void saveToHistoryChanged();
    void receiveViaLinkAutoAcceptChanged();
    void createChecksumsChanged();
    void encryptionChanged();

private slots:
    void runFolderPicker();

private:
    static QString defaultDownloadFolder();
    void setCustomDownloadFolder(const QString &path);
    void updateDownloadFolder();
    // Stores value under key if it differs from *member; returns true if it did.
    static bool store(const char *key, bool *member, bool value);
    static bool store(const char *key, QString *member, const QString &value);

    QString m_theme;
    QString m_language;
    bool m_quickSave;
    bool m_quickSaveFromFavorites;
    bool m_requirePin;
    QString m_pin;
    bool m_saveMediaToGallery;
    bool m_autoFinish;
    bool m_saveToHistory;
    bool m_receiveViaLinkAutoAccept;
    bool m_createChecksums;
    bool m_encryption;

    QString m_custom;          // user's choice, "" = use default
    QString m_downloadFolder;  // effective folder
};

#endif // SETTINGS_H
