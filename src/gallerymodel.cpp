// SPDX-FileCopyrightText: 2026 Wunder Wungiel
// SPDX-License-Identifier: GPL-3.0-or-later

#include "gallerymodel.h"

#include "gallerythumbnails.h"
#include "selectionmodel.h"
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QMap>
#include <QMetaObject>
#include <QTimer>
#include <QUrl>

#ifdef Q_OS_SYMBIAN
#include <mdesession.h>
#include <mdequery.h>
#include <mdeobjectquery.h>
#include <mdeobject.h>
#include <mdeobjectdef.h>
#include <mdenamespacedef.h>
#include <mdeproperty.h>
#include <mdepropertydef.h>
#include <mdeconstants.h>
#else
#include <QDesktopServices>
#include <QDirIterator>
#endif

static qint64 nowMs()
{
    return QDateTime::currentDateTime().toMSecsSinceEpoch();
}

#ifdef Q_OS_SYMBIAN
// The MdS side: a session (opened once, asynchronously) and one object
// query per media type. MdS calls back from the active scheduler on the UI
// thread; a query is not deleted inside its own callback, so each step goes
// through GalleryModel::mdsStep() (queued).
class GalleryMdsQuery : public MMdESessionObserver, public MMdEQueryObserver
{
public:
    explicit GalleryMdsQuery(GalleryModel *model)
        : m_model(model), m_session(0), m_query(0), m_opened(false), m_broken(false),
          m_stage(0), m_created(0), m_modified(0), m_stageStarted(0)
    {
    }
    ~GalleryMdsQuery()
    {
        delete m_query;
        delete m_session;
    }

    void start()
    {
        m_found.clear();
        m_stage = 0;
        if (m_broken) { // a failed session is replaced on the next listing
            delete m_query;
            m_query = 0;
            delete m_session;
            m_session = 0;
            m_opened = false;
            m_broken = false;
        }
        if (!m_session) {
            qDebug() << "Gallery: opening the metadata session";
            TRAPD(err, m_session = CMdESession::NewL(*this));
            if (err != KErrNone) {
                qWarning() << "Gallery: cannot open the metadata session, error" << err;
                m_session = 0;
                m_stage = 2;
                queueStep();
            }
            return; // continues in HandleSessionOpened()
        }
        if (m_opened)
            queueStep();
        // else: still opening, HandleSessionOpened() continues
    }

    // One step: the next query, or the end.
    void step()
    {
        delete m_query;
        m_query = 0;
        while (m_stage < 2) {
            TRAPD(err, startQueryL());
            if (err == KErrNone)
                return; // continues in HandleQueryCompleted()
            qWarning() << "Gallery: cannot query" << stageName() << "error" << err;
            delete m_query;
            m_query = 0;
            ++m_stage;
        }
        m_model->finishLoad(m_found, "MdS");
    }

    void HandleSessionOpened(CMdESession &, TInt aError)
    {
        if (aError != KErrNone) {
            qWarning() << "Gallery: metadata session failed to open, error" << aError
                       << (aError == KErrPermissionDenied ? "(missing capability)" : "");
            m_broken = true;
            m_stage = 2;
        } else {
            qDebug() << "Gallery: metadata session open";
            m_opened = true;
        }
        queueStep();
    }

    void HandleSessionError(CMdESession &, TInt aError)
    {
        qWarning() << "Gallery: metadata session error" << aError;
        m_broken = true;
        if (m_model->loading()) {
            m_stage = 2;
            queueStep();
        }
    }

    void HandleQueryNewResults(CMdEQuery &, TInt, TInt) {}

    void HandleQueryCompleted(CMdEQuery &aQuery, TInt aError)
    {
        const int before = m_found.size();
        if (aError != KErrNone)
            qWarning() << "Gallery:" << stageName() << "query failed, error" << aError;
        TRAPD(err, collectL(aQuery));
        if (err != KErrNone)
            qWarning() << "Gallery: reading the" << stageName() << "results failed, error" << err;
        qDebug() << "Gallery: MdS has" << (m_found.size() - before) << stageName()
                 << "in" << (nowMs() - m_stageStarted) << "ms";
        ++m_stage;
        queueStep();
    }

private:
    const char *stageName() const { return m_stage == 0 ? "images" : "videos"; }

    void queueStep()
    {
        QMetaObject::invokeMethod(m_model, "mdsStep", Qt::QueuedConnection);
    }

