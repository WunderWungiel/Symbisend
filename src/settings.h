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
// quickSave, requirePin/pin, downloadFolder and receiveViaLinkAutoAccept are
// used by Receiver; the
// other receive settings are stored only for now.
class Settings : public QObject
{
    Q_OBJECT
    // General
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)

    // Receive
    Q_PROPERTY(bool quickSave READ quickSave WRITE setQuickSave NOTIFY quickSaveChanged)
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

    // Qt-style path ("F:/Symbisend") for C++ code that writes files.
    QString downloadFolder() const { return m_downloadFolder; }
    QString downloadFolderNative() const;
    bool downloadFolderIsDefault() const { return m_custom.isEmpty(); }

    // Opens the native folder picker (asynchronously: returns at once, the
    // property changes when the user confirms a folder).
    Q_INVOKABLE void pickDownloadFolder();
    // Forgets the user's choice and goes back to the automatic default.
    Q_INVOKABLE void resetDownloadFolder();

signals:
    void themeChanged();
    void languageChanged();
    void quickSaveChanged();
    void requirePinChanged();
    void pinChanged();
    void downloadFolderChanged();
    void saveMediaToGalleryChanged();
    void autoFinishChanged();
    void saveToHistoryChanged();
    void receiveViaLinkAutoAcceptChanged();

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
    bool m_requirePin;
    QString m_pin;
    bool m_saveMediaToGallery;
    bool m_autoFinish;
    bool m_saveToHistory;
    bool m_receiveViaLinkAutoAccept;

    QString m_custom;          // user's choice, "" = use default
    QString m_downloadFolder;  // effective folder
};

#endif // SETTINGS_H
