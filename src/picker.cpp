// SPDX-FileCopyrightText: 2026 Wunder Wungiel
// SPDX-License-Identifier: GPL-3.0-or-later

#include "picker.h"

#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QDir>
#include <QTimer>

#ifdef Q_OS_SYMBIAN
#include <AknCommonDialogsDynMem.h>
#include <aknnotewrappers.h>
#include <MGFetch.h>
#include <badesca.h>
#else
#include <QDesktopServices>
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

void Picker::pickMedia(const QString &kind)
{
    if (kind != QLatin1String("image") && kind != QLatin1String("video")) {
        qWarning() << "Picker: unknown media kind" << kind;
        return;
    }
    m_mediaKind = kind;
    QTimer::singleShot(0, this, SLOT(runMediaPicker()));
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
                kAllMedias, file, 0, toPtr(QLatin1String("Select file"))));
    if (err != KErrNone) {
        qWarning() << "Picker: file picker failed, error" << err;
        return;
    }
    if (ok)
        paths << fromDesC(file);
#else
    paths = QFileDialog::getOpenFileNames(0, QLatin1String("Select files"), QDir::homePath());
#endif

    reportFiles("file", paths);
}

void Picker::runMediaPicker()
{
    bool video = m_mediaKind == QLatin1String("video");
    qDebug() << "Picker: opening gallery picker for" << (video ? "videos" : "images");
    QStringList paths;

#ifdef Q_OS_SYMBIAN
    // The Photos fetcher; EAnyMediaFile is not supported since S60 5.0, so
    // images and videos are separate.
    CDesCArrayFlat *files = new CDesCArrayFlat(8);
    TBool ok = EFalse;
    TRAPD(err, ok = MGFetch::RunL(*files, video ? EVideoFile : EImageFile, ETrue,
                                  toPtr(QLatin1String("Select")),
                                  toPtr(QLatin1String(video ? "Select videos" : "Select images"))));
    if (err != KErrNone) {
        qWarning() << "Picker: gallery picker failed, error" << err
                   << (err == KErrPermissionDenied ? "(missing capability)" : "");
    } else if (ok) {
        for (TInt i = 0; i < files->MdcaCount(); ++i)
            paths << fromDesC(files->MdcaPoint(i));
    }
    delete files;
    if (err != KErrNone)
        return;
#else
    QString filter = video ? QLatin1String("Videos (*.mp4 *.3gp *.mkv *.avi *.mov)")
                           : QLatin1String("Images (*.jpg *.jpeg *.png *.gif *.bmp)");
    QString start = QDesktopServices::storageLocation(video ? QDesktopServices::MoviesLocation
                                                            : QDesktopServices::PicturesLocation);
    paths = QFileDialog::getOpenFileNames(0, QLatin1String(video ? "Select videos" : "Select images"),
                                          start, filter);
#endif

    reportFiles(video ? "video" : "image", paths);
}

void Picker::runFolderPicker()
{
    qDebug() << "Picker: opening folder picker";
    QString picked;

#ifdef Q_OS_SYMBIAN
    TFileName folder;
    TBool ok = EFalse;
    TRAPD(err, ok = AknCommonDialogsDynMem::RunFolderSelectDlgLD(
                kAllMedias, folder, KNullDesC, 0, 0, toPtr(QLatin1String("Select folder"))));
    if (err != KErrNone) {
        qWarning() << "Picker: folder picker failed, error" << err;
        return;
    }
    if (ok)
        picked = fromDesC(folder);
#else
    picked = QFileDialog::getExistingDirectory(0, QLatin1String("Select folder"), QDir::homePath());
#endif

    if (picked.isEmpty()) {
        qDebug() << "Picker: folder picker cancelled";
        return;
    }
    qDebug() << "Picker: picked folder" << picked;
    emit folderPicked(picked);
}
