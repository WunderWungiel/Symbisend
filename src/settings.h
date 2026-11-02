#ifndef SETTINGS_H
#define SETTINGS_H

#include <QObject>
#include <QString>

// User-facing app settings, persisted in QSettings.
//
// downloadFolder: where received files go. Unless the user picked a folder,
// it is the default for the drives present right now (F: > E: > C:/Data),
// recomputed on every launch so inserting a memory card is picked up.
class Settings : public QObject
{
    Q_OBJECT
    // Native-separator path for display, e.g. "F:\Symbisend".
    Q_PROPERTY(QString downloadFolder READ downloadFolderNative NOTIFY downloadFolderChanged)
    Q_PROPERTY(bool downloadFolderIsDefault READ downloadFolderIsDefault NOTIFY downloadFolderChanged)

public:
    explicit Settings(QObject *parent = 0);

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
    void downloadFolderChanged();

private slots:
    void runFolderPicker();

private:
    static QString defaultDownloadFolder();
    void setCustomDownloadFolder(const QString &path);
    void updateDownloadFolder();

    QString m_custom;          // user's choice, "" = use default
    QString m_downloadFolder;  // effective folder
};

#endif // SETTINGS_H
