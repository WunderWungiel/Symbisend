// SPDX-FileCopyrightText: 2026 Wunder Wungiel
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef RECEIVEFILESMODEL_H
#define RECEIVEFILESMODEL_H

#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QStringList>

// The files of the current offer, for the Options page: which ones to take
// and under what name. Like LocalSend's selectedReceivingFilesProvider, every
// file starts selected under its own name. Row numbers match the order
// Receiver gave them to setFiles(). See docs/qml-api.md.
class ReceiveFilesModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)

public:
    enum Roles {
        OriginalNameRole = Qt::UserRole + 1,
        NameRole,
        EditNameRole,
        KindRole,
        SizeRole,
        SizeTextRole,
        SelectedRole,
        RenamedRole
    };

    explicit ReceiveFilesModel(QObject *parent = 0);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;

    int selectedCount() const;

    // Starts over with a new offer; both lists have one entry per file.
    void setFiles(const QStringList &names, const QList<qint64> &sizes);
    bool isSelected(int row) const;
    QString name(int row) const;

    // Selecting a file again gives it its own name back, as in the official app.
    Q_INVOKABLE void setSelected(int row, bool selected);
    // Why this input can't be a file name, or "" if it can.
    Q_INVOKABLE QString nameError(const QString &input) const;
    // Applies the input to the last part of the name, so a file in a sent
    // folder stays in that folder. An input without an extension gets the
    // original one. False, and nothing changes, if nameError() has an objection.
    Q_INVOKABLE bool rename(int row, const QString &input);
    // Everything selected under its own name again.
    Q_INVOKABLE void reset();

signals:
    void countChanged();
    void selectionChanged();

private:
    struct File {
        QString originalName;   // may contain folders ("Trip/a.jpg")
        QString name;           // the same, with the user's last part
        qint64 size;
        bool selected;
    };

    static int leafStart(const QString &path);

    QList<File> m_files;
};

#endif // RECEIVEFILESMODEL_H
