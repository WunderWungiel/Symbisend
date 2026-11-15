// SPDX-FileCopyrightText: 2026 The Symbigram authors (https://github.com/Symnok/Symbigram)
// SPDX-FileCopyrightText: 2026 Wunder Wungiel
// SPDX-License-Identifier: GPL-3.0-or-later

#include "gallerymodel.h"

#include "selectionmodel.h"
#include "thumbnailer.h"
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTimer>
#include <QUrl>
#include <QtAlgorithms>

#ifndef Q_OS_SYMBIAN
#include <QDesktopServices>
#endif

namespace
{
    const char *const KImagePatterns[] = { "*.jpg", "*.jpeg", "*.png", "*.bmp", "*.gif" };
    const char *const KVideoPatterns[] = {
        "*.mp4", "*.m4v", "*.3gp", "*.3g2", "*.mov", "*.avi", "*.mkv", "*.wmv"
    };

    // Where a Symbian phone keeps photos and videos, per drive (C:/Data/Images,
    // E:/Images, E:/Videos, ...). Received media go to Images/Symbisend and
    // Videos/Symbisend, inside these. A missing folder costs nothing.
    const char *const KFolders[] = {
        "Data/Images", "Data/Videos", "Images", "Videos", "DCIM", "Data/Pictures", "Pictures"
    };

    const int KMaxDepth = 4;     // Images/201211/... is shallow; this is plenty
    const int KMaxDirs = 400;    // an enormous library cannot make us walk forever
    const int KMaxFiles = 4000;  // beyond this the grid is unusable anyway

    qint64 nowMs()
    {
        return QDateTime::currentDateTime().toMSecsSinceEpoch();
    }

    // Hidden folders and the old Gallery's thumbnails (_PAlbTN).
    bool skippedFolder(const QString &name)
    {
        return name.startsWith(QLatin1Char('.')) || name.startsWith(QLatin1Char('_'));
    }

    bool isVideoName(const QString &name)
    {
        const QString l = name.toLower();
        for (int i = 0; i < int(sizeof(KVideoPatterns) / sizeof(KVideoPatterns[0])); ++i) {
            if (l.endsWith(QLatin1String(KVideoPatterns[i] + 1))) // "*.mp4" -> ".mp4"
                return true;
        }
        return false;
    }
}

GalleryModel::GalleryModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_timer(new QTimer(this))
    , m_thumbs(new Thumbnailer(this))
    , m_thumbsOk(false)
    , m_scanning(false)
    , m_dirsVisited(0)
    , m_scanStarted(0)
{
    QHash<int, QByteArray> roles;
    roles[PathRole] = "path";
    roles[NameRole] = "name";
    roles[FolderRole] = "folder";
    roles[IsVideoRole] = "isVideo";
    roles[ThumbRole] = "thumb";
    roles[ThumbFailedRole] = "thumbFailed";
    roles[CheckedRole] = "checked";
    setRoleNames(roles);

    Thumbnailer::discardLegacyCache();

    // Interval 0: one directory per turn of the event loop, so touch and
    // paint still get served.
    m_timer->setInterval(0);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(scanSlice()));

    // Same thread: a direct call, but never from inside requestThumb().
    connect(m_thumbs, SIGNAL(ready(QString, QString)),
            this, SLOT(thumbReady(QString, QString)));
}

GalleryModel::~GalleryModel()
{
}

int GalleryModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant GalleryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return QVariant();
    const Entry &e = m_rows.at(index.row());
    switch (role) {
    case PathRole: return e.path;
    case NameRole: return e.name;
    case FolderRole: return e.folder;
    case IsVideoRole: return e.video;
    case ThumbRole: return e.thumb.isEmpty() ? QString() : QUrl::fromLocalFile(e.thumb).toString();
    case ThumbFailedRole: return e.failed;
    case CheckedRole: return m_checkedKeys.contains(key(e.path));
    }
    return QVariant();
}

