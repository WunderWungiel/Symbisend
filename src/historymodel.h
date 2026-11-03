#ifndef HISTORYMODEL_H
#define HISTORYMODEL_H

#include <QAbstractListModel>
#include <QDateTime>
#include <QList>
#include <QLocale>
#include <QString>
#include <QTimer>
#include <QVariant>

// One received thing: a saved file or an accepted text message.
struct HistoryEntry
{
    QString kind;          // "file", "image", "video" or "text"
    QString fileName;      // name of the saved file; empty for text
    QString path;          // absolute, '/' separators; empty for text
    QString text;          // text messages only (at most MaxTextLength characters)
    QString preview;       // text messages: first line, shortened (for the list)
    qint64 size;           // bytes (UTF-8 length of the whole message for text)
    QDateTime time;        // when it was received (local time)
    QString senderAlias;   // the device that sent it
};

// History of received files and messages, newest first, like LocalSend's
// receive history. Filled by Receiver, persisted as JSON in its own file
// (history.json in the app's data folder, not in QSettings, so the settings
// file isn't rewritten with every received file). See docs/qml-api.md.
class HistoryModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        KindRole = Qt::UserRole + 1,
        FileNameRole,
        PathRole,
        TextRole,
        SizeRole,
        SizeTextRole,
        TimeRole,
        TimeTextRole,
        SenderAliasRole,
        PreviewRole
    };

    // Older entries are dropped (the official app keeps 200 too).
    static const int MaxEntries = 200;
    // Longer messages are kept shortened, so 200 of them stay small in memory.
    static const int MaxTextLength = 10000;
    // The list's one-line preview of a message.
    static const int MaxPreviewLength = 150;

    explicit HistoryModel(QObject *parent = 0);
    ~HistoryModel();

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;

    // A file saved at path ('/' separators).
    void addFile(const QString &path, qint64 size, const QString &senderAlias);
    // An accepted text message.
    void addText(const QString &text, const QString &senderAlias);

    // Opens the file of entry `row` in the system's app for it (the phone's
    // document handler). False if it is a text entry, the file is gone or
    // nothing could open it.
    Q_INVOKABLE bool openFile(int row);

    // Shows `folder` (native or '/' separators) in the phone's file manager
    // (Symbian: the File manager app; elsewhere: the desktop's file browser).
    // Creates the folder first if needed. False if nothing could show it.
    Q_INVOKABLE bool openFolder(const QString &folder);

    // Forgets every entry. The received files themselves stay on disk.
    Q_INVOKABLE void clear();

signals:
    void countChanged();

private slots:
    void save();

private:
    void prepend(const HistoryEntry &entry);
    void load();
    bool loadFromFile();
    void migrateFromSettings();
    void addLoaded(const QVariantMap &m);
    static QString filePath();
    static QString previewOf(const QString &text);

    QList<HistoryEntry> m_entries;
    QLocale m_locale;      // QLocale::system() once, not per data() call
    // A transfer of many files adds them one by one: write once it's quiet.
    QTimer m_saveTimer;
};

#endif // HISTORYMODEL_H
