#include "settings.h"

#include <QDebug>
#include <QDir>
#include <QSettings>
#include <QTimer>

#ifdef Q_OS_SYMBIAN
#include <AknCommonDialogsDynMem.h>
#else
#include <QFileDialog>
#endif

static const char *kDownloadFolderKey = "transfer/downloadFolder";

Settings::Settings(QObject *parent)
    : QObject(parent)
{
    m_custom = QSettings().value(QLatin1String(kDownloadFolderKey)).toString();
    updateDownloadFolder();
}

QString Settings::downloadFolderNative() const
{
    return QDir::toNativeSeparators(m_downloadFolder);
}

QString Settings::defaultDownloadFolder()
{
#ifdef Q_OS_SYMBIAN
    // Prefer the memory card (F:), then mass memory (E:), then phone memory.
    const char *candidates[] = { "F:/Symbisend", "E:/Symbisend", "C:/Data/Symbisend" };
    for (unsigned i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        QString path = QString::fromLatin1(candidates[i]);
        if (QDir(path.left(3)).exists() && QDir().mkpath(path))
            return path;
        qDebug() << "Settings: default download folder" << path << "not usable";
    }
    return QLatin1String("C:/Data/Symbisend");
#else
    return QDir::homePath() + QLatin1String("/Downloads/Symbisend");
#endif
}

void Settings::updateDownloadFolder()
{
    QString folder;
    if (!m_custom.isEmpty()) {
        if (QDir(m_custom).exists() || QDir().mkpath(m_custom))
            folder = m_custom;
        else
            // E.g. the memory card was removed. Keep the choice for next time.
            qWarning() << "Settings: chosen download folder" << m_custom
                       << "is not available, using the default";
    }
    if (folder.isEmpty())
        folder = defaultDownloadFolder();

    if (folder != m_downloadFolder) {
        m_downloadFolder = folder;
        qDebug() << "Settings: download folder" << m_downloadFolder
                 << (m_custom.isEmpty() ? "(default)" : "(chosen)");
        emit downloadFolderChanged();
    }
}

void Settings::setCustomDownloadFolder(const QString &path)
{
    // Store as a clean Qt path without trailing slash ("F:/Music"), but keep
    // a bare drive root as "F:/".
    QString p = QDir::cleanPath(QDir::fromNativeSeparators(path));
    if (p.length() == 2 && p.at(1) == QLatin1Char(':'))
        p += QLatin1Char('/');

    bool wasDefault = m_custom.isEmpty();
    m_custom = p;
    QSettings().setValue(QLatin1String(kDownloadFolderKey), m_custom);
    updateDownloadFolder();
    if (wasDefault)
        emit downloadFolderChanged(); // downloadFolderIsDefault changed
}

void Settings::resetDownloadFolder()
{
    if (m_custom.isEmpty())
        return;
    qDebug() << "Settings: download folder reset to default";
    m_custom.clear();
    QSettings().remove(QLatin1String(kDownloadFolderKey));
    updateDownloadFolder();
    emit downloadFolderChanged(); // downloadFolderIsDefault changed
}

void Settings::pickDownloadFolder()
{
    // The native dialog runs a nested event loop; don't start it from inside
    // the QML click handler.
    QTimer::singleShot(0, this, SLOT(runFolderPicker()));
}

void Settings::runFolderPicker()
{
    qDebug() << "Settings: opening folder picker at" << m_downloadFolder;
    QString picked;

#ifdef Q_OS_SYMBIAN
    QString start = QDir::toNativeSeparators(m_downloadFolder);
    if (!start.endsWith(QLatin1Char('\\')))
        start += QLatin1Char('\\');
    TPtrC startPtr(reinterpret_cast<const TUint16 *>(start.utf16()), start.length());

    QString title = QLatin1String("Download folder");
    TPtrC titlePtr(reinterpret_cast<const TUint16 *>(title.utf16()), title.length());

    TInt medias = AknCommonDialogsDynMem::EMemoryTypePhone
            | AknCommonDialogsDynMem::EMemoryTypeInternalMassStorage
            | AknCommonDialogsDynMem::EMemoryTypeMMCExternal;

    TFileName folder;
    TBool ok = EFalse;
    // Resource ids 0 = the dialogs' default resources.
    TRAPD(err, ok = AknCommonDialogsDynMem::RunFolderSelectDlgLD(
                medias, folder, startPtr, 0, 0, titlePtr));
    if (err != KErrNone) {
        qWarning() << "Settings: folder picker failed, error" << err;
        return;
    }
    if (ok)
        picked = QString::fromUtf16(folder.Ptr(), folder.Length());
#else
    picked = QFileDialog::getExistingDirectory(0, QLatin1String("Download folder"),
                                               m_downloadFolder);
#endif

    if (picked.isEmpty()) {
        qDebug() << "Settings: folder picker cancelled";
        return;
    }
    qDebug() << "Settings: picked folder" << picked;
    setCustomDownloadFolder(picked);
}