    void startQueryL()
    {
        m_stageStarted = nowMs();
        CMdENamespaceDef &ns = m_session->GetDefaultNamespaceDefL();
        CMdEObjectDef &def = ns.GetObjectDefL(m_stage == 0 ? MdeConstants::Image::KImageObject()
                                                           : MdeConstants::Video::KVideoObject());
        // Only the dates (for sorting) are fetched; the URI always comes along.
        m_created = 0;
        m_modified = 0;
        TRAP_IGNORE(m_created = &def.GetPropertyDefL(MdeConstants::Object::KCreationDateProperty));
        TRAP_IGNORE(m_modified = &def.GetPropertyDefL(MdeConstants::Object::KLastModifiedDateProperty));
        m_query = m_session->NewObjectQueryL(ns, def, this);
        m_query->SetResultMode(EQueryResultModeItem);
        if (m_created)
            m_query->AddPropertyFilterL(m_created);
        if (m_modified)
            m_query->AddPropertyFilterL(m_modified);
        m_query->FindL();
    }

    qint64 timeOf(const CMdEObject &o, CMdEPropertyDef *def)
    {
        CMdEProperty *p = 0;
        if (!def || o.Property(*def, p) < 0 || !p)
            return 0;
        qint64 t = 0;
        TRAP_IGNORE(t = p->TimeValueL().Int64());
        return t;
    }

    void collectL(CMdEQuery &aQuery)
    {
        CMdEObjectQuery &q = static_cast<CMdEObjectQuery &>(aQuery);
        const bool video = m_stage == 1;
        for (TInt i = 0; i < q.Count(); ++i) {
            const CMdEObject &o = q.Result(i);
            const TDesC &uri = o.Uri();
            GalleryModel::Item item;
            item.path = QDir::fromNativeSeparators(QString::fromUtf16(uri.Ptr(), uri.Length()));
            if (item.path.isEmpty())
                continue;
            item.name = item.path.section(QLatin1Char('/'), -1);
            item.video = video;
            item.time = timeOf(o, m_created);
            if (item.time == 0)
                item.time = timeOf(o, m_modified);
            m_found << item;
        }
    }

    GalleryModel *m_model;
    CMdESession *m_session;
    CMdEObjectQuery *m_query;
    bool m_opened;
    bool m_broken;
    int m_stage;                 // 0 images, 1 videos, 2 done
    CMdEPropertyDef *m_created;
    CMdEPropertyDef *m_modified;
    qint64 m_stageStarted;
    QList<GalleryModel::Item> m_found;
};
#endif

GalleryModel::GalleryModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_loading(false)
    , m_loadStarted(0)
    , m_mds(0)
    , m_prefetcher(new GalleryPrefetcher(this))
    , m_open(false)
{
    QHash<int, QByteArray> roles;
    roles[PathRole] = "path";
    roles[NameRole] = "name";
    roles[IsVideoRole] = "isVideo";
    roles[ThumbnailRole] = "thumbnail";
    roles[CheckedRole] = "checked";
    setRoleNames(roles);
}

GalleryModel::~GalleryModel()
{
    m_prefetcher->stop();
#ifdef Q_OS_SYMBIAN
    delete m_mds;
#endif
}

int GalleryModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

