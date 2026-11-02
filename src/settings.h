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
// Most of the other settings are stored only; nothing acts on them yet.
class Settings : public QObject
{
    Q_OBJECT
    // General
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)

    // Receive
    Q_PROPERTY(bool quickSave READ quickSave WRITE setQuickSave NOTIFY quickSaveChanged)
    Q_PROPERTY(bool requirePin READ requirePin WRITE setRequirePin NOTIFY requirePinChanged)
    // Native-separator path for display, e.g. "F:\Symbisend".
    Q_PROPERTY(QString downloadFolder READ downloadFolderNative NOTIFY downloadFolderChanged)
    Q_PROPERTY(bool downloadFolderIsDefault READ downloadFolderIsDefault NOTIFY downloadFolderChanged)
    Q_PROPERTY(bool saveMediaToGallery READ saveMediaToGallery WRITE setSaveMediaToGallery NOTIFY saveMediaToGalleryChanged)
    Q_PROPERTY(bool autoFinish READ autoFinish WRITE setAutoFinish NOTIFY autoFinishChanged)
    Q_PROPERTY(bool saveToHistory READ saveToHistory WRITE setSaveToHistory NOTIFY saveToHistoryChanged)

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
    bool saveMediaToGallery() const { return m_saveMediaToGallery; }
    void setSaveMediaToGallery(bool on);
    bool autoFinish() const { return m_autoFinish; }
    void setAutoFinish(bool on);
    bool saveToHistory() const { return m_saveToHistory; }
    void setSaveToHistory(bool on);

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
    void downloadFolderChanged();
    void saveMediaToGalleryChanged();
    void autoFinishChanged();
    void saveToHistoryChanged();

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
    bool m_saveMediaToGallery;
    bool m_autoFinish;
    bool m_saveToHistory;

    QString m_custom;          // user's choice, "" = use default
    QString m_downloadFolder;  // effective folder
};

#endif // SETTINGS_H
