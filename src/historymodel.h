#ifndef HISTORYMODEL_H
#define HISTORYMODEL_H

#include <QAbstractListModel>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QTimer>

// One received thing: a saved file or an accepted text message.
struct HistoryEntry
{
    QString kind;          // "file", "image", "video" or "text"
    QString fileName;      // name of the saved file; empty for text
    QString path;          // absolute, '/' separators; empty for text
    QString text;          // text messages only
    qint64 size;           // bytes (UTF-8 length for text)
    QDateTime time;        // when it was received (local time)
    QString senderAlias;   // the device that sent it
};

// History of received files and messages, newest first, like LocalSend's
// receive history. Filled by Receiver, persisted in QSettings. See
// docs/qml-api.md.
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
        SenderAliasRole
    };

    // Older entries are dropped (the official app keeps 200 too).
    static const int MaxEntries = 200;

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

signals:
    void countChanged();

private slots:
    void save();

private:
    void prepend(const HistoryEntry &entry);
    void load();

    QList<HistoryEntry> m_entries;
    // A transfer of many files adds them one by one: write once it's quiet.
    QTimer m_saveTimer;
};

#endif // HISTORYMODEL_H