QVariant GalleryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_items.size())
        return QVariant();
    const Item &it = m_items.at(index.row());
    switch (role) {
    case PathRole:
        return it.path;
    case NameRole:
        return it.name;
    case IsVideoRole:
        return it.video;
    case ThumbnailRole:
        // Videos have no thumbnail (yet): the page shows an icon.
        if (it.video)
            return QString();
        return QString(QLatin1String("image://gallery/"))
               + QString::fromLatin1(QUrl::toPercentEncoding(it.path));
    case CheckedRole:
        return m_checkedKeys.contains(key(it.path));
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

bool GalleryModel::isJunk(const QString &path, QString *why)
{
    const QStringList parts = QDir::fromNativeSeparators(path).split(QLatin1Char('/'), QString::SkipEmptyParts);
    for (int i = 0; i < parts.size(); ++i) {
        const QString &c = parts.at(i);
        if (i == 0 && c.endsWith(QLatin1Char(':')))
            continue; // the drive
        const bool isFile = i == parts.size() - 1;
        bool junk = c.startsWith(QLatin1Char('.'));
        if (!junk && !isFile) {
            const QString l = c.toLower();
            junk = c.startsWith(QLatin1Char('_'))            // _PAlbTN (old Gallery thumbnails)
                   || l.contains(QLatin1String("cache"))
                   || l.contains(QLatin1String("thumb"))
                   || l == QLatin1String("temp") || l == QLatin1String("tmp")
                   || l == QLatin1String("private") || l == QLatin1String("system")
                   || l == QLatin1String("sys") || l == QLatin1String("resource");
        }
        if (junk) {
            if (why)
                *why = c;
            return true;
        }
    }
    return false;
}

void GalleryModel::load()
{
    m_open = true;
    if (m_loading)
        return;
    m_loading = true;
    m_loadStarted = nowMs();
    emit loadingChanged();
    qDebug() << "Gallery: listing photos and videos";
#ifdef Q_OS_SYMBIAN
    if (!m_mds)
        m_mds = new GalleryMdsQuery(this);
    m_mds->start();
#else
    QTimer::singleShot(0, this, SLOT(scanFolders()));
#endif
}

void GalleryModel::mdsStep()
{
#ifdef Q_OS_SYMBIAN
    if (m_mds)
        m_mds->step();
#endif
}

void GalleryModel::scanFolders()
{
#ifndef Q_OS_SYMBIAN
    QStringList roots;
    roots << QDesktopServices::storageLocation(QDesktopServices::PicturesLocation)
          << QDesktopServices::storageLocation(QDesktopServices::MoviesLocation);
    roots.removeDuplicates();
    QStringList images, videos;
    images << "*.jpg" << "*.jpeg" << "*.png" << "*.gif" << "*.bmp";
    videos << "*.mp4" << "*.3gp" << "*.mkv" << "*.avi" << "*.mov";
    const int kMaxFiles = 20000;
    QList<Item> found;
    foreach (const QString &root, roots) {
        if (root.isEmpty() || !QFileInfo(root).isDir())
            continue;
        QDirIterator it(root, images + videos, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext() && found.size() < kMaxFiles) {
            it.next();
            const QFileInfo fi = it.fileInfo();
            Item item;
            item.path = fi.absoluteFilePath();
            item.name = fi.fileName();
            item.video = QDir::match(videos, item.name);
            item.time = fi.lastModified().toMSecsSinceEpoch();
            found << item;
        }
    }
    finishLoad(found, "folder scan");
#endif
}

static bool newerFirst(const GalleryModel::Item &a, const GalleryModel::Item &b);

void GalleryModel::finishLoad(const QList<Item> &found, const char *source)
{
    // What is left out, by the junk folder (path up to it), and what is
    // listed, by folder: both logged, to spot what the filter misses or hits.
    QMap<QString, int> junk, listed;
    QList<Item> items;
    foreach (const Item &it, found) {
        QString why;
        if (isJunk(it.path, &why)) {
            const int at = it.path.indexOf(why);
            junk[at >= 0 ? it.path.left(at + why.length()) : why] += 1;
            continue;
        }
        listed[it.path.section(QLatin1Char('/'), 0, -2)] += 1;
        items << it;
    }
    qStableSort(items.begin(), items.end(), newerFirst);

    qDebug() << "Gallery:" << source << "found" << found.size() << "items in"
             << (nowMs() - m_loadStarted) << "ms; listing" << items.size()
             << "in" << listed.size() << "folders, left out" << (found.size() - items.size());
    for (QMap<QString, int>::const_iterator i = listed.constBegin(); i != listed.constEnd(); ++i)
        qDebug() << "Gallery:   listed" << i.value() << "in" << i.key();
    for (QMap<QString, int>::const_iterator i = junk.constBegin(); i != junk.constEnd(); ++i)
        qDebug() << "Gallery:   left out" << i.value() << "in" << i.key();

    const int oldCount = m_items.size();
    beginResetModel();
    m_items = items;
    endResetModel();
    if (oldCount != m_items.size())
        emit countChanged();
    m_loading = false;
    emit loadingChanged();

    // Thumbnails for everything, newest first, while the page is open.
    if (m_open) {
        QStringList photos;
        foreach (const Item &it, m_items) {
            if (!it.video)
                photos << it.path;
        }
        m_prefetcher->start(photos);
    }
}

void GalleryModel::close()
{
    m_open = false;
    m_prefetcher->requestStop(); // without waiting: the UI would freeze meanwhile
}

static bool newerFirst(const GalleryModel::Item &a, const GalleryModel::Item &b)
{
    return a.time > b.time;
}

void GalleryModel::toggle(int index)
{
    if (index < 0 || index >= m_items.size())
        return;
    const Item &it = m_items.at(index);
    const QString k = key(it.path);
    if (m_checkedKeys.contains(k)) {
        m_checkedKeys.remove(k);
        for (int i = 0; i < m_checked.size(); ++i) {
            if (key(m_checked.at(i).path) == k) {
                m_checked.removeAt(i);
                break;
            }
        }
    } else {
        QFileInfo fi(it.path);
        if (!fi.isFile()) {
            qWarning() << "Gallery: no longer there:" << it.path;
            return;
        }
        Checked c;
        c.path = it.path;
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
    if (!m_items.isEmpty())
        emit dataChanged(createIndex(0, 0), createIndex(m_items.size() - 1, 0));
    emit checkedChanged();
}

QStringList GalleryModel::checkedFiles() const
{
    QStringList paths;
    foreach (const Checked &c, m_checked)
        paths << c.path;
    return paths;
}