QString GalleryModel::checkedSizeText() const
{
    qint64 total = 0;
    foreach (const Checked &c, m_checked)
        total += c.size;
    return SelectionModel::formatBytes(qreal(total));
}

void GalleryModel::load()
{
    m_timer->stop();
    m_thumbs->clearQueue();
    m_thumbsOk = m_thumbs->available(); // loads the service once
    m_found.clear();
    m_todo.clear();
    m_dirsVisited = 0;
    m_scanStarted = nowMs();

    QStringList roots;
#ifdef Q_OS_SYMBIAN
    // Every drive the phone has now; a removed card simply isn't listed.
    const QFileInfoList drives = QDir::drives();
    for (int d = 0; d < drives.size(); ++d) {
        const QString root = drives.at(d).absolutePath(); // "C:/", "E:/", ...
        if (root.startsWith(QLatin1String("Z"), Qt::CaseInsensitive))
            continue; // ROM
        for (int f = 0; f < int(sizeof(KFolders) / sizeof(KFolders[0])); ++f)
            roots << root + QLatin1String(KFolders[f]);
    }
#else
    Q_UNUSED(KFolders);
    roots << QDesktopServices::storageLocation(QDesktopServices::PicturesLocation)
          << QDesktopServices::storageLocation(QDesktopServices::MoviesLocation);
    roots.removeDuplicates();
#endif
    QStringList existing;
    foreach (const QString &r, roots) {
        if (r.isEmpty() || !QFileInfo(r).isDir())
            continue;
        Dir dir;
        dir.path = QDir::fromNativeSeparators(r);
        dir.depth = 0;
        m_todo << dir;
        existing << dir.path;
    }
    qDebug() << "Gallery: scanning" << existing;

    m_scanning = true;
    emit loadingChanged();
    emit foundChanged();
    if (m_todo.isEmpty())
        finish();
    else
        m_timer->start();
}

void GalleryModel::close()
{
    m_thumbs->clearQueue();
    if (!m_scanning)
        return;
    m_timer->stop();
    m_todo.clear();
    m_found.clear();
    m_scanning = false;
    qDebug() << "Gallery: scan stopped (page closed)";
    emit loadingChanged();
    emit foundChanged();
}

void GalleryModel::scanSlice()
{
    if (m_todo.isEmpty() || m_dirsVisited >= KMaxDirs || m_found.size() >= KMaxFiles) {
        finish();
        return;
    }

    const Dir dir = m_todo.takeFirst();
    ++m_dirsVisited;
    QDir d(dir.path);

    QStringList patterns;
    for (int i = 0; i < int(sizeof(KImagePatterns) / sizeof(KImagePatterns[0])); ++i)
        patterns << QLatin1String(KImagePatterns[i]);
    for (int i = 0; i < int(sizeof(KVideoPatterns) / sizeof(KVideoPatterns[0])); ++i)
        patterns << QLatin1String(KVideoPatterns[i]);

    const QFileInfoList files = d.entryInfoList(patterns, QDir::Files | QDir::Readable);
    for (int i = 0; i < files.size() && m_found.size() < KMaxFiles; ++i) {
        const QFileInfo &fi = files.at(i);
        if (fi.size() <= 0)
            continue;
        Entry e;
        e.path = fi.absoluteFilePath();
        e.name = fi.fileName();
        e.folder = fi.absoluteDir().dirName();
        e.video = isVideoName(e.name);
        // Without the system service there are no thumbnails: the grid
        // shows placeholders at once instead of waiting.
        e.failed = !m_thumbsOk;
        e.modified = fi.lastModified().toTime_t();
        m_found << e;
    }
    emit foundChanged();

    if (dir.depth < KMaxDepth) {
        const QFileInfoList subs = d.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable);
        for (int i = 0; i < subs.size(); ++i) {
            if (skippedFolder(subs.at(i).fileName())) {
                qDebug() << "Gallery: skipping" << subs.at(i).absoluteFilePath();
                continue;
            }
            Dir sub;
            sub.path = subs.at(i).absoluteFilePath();
            sub.depth = dir.depth + 1;
            m_todo << sub;
        }
    }
}

