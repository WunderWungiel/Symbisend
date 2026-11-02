#ifndef SELECTIONMODEL_H
#define SELECTIONMODEL_H

#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QStringList>

// One thing to send: a file (from the file/gallery/folder pickers) or a text
// message (typed or pasted).
struct SelectionItem
{
    QString kind;          // "file", "image", "video" or "text"
    QString path;          // absolute, '/' separators; empty for text
    QString name;          // file name; for text, the text itself
    QString relativePath;  // "Folder/sub/a.jpg" for folder contents, else = name
    QString text;          // text messages only
    qint64 size;           // bytes (UTF-8 length for text)
};

// The Send page's selection: everything that will be sent to the next device
// the user taps, like LocalSend's selected files. See docs/qml-api.md.
class SelectionModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    // Bytes; qreal so that sizes over 2 GB survive the trip to QML.
    Q_PROPERTY(qreal totalSize READ totalSize NOTIFY countChanged)
    Q_PROPERTY(QString totalSizeText READ totalSizeText NOTIFY countChanged)

public:
    enum Roles {
        KindRole = Qt::UserRole + 1,
        NameRole,
        PathRole,
        RelativePathRole,
        TextRole,
        SizeRole,
        SizeTextRole
    };

    explicit SelectionModel(QObject *parent = 0);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;

    qreal totalSize() const;
    QString totalSizeText() const;

    const QList<SelectionItem> &items() const { return m_items; }

    // "1.2 MB", like LocalSend.
    Q_INVOKABLE QString formatSize(qreal bytes) const;

public slots:
    // Files picked with the file or gallery picker. Already selected paths and
    // missing files are skipped.
    void addFiles(const QStringList &paths);
    // Every file inside the folder, recursively, with relative paths that start
    // with the folder's name.
    void addFolder(const QString &path);
    // A text message. Empty text is ignored.
    void addText(const QString &text);
    // Replace the text of a text item.
    void setText(int index, const QString &text);
    void remove(int index);
    void clear();

signals:
    // Also emitted when setText() changes the total size.
    void countChanged();

private:
    void append(const QList<SelectionItem> &items);
    bool containsPath(const QString &path) const;
    static QString kindForFile(const QString &fileName);

    QList<SelectionItem> m_items;
    qint64 m_totalSize;
};

#endif // SELECTIONMODEL_H
