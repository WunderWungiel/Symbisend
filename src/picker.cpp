// SPDX-FileCopyrightText: 2026 Wunder Wungiel
// SPDX-License-Identifier: GPL-3.0-or-later

#include "picker.h"

#include "i18n.h"
#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QDir>
#include <QTimer>

#ifdef Q_OS_SYMBIAN
#include <AknCommonDialogsDynMem.h>
#include <aknnotewrappers.h>
#else
#include <QFileDialog>
#include <QMessageBox>
#endif

#ifdef Q_OS_SYMBIAN
static TPtrC toPtr(const QString &s)
{
    return TPtrC(reinterpret_cast<const TUint16 *>(s.utf16()), s.length());
}

static QString fromDesC(const TDesC &d)
{
    return QString::fromUtf16(d.Ptr(), d.Length());
}

static const TInt kAllMedias = AknCommonDialogsDynMem::EMemoryTypePhone
        | AknCommonDialogsDynMem::EMemoryTypeInternalMassStorage
        | AknCommonDialogsDynMem::EMemoryTypeMMCExternal;
#endif

Picker::Picker(QObject *parent)
    : QObject(parent)
    , m_clipboardHasText(false)
{
    connect(QApplication::clipboard(), SIGNAL(dataChanged()), this, SLOT(checkClipboard()));
    checkClipboard();
}

void Picker::checkClipboard()
{
    bool has = !QApplication::clipboard()->text().isEmpty();
    if (has == m_clipboardHasText)
        return;
    m_clipboardHasText = has;
    qDebug() << "Picker: clipboard" << (has ? "has text" : "is empty");
    emit clipboardHasTextChanged();
}

void Picker::pickFile()
{
    QTimer::singleShot(0, this, SLOT(runFilePicker()));
}

void Picker::pickFolder()
{
    QTimer::singleShot(0, this, SLOT(runFolderPicker()));
}

QString Picker::clipboardText() const
{
    QString text = QApplication::clipboard()->text();
    if (text.isEmpty()) {
        qDebug() << "Picker: clipboard is empty";
    } else {
        const int kMaxLogged = 1000;
        qDebug() << "Picker: clipboard text," << text.length() << "chars:"
                 << (text.length() > kMaxLogged ? text.left(kMaxLogged) + QLatin1String("...") : text);
    }
    return text;
}

void Picker::copyText(const QString &text)
{
    qDebug() << "Picker: copying" << text.length() << "chars to the clipboard";
    QApplication::clipboard()->setText(text);
}

void Picker::showInfo(const QString &text)
{
    qDebug() << "Picker: info note:" << text;
#ifdef Q_OS_SYMBIAN
    // Non-waiting note: ExecuteLD() returns at once and the note deletes
    // itself when it times out.
    TRAPD(err, {
        CAknInformationNote *note = new (ELeave) CAknInformationNote(EFalse);
        note->ExecuteLD(toPtr(text));
    });
    if (err != KErrNone)
        qWarning() << "Picker: info note failed, error" << err;
#else
    QMessageBox::information(0, QLatin1String("Symbisend"), text);
#endif
}

void Picker::reportFiles(const char *what, const QStringList &paths)
{
    if (paths.isEmpty()) {
        qDebug() << "Picker:" << what << "picker cancelled";
        return;
    }
    qDebug() << "Picker: picked" << paths.count() << what << (paths.count() == 1 ? "item" : "items");
    foreach (const QString &p, paths)
        qDebug() << "Picker:   " << p;
    emit filesPicked(paths);
}

void Picker::runFilePicker()
{
    qDebug() << "Picker: opening file picker";
    QStringList paths;

#ifdef Q_OS_SYMBIAN
    TFileName file;
    TBool ok = EFalse;
    // Memory selection first, then browsing; resource id 0 = default resources.
    TRAPD(err, ok = AknCommonDialogsDynMem::RunSelectDlgLD(
                kAllMedias, file, 0, toPtr(I18n::text("symbisend.picker.selectFile"))));
    if (err != KErrNone) {
        qWarning() << "Picker: file picker failed, error" << err;
        return;
    }
    if (ok)
        paths << fromDesC(file);
#else
    paths = QFileDialog::getOpenFileNames(0, I18n::text("symbisend.picker.selectFiles"), QDir::homePath());
#endif

    reportFiles("file", paths);
}

void Picker::runFolderPicker()
{
    qDebug() << "Picker: opening folder picker";
    QString picked;

#ifdef Q_OS_SYMBIAN
    TFileName folder;
    TBool ok = EFalse;
    TRAPD(err, ok = AknCommonDialogsDynMem::RunFolderSelectDlgLD(
                kAllMedias, folder, KNullDesC, 0, 0, toPtr(I18n::text("symbisend.picker.selectFolder"))));
    if (err != KErrNone) {
        qWarning() << "Picker: folder picker failed, error" << err;
        return;
    }
    if (ok)
        picked = fromDesC(folder);
#else
    picked = QFileDialog::getExistingDirectory(0, I18n::text("symbisend.picker.selectFolder"), QDir::homePath());
#endif

    if (picked.isEmpty()) {
        qDebug() << "Picker: folder picker cancelled";
        return;
    }
    qDebug() << "Picker: picked folder" << picked;
    emit folderPicked(picked);
}