void GalleryModel::finish()
{
    m_timer->stop();
    const bool cut = !m_todo.isEmpty();
    m_todo.clear();

    // Published in one go, newest first: rows appearing and then reordering
    // under the user's finger is worse than a brief busy indicator.
    qStableSort(m_found.begin(), m_found.end(), newerFirst);
    int videos = 0;
    foreach (const Entry &e, m_found) {
        if (e.video)
            ++videos;
    }
    qDebug() << "Gallery: listed" << (m_found.size() - videos) << "photos and" << videos
             << "videos from" << m_dirsVisited << "folders in" << (nowMs() - m_scanStarted) << "ms"
             << (cut ? "(stopped at the limit)" : "");

    const int oldCount = m_rows.size();
    beginResetModel();
    m_rows = m_found;
    m_rowOf.clear();
    for (int i = 0; i < m_rows.size(); ++i)
        m_rowOf.insert(key(m_rows.at(i).path), i);
    endResetModel();
    m_found.clear();

    m_scanning = false;
    emit loadingChanged();
    emit foundChanged();
    if (oldCount != m_rows.size())
        emit countChanged();
}

void GalleryModel::requestThumb(int index, int edge)
{
    if (index < 0 || index >= m_rows.size())
        return;
    const Entry &e = m_rows.at(index);
    if (e.failed || !e.thumb.isEmpty())
        return;

    // Cached from an earlier visit: take it now and skip the service.
    const QString cached = Thumbnailer::thumbPathFor(e.path);
    if (QFile::exists(cached)) {
        m_rows[index].thumb = cached;
        const QModelIndex mi = createIndex(index, 0);
        emit dataChanged(mi, mi);
        return;
    }
    m_thumbs->request(e.path, edge);
}

void GalleryModel::thumbReady(const QString &imagePath, const QString &thumbPath)
{
    const QHash<QString, int>::const_iterator it = m_rowOf.constFind(key(imagePath));
    if (it == m_rowOf.constEnd())
        return; // listed again meanwhile
    const int row = it.value();
    // Empty: unreadable, or the cache could not be written. Recorded, so the
    // cell shows a placeholder instead of waiting for ever.
    if (thumbPath.isEmpty())
        m_rows[row].failed = true;
    else
        m_rows[row].thumb = thumbPath;
    const QModelIndex mi = createIndex(row, 0);
    emit dataChanged(mi, mi);
}

void GalleryModel::toggle(int index)
{
    if (index < 0 || index >= m_rows.size())
        return;
    const Entry &e = m_rows.at(index);
    const QString k = key(e.path);
    if (m_checkedKeys.contains(k)) {
        m_checkedKeys.remove(k);
        for (int i = 0; i < m_checked.size(); ++i) {
            if (key(m_checked.at(i).path) == k) {
                m_checked.removeAt(i);
                break;
            }
        }
    } else {
        QFileInfo fi(e.path);
        if (!fi.isFile()) {
            qWarning() << "Gallery: no longer there:" << e.path;
            return;
        }
        Checked c;
        c.path = e.path;
        c.size = fi.size();
        m_checked << c;
        m_checkedKeys.insert(k);
    }
    const QModelIndex mi = createIndex(index, 0);
    emit dataChanged(mi, mi);
    emit checkedChanged();
}

void GalleryModel::clearChecked()
{
    if (m_checked.isEmpty())
        return;
    m_checked.clear();
    m_checkedKeys.clear();
    if (!m_rows.isEmpty())
        emit dataChanged(createIndex(0, 0), createIndex(m_rows.size() - 1, 0));
    emit checkedChanged();
}

QStringList GalleryModel::checkedFiles() const
{
    QStringList paths;
    foreach (const Checked &c, m_checked)
        paths << c.path;
    return paths;
}
