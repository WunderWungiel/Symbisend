#ifndef PICKER_H
#define PICKER_H

#include <QObject>
#include <QString>
#include <QStringList>

// Native pickers for the Send page: file, media (gallery), folder and the
// clipboard. On Symbian these are the Avkon common dialogs and the Photos
// fetcher (MGFetch); elsewhere QFileDialog.
//
// The pick*() methods return at once; the dialogs run a nested event loop, so
// they are started from the event loop rather than from the QML click
// handler. The result arrives as filesPicked()/folderPicked(); nothing is
// emitted on cancel. For now the results are only logged.
class Picker : public QObject
{
    Q_OBJECT
    // Whether the clipboard holds text (for enabling the Paste button).
    // Updated when this app changes the clipboard (QClipboard::dataChanged);
    // a copy made in another app is not signalled on Symbian, so QML calls
    // checkClipboard() when the app becomes active / the Send page shows.
    Q_PROPERTY(bool clipboardHasText READ clipboardHasText NOTIFY clipboardHasTextChanged)

public:
    explicit Picker(QObject *parent = 0);

    // One file from the phone's drives.
    Q_INVOKABLE void pickFile();
    // Photos/videos from the gallery, several at once. kind: "image" or
    // "video" (the gallery fetcher shows one type at a time).
    Q_INVOKABLE void pickMedia(const QString &kind);
    // A folder.
    Q_INVOKABLE void pickFolder();
    // The clipboard's text ("" if empty).
    Q_INVOKABLE QString clipboardText() const;
    // Puts text on the clipboard.
    Q_INVOKABLE void copyText(const QString &text);
    bool clipboardHasText() const { return m_clipboardHasText; }
    // A short native information note (Avkon CAknInformationNote on Symbian,
    // a message box elsewhere). Returns at once; the note hides by itself.
    Q_INVOKABLE void showInfo(const QString &text);

public slots:
    // Re-reads the clipboard and updates clipboardHasText (callable from QML).
    void checkClipboard();

signals:
    void filesPicked(const QStringList &paths);
    void folderPicked(const QString &path);
    void clipboardHasTextChanged();

private slots:
    void runFilePicker();
    void runMediaPicker();
    void runFolderPicker();

private:
    void reportFiles(const char *what, const QStringList &paths);

    QString m_mediaKind;
    bool m_clipboardHasText;
};

#endif // PICKER_H
