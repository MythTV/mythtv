#include "imagemanager.h"

#include <QImageReader>
#include <QRunnable>
#include <utility>

#include "libmythbase/mthreadpool.h"
#include "libmythbase/mythdate.h"
#include "libmythui/mediamonitor.h"

#include "dbaccess.h"  // for FileAssociations

#define LOC QString("ImageManager: ")
#define DBLOC QString("ImageDb(%1): ").arg(m_table)

// Must be empty as it's prepended to path
static constexpr const char* STORAGE_GROUP_MOUNT { "" };

static constexpr const char* DB_TABLE { "gallery_files" };

// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define RESULT_ERR(ERR, MESG) \
{   LOG(VB_GENERAL, LOG_ERR, LOC + (MESG)); \
    return QStringList("ERROR") << (ERR); }

#define RESULT_OK(MESG) \
{   LOG(VB_FILE, LOG_DEBUG, LOC + (MESG)); \
    return QStringList("OK"); }
// NOLINTEND(cppcoreguidelines-macro-usage)

static constexpr const char* IMPORTDIR { "Import" };


//! A device containing images (ie. USB stick, CD, storage group etc)
class Device
{
public:
    Device(QString name, QString mount,
           MythMediaDevice *media = nullptr, QTemporaryDir *import = nullptr)
        : m_name(std::move(name)), m_mount(std::move(mount)),
          m_media(media), m_dir(import)
    {
        // Path relative to TEMP storage group
        m_thumbs = QString("%1/%2").arg(THUMBNAIL_SUBDIR, m_name);
    }


    //! Delete device, its thumbnails and any imported images
    ~Device()
    {
        Close();

        // Remove imported images
        delete m_dir;

        // Clean up non-SG thumbnails
        if (m_mount != STORAGE_GROUP_MOUNT)
            RemoveThumbs();
    }


    //! Releases device
    void Close(bool eject = false)
    {
        // Imports remain present; others do not
        m_present = isImport();

        // Release device
        if (m_media)
        {
            if (eject)
            {
                LOG(VB_GENERAL, LOG_INFO, LOC + QString("Ejecting '%1' at '%2'")
                    .arg(m_name, m_mount));
                MediaMonitor::GetMediaMonitor()->EjectMedia(m_media->getDevicePath());
            }
            else
            {
                LOG(VB_MEDIA, LOG_DEBUG, LOC + QString("Unlocked '%1'").arg(m_name));
            }

            MediaMonitor::GetMediaMonitor()->Unlock(m_media);
            m_media = nullptr;
        }
    }


    /*!
     \brief Clears all files and sub-dirs within a directory
     \param path Dir to clear
    */
    static void RemoveDirContents(const QString& path)
    {
        QDir(path).removeRecursively();
    }


    //! Delete thumbnails associated with device
    void RemoveThumbs(void) const
    {
        // Remove thumbnails
        QString dirFmt = QString("%1/") % TEMP_SUBDIR % "/%2";
        QString dir = dirFmt.arg(GetConfDir(), m_thumbs);
        LOG(VB_FILE, LOG_INFO, LOC + QString("Removing thumbnails in %1").arg(dir));
        RemoveDirContents(dir);
        QDir::root().rmpath(dir);
    }


    bool isImport() const                    { return m_dir; }
    bool isPresent() const                   { return m_present; }
    void setPresent(MythMediaDevice *media)  { m_present = true; m_media = media; }

    //! True when gallery UI is running & device is useable. Always true for imports
    bool             m_present { true };
    QString          m_name;   //!< Device model/volume/id
    QString          m_mount;  //!< Mountpoint
    QString          m_thumbs; //!< Dir sub-path of device thumbnails
    MythMediaDevice *m_media { nullptr }; //!< Set for MediaMonitor devices only
    QTemporaryDir   *m_dir   { nullptr }; //!< Dir path of images: import devices only
};


static Device kNullDevice = Device("Unknown Device", "<Invalid Path>");


DeviceManager::~DeviceManager()
{
    qDeleteAll(m_devices);
}


//! Get path at which the device is mounted
QString DeviceManager::DeviceMount(int devId) const
{
    return m_devices.value(devId, &kNullDevice)->m_mount;
}


//! Get model name of the device
QString DeviceManager::DeviceName(int devId) const
{
    return m_devices.value(devId, &kNullDevice)->m_name;
}


QString DeviceManager::ThumbDir(int fs) const
{
    return m_devices.value(fs, &kNullDevice)->m_thumbs;
}


/*!
 \brief Define a new device and assign it a unique id. If the device is already
 known, its existing id is returned.
 \param name  Device model/volume/id
 \param mount Device mountpoint
 \param media Set for MediaMonitor devices only
 \param dir   Dir path of images: import devices only
 \return int A unique id for this device.
*/
int DeviceManager::OpenDevice(const QString &name, const QString &mount,
                              MythMediaDevice *media, QTemporaryDir *dir)
{
    // Handle devices reappearing at same mountpoint.
    // If a USB is unplugged whilst in use (without unmounting) we get no event
    // but we do when it's re-inserted
    QString state("Known");
    int id = LocateMount(mount);

    if (id == DEVICE_INVALID)
    {
        state = "New";
        id = m_devices.isEmpty() ? 0 : m_devices.lastKey() + 1;
        m_devices.insert(id, new Device(name, mount, media, dir));
    }
    else
    {
        Device *dev = m_devices.value(id);
        if (dev)
            dev->setPresent(media);
    }

    LOG(VB_GENERAL, LOG_INFO, LOC +
        QString("%1 device %2 mounted at '%3' [Id %4]")
        .arg(state, name, mount).arg(id));

    return id;
}


/*!
 \brief Remove a device (or all devices)
 \param devId Id of device to remove
 \param action What action to take.
 \return bool True if device is recognized
*/
QStringList DeviceManager::CloseDevices(int devId, const QString &action)
{
    QStringList clear;

    if (action == "DEVICE CLOSE ALL")
    {
        // Close all devices but retain their thumbnails
        for (auto *dev : std::as_const(m_devices))
            if (dev)
                dev->Close();
    }
    else if (action == "DEVICE CLEAR ALL")
    {
        // Remove all thumbnails but retain devices
        for (const auto *dev : std::as_const(m_devices)) {
            if (dev)
            {
                clear << dev->m_mount;
                dev->RemoveThumbs();
            }
        }
    }
    else
    {
        // Remove single device & its thumbnails, optionally ejecting it
        Device *dev = m_devices.take(devId);
        if (dev)
        {
            if (action == "DEVICE EJECT")
                dev->Close(true);
            clear << dev->m_mount;
            delete dev;
        }
    }
    return clear;
}


/*!
 \brief Find the id of a device
 \param mount Device mountpoint
 \return int Id (positive) if found, 0 if unknown
*/
int DeviceManager::LocateMount(const QString &mount) const
{
    DeviceMap::const_iterator it = m_devices.constBegin();
    while (it != m_devices.constEnd())
    {
        if (it.value()->m_mount == mount)
            return it.key();
        ++it;
    }
    return DEVICE_INVALID;
}


//! Get all known devices
StringMap DeviceManager::GetDeviceDirs() const
{
    StringMap paths;
    for (auto it = m_devices.constKeyValueBegin();
         it != m_devices.constKeyValueEnd(); ++it)
    {
        if (it->second)
            paths.insert(it->first, it->second->m_mount);
    }
    return paths;
}


//! Get list of mountpoints for non-import devices
QList<int> DeviceManager::GetAbsentees()
{
    QList<int> absent;
    for (auto it = m_devices.constKeyValueBegin();
         it != m_devices.constKeyValueEnd(); it++)
    {
        Device *dev = it->second;
        if (dev && !dev->isPresent())
            absent << it->first;
    }
    return absent;
}


/*!
 \brief Constructor
*/
ImageAdapterBase::ImageAdapterBase() :
    m_imageFileExt(SupportedImages()),
    m_videoFileExt(SupportedVideos())
{
    // Generate glob list from supported extensions
    QStringList glob;
    QStringList allExt = m_imageFileExt + m_videoFileExt;
    for (const auto& ext : std::as_const(allExt))
        glob << "*." + ext;

    // Apply filters to only detect image files
    m_dirFilter.setNameFilters(glob);
    m_dirFilter.setFilter(QDir::AllDirs | QDir::Files | QDir::Readable |
                          QDir::NoDotAndDotDot | QDir::NoSymLinks);

    // Sync files before dirs to improve thumb generation response
    // Order by time (oldest first) - this determines the order thumbs appear
    m_dirFilter.setSorting(QDir::DirsLast | QDir::Time | QDir::Reversed);
}

/*!
   \brief Return recognised pictures
   \return List of file extensions
 */
QStringList ImageAdapterBase::SupportedImages()
{
    // Determine supported picture formats from Qt
    QStringList formats;
    QList<QByteArray> supported = QImageReader::supportedImageFormats();
    for (const auto& ext : std::as_const(supported))
        formats << QString(ext);
    return formats;
}


/*!
   \brief Return recognised video extensions
   \return List of file extensions
 */
QStringList ImageAdapterBase::SupportedVideos()
{
    // Determine supported video formats from MythVideo
    QStringList formats;
    const FileAssociations::association_list faList =
        FileAssociations::getFileAssociation().getList();
    for (const auto & fa : faList)
    {
        if (!fa.use_default && fa.playcommand == "Internal")
            formats << QString(fa.extension);
    }
    return formats;
}


/*!
 \brief Construct a local image from a file
 \param fi File
 \param parentId Id of the parent dir
 \param devId Id of device containing the file
 \param base Unused but required for adapter interface
 \return ImageItem An image object
*/
ImageItem *ImageAdapterLocal::CreateItem(const QFileInfo &fi, int parentId,
                                         int devId, const QString & /*base*/) const
{
    auto *im = new ImageItem();

    im->m_parentId  = parentId;
    im->m_device    = devId;
    im->m_filePath  = fi.absoluteFilePath();

    if (parentId == GALLERY_DB_ID)
    {
        // Import devices show time of import, other devices show 'last scan time'
        auto secs     = im->m_filePath.contains(IMPORTDIR)
                ? fi.lastModified().toSecsSinceEpoch()
                : QDateTime::currentSecsSinceEpoch();
        im->m_date    = std::chrono::seconds(secs);
        im->m_modTime = im->m_date;
        im->m_type    = kDevice;
        return im;
    }

    im->m_modTime = std::chrono::seconds(fi.lastModified().toSecsSinceEpoch());

    if (fi.isDir())
    {
        im->m_type = kDirectory;
        return im;
    }

    im->m_extension = fi.suffix().toLower();
    im->m_type      = GetImageType(im->m_extension);

    if (im->m_type == kUnknown)
    {
        delete im;
        return nullptr;
    }

    im->m_thumbPath = GetAbsThumbPath(ThumbDir(im->m_device), ThumbPath(*im));
    im->m_size      = fi.size();

    return im;
}


/*!
 * \brief Send local message to UI about local ids
 * \param mesg Message name
 * \param extra Message data
 */
void ImageAdapterLocal::Notify(const QString &mesg,
                               const QStringList &extra)
{
    QString host(gCoreContext->GetHostName());
    gCoreContext->SendEvent(MythEvent(QString("%1 %2").arg(mesg, host), extra));
}


/*!
 \brief Construct a remote image from a file
 \param fi File
 \param parentId Id of the parent dir
 \param devId Unused
 \param base SG dir path
 \return ImageItem An image object
*/
ImageItem *ImageAdapterSg::CreateItem(const QFileInfo &fi, int parentId,
                                      int /*devId*/, const QString &base) const
{
    auto *im = new ImageItem();

    im->m_device    = 0;
    im->m_parentId  = parentId;

    if (parentId == GALLERY_DB_ID)
    {
        // All SG dirs map to a single Db dir
        im->m_filePath = "";
        im->m_type     = kDevice;
        im->m_date     = std::chrono::seconds(QDateTime::currentSecsSinceEpoch());
        im->m_modTime  = im->m_date;
        return im;
    }

    // Strip SG path & leading / to leave a relative path
    im->m_filePath = fi.absoluteFilePath().mid(base.size() + 1);
    im->m_modTime  = std::chrono::seconds(fi.lastModified().toSecsSinceEpoch());

    if (fi.isDir())
    {
        im->m_type = kDirectory;
        return im;
    }

    im->m_extension = fi.suffix().toLower();
    im->m_type      = GetImageType(im->m_extension);

    if (im->m_type == kUnknown)
    {
        delete im;
        return nullptr;
    }

    im->m_thumbPath = GetAbsThumbPath(ThumbDir(im->m_device), ThumbPath(*im));
    im->m_size      = fi.size();

    return im;
}


/*!
 * \brief Send message to all clients about remote ids
 * \param mesg Message name
 * \param extra Message data
 */
void ImageAdapterSg::Notify(const QString &mesg,
                            const QStringList &extra)
{
    gCoreContext->SendEvent(MythEvent(mesg, extra));
}


/*!
 \brief Returns SG dirs
 \return StringMap Map <Arbitrary id, Device Path>
*/
StringMap ImageAdapterSg::GetScanDirs() const
{
    StringMap map;
    int i = 0;
    QStringList paths = m_sg.GetDirList();
    for (const auto& path : std::as_const(paths))
        map.insert(i++, path);
    return map;
}


/*!
 \brief Get absolute filepath for a remote image
 \details For the SG node the path of SG dir with most space is returned
 \param im Image
 \return QString Absolute filepath
*/
QString ImageAdapterSg::GetAbsFilePath(const ImagePtrK &im) const
{
    if (im->IsDevice())
        return m_sg.FindNextDirMostFree();
    return im->m_filePath.startsWith("/") ? im->m_filePath
                                          : m_sg.FindFile(im->m_filePath);
}


// Database fields used by several image queries
static constexpr const char* kDBColumns {
"file_id, filename, name, dir_id, type, modtime, size, "
"extension, date, hidden, orientation, angle, path, zoom"
// Id, filepath, basename, parentId, type, modtime, size,
// extension, image date, hidden, orientation, cover id, comment, device id
};

/*!
\brief Create image from Db query data
\param query Db query result
\return ImageItem An image object
*/
template <class FS>
ImageItem *ImageDb<FS>::CreateImage(const MSqlQuery &query) const
{
    auto *im = new ImageItem(FS::ImageId(query.value(0).toInt()));

    // Ordered as per kDBColumns
    im->m_filePath      = query.value(1).toString();
    im->m_baseName      = query.value(2).toString();
    im->m_parentId      = FS::ImageId(query.value(3).toInt());
    im->m_type          = query.value(4).toInt();
    im->m_modTime       = std::chrono::seconds(query.value(5).toInt());
    im->m_size          = query.value(6).toInt();
    im->m_extension     = query.value(7).toString();
    im->m_date          = std::chrono::seconds(query.value(8).toUInt());
    im->m_isHidden      = query.value(9).toBool();
    im->m_orientation   = query.value(10).toInt();
    im->m_userThumbnail = FS::ImageId(query.value(11).toInt());
    im->m_comment       = query.value(12).toString();
    im->m_device        = query.value(13).toInt();
    im->m_url           = FS::MakeFileUrl(im->m_filePath);

    if (im->IsFile())
    {
        // Only pics/vids have thumbs
        QString thumbPath(FS::ThumbPath(*im));
        QString devPath(FS::ThumbDir(im->m_device));
        QString url(FS::MakeThumbUrl(devPath, thumbPath));

        im->m_thumbPath = FS::GetAbsThumbPath(devPath, thumbPath);
        im->m_thumbNails.append(qMakePair(im->m_id, url));
    }
    return im;
}


/*!
 * \brief Read database images/dirs by id
 * \param[in] ids Comma-separated list of ids
 * \param[in,out] files List of files
 * \param[in,out] dirs List of dirs
 * \param[in] refine SQL clause to refine selection & apply ordering
 * \return int Number of items matching query, -1 on SQL error
 */
template <class FS>
int ImageDb<FS>::GetImages(const QString &ids, ImageList &files, ImageList &dirs,
                           const QString &refine) const
{
    if (ids.isEmpty())
        return 0;

    QString select = QString("file_id IN (%1) %2").arg(FS::DbIds(ids), refine);
    return ReadImages(dirs, files, select);
}


/*!
 \brief Read immediate children of a dir
 \param[in,out] dirs List of child subdirs
 \param[in,out] files List of child files
 \param[in] ids Comma-separated list of dir ids
 \param[in] refine SQL clause to refine selection & apply ordering
 \return int Number of items matching query, -1 on SQL error
*/
template <class FS>
int ImageDb<FS>::GetChildren(const QString &ids, ImageList &files, ImageList &dirs,
                const QString &refine) const
{
    QString select = QString("dir_id IN (%1) %2").arg(FS::DbIds(ids), refine);
    return ReadImages(dirs, files, select);
}


/*!
 \brief Read a dir and its immediate children from Db
 \param[in] id Dir id
 \param[out] parent Dir item
 \param[in,out] dirs Ordered/filtered child subdirs
 \param[in,out] files Ordered/filtered child files
 \param[in] refine SQL clause for filtering/ordering child images
 \return int Number of items matching query.
*/
template <class FS>
int ImageDb<FS>::GetDirectory(int id, ImagePtr &parent,
                              ImageList &files, ImageList &dirs,
                              const QString &refine) const
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(QString("SELECT %1 FROM %2 "
                          "WHERE (dir_id = :ID1 OR file_id = :ID2) "
                          "%3;").arg(kDBColumns, m_table, refine));

    // Qt < 5.4 won't bind multiple occurrences
    int dbId = FS::DbId(id);
    query.bindValue(":ID1", dbId);
    query.bindValue(":ID2", dbId);

    if (!query.exec())
    {
        MythDB::DBError(DBLOC, query);
        return -1;
    }
    while (query.next())
    {
        ImagePtr im(CreateImage(query));

        if (im->IsFile())
            files.append(im);
        else if (im->m_id == id)
            parent = im;
        else
            dirs.append(im);
    }
    return query.size();
}


/*!
 \brief Return images and all of their descendants.

 \param[in] ids Image ids
 \param[in,out] files Ordered/filtered files
 \param[in,out] dirs  Ordered/filtered dirs
*/
template <class FS>
bool ImageDb<FS>::GetDescendants(const QString &ids,
                                 ImageList &files, ImageList &dirs) const
{
    if (ids.isEmpty())
        return false;

    if (ReadImages(dirs, files, QString("file_id IN (%1)").arg(FS::DbIds(ids))) < 0)
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    QString sql =
            QString("SELECT %1"
                    ", LENGTH(filename) - LENGTH(REPLACE(filename, '/', ''))"
                    " AS depth "
                    "FROM %2 WHERE filename LIKE :PREFIX "
                    "ORDER BY depth;").arg(kDBColumns,m_table);

    for (const auto& im1 : std::as_const(dirs))
    {
        query.prepare(sql);
        query.bindValue(":PREFIX", im1->m_filePath + "/%");

        if (!query.exec())
        {
            MythDB::DBError(DBLOC, query);
            return false;
        }

        while (query.next())
        {
            ImagePtr im2(CreateImage(query));
            if (im2->IsDirectory())
                dirs.append(im2);
            else
                files.append(im2);
        }
    }
    return true;
}


/*!
 \brief Returns all files in the sub-tree of a dir.
 \param[in,out] files List of images within sub-tree. Direct children first, then
  depth-first traversal of peer sub-dirs. Each level ordered as per refine criteria
 \param id Dir id
 \param refine SQL clause defining filter & ordering criteria
*/
template <class FS>
bool ImageDb<FS>::GetImageTree(int id, ImageList &files, const QString &refine) const
{
    // Load starting children
    ImageList dirs;
    if (GetChildren(QString::number(id), files, dirs, refine) < 0)
        return false;

    for (const auto& im : std::as_const(dirs))
        if (!GetImageTree(im->m_id, files, refine))
            return false;
    return true;
}


/*!
 * \brief Read all database images and dirs as map. No filters or ordering applied.
 * \param[in,out] files Map <filepath, image>
 * \param[in,out] dirs Map <filepath, dir>
 */
template <class FS>
bool ImageDb<FS>::ReadAllImages(ImageHash &files, ImageHash &dirs) const
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(QString("SELECT %1 FROM %2").arg(kDBColumns, m_table));

    if (!query.exec())
    {
        MythDB::DBError(DBLOC, query);
        return false;
    }

    while (query.next())
    {
        ImagePtr im(CreateImage(query));
        if (im->IsDirectory())
            dirs.insert(im->m_filePath, im);
        else
            files.insert(im->m_filePath, im);
    }
    return true;
}


/*!
 \brief Clear Db for device & remove device
 \param devId Device id, 0 to clear all devices
 \param action The myth protocol message. Determines whether/how much information is removed from the database.
 \return Either list of ids that have been deleted or "ALL" with list of
 filepath prefixes that will remove device images from the UI image cache
*/
template <class FS>
void ImageDb<FS>::ClearDb(int devId, const QString &action)
{
    if (action == "DEVICE CLOSE ALL")
        // Retain Db images when closing UI
        return;

    MSqlQuery query(MSqlQuery::InitCon());

    if (action == "DEVICE CLEAR ALL")
    {
        // Clear images from all devices. Reset auto-increment
        query.prepare(QString("TRUNCATE TABLE %1;").arg(m_table));

        if (!query.exec())
            MythDB::DBError(DBLOC, query);
    }
    else // Actions DEVICE REMOVE & DEVICE EJECT
    {
        // Delete all images of the device
        query.prepare(QString("DELETE IGNORE FROM %1 WHERE zoom = :FS;").arg(m_table));
        query.bindValue(":FS", devId);

        if (!query.exec())
            MythDB::DBError(DBLOC, query);
    }
}


/*!
 \brief Adds new image to database, optionally checking for existing filepath
 \param im Image to add
 \param checkForDuplicate If true, the image will not be added if its filepath
 already exists in Db.
 \return int Id of new image or the existing image with same filepath
*/
template <class FS>
int ImageDb<FS>::InsertDbImage(ImageItemK &im, bool checkForDuplicate) const
{
    MSqlQuery query(MSqlQuery::InitCon());

    if (checkForDuplicate)
    {
        query.prepare(QString("SELECT file_id FROM %1 WHERE filename = :NAME;")
                      .arg(m_table));

        query.bindValue(":NAME", im.m_filePath);

        if (!query.exec())
        {
            MythDB::DBError(DBLOC, query);
            return -1;
        }

        if (query.size() > 0)
        {
            LOG(VB_FILE, LOG_DEBUG, QString("Image: %1 already exists in Db")
                .arg(im.m_filePath));
            return query.value(0).toInt();
        }
    }

    query.prepare(QString("INSERT INTO %1 (%2) VALUES (0, "
                          ":FILEPATH, :NAME,      :PARENT, :TYPE,   :MODTIME, "
                          ":SIZE,     :EXTENSION, :DATE,   :HIDDEN, :ORIENT, "
                          ":COVER,    :COMMENT,   :FS);").arg(m_table, kDBColumns));

    query.bindValueNoNull(":FILEPATH",  im.m_filePath);
    query.bindValueNoNull(":NAME",      FS::BaseNameOf(im.m_filePath));
    query.bindValue(":FS",        im.m_device);
    query.bindValue(":PARENT",    FS::DbId(im.m_parentId));
    query.bindValue(":TYPE",      im.m_type);
    query.bindValue(":MODTIME",   static_cast<qint64>(im.m_modTime.count()));
    query.bindValue(":SIZE",      im.m_size);
    query.bindValueNoNull(":EXTENSION", im.m_extension);
    query.bindValue(":DATE",      static_cast<qint64>(im.m_date.count()));
    query.bindValue(":ORIENT",    im.m_orientation);
    query.bindValueNoNull(":COMMENT",   im.m_comment);
    query.bindValue(":HIDDEN",    im.m_isHidden);
    query.bindValue(":COVER",     FS::DbId(im.m_userThumbnail));

    if (query.exec())
        return FS::ImageId(query.lastInsertId().toInt());

    MythDB::DBError(DBLOC, query);
    return -1;
}


/*!
 * \brief Updates or creates database image or dir
 * \details Item does not need to pre-exist
 * \param im Image or dir
 */
template <class FS>
bool ImageDb<FS>::UpdateDbImage(ImageItemK &im) const
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(QString
                  ("UPDATE %1 SET "
                   "filename = :FILEPATH,  name = :NAME, "
                   "dir_id = :PARENT,    type = :TYPE, "
                   "modtime = :MODTIME,   size = :SIZE, "
                   "extension = :EXTENSION, date = :DATE,   zoom = :FS, "
                   "hidden = :HIDDEN,    orientation = :ORIENT, "
                   "angle = :COVER,     path = :COMMENT "
                   "WHERE file_id = :ID;").arg(m_table));

    query.bindValue(":ID",        FS::DbId(im.m_id));
    query.bindValue(":FILEPATH",  im.m_filePath);
    query.bindValue(":NAME",      FS::BaseNameOf(im.m_filePath));
    query.bindValue(":PARENT",    FS::DbId(im.m_parentId));
    query.bindValue(":TYPE",      im.m_type);
    query.bindValue(":MODTIME",   static_cast<qint64>(im.m_modTime.count()));
    query.bindValue(":SIZE",      im.m_size);
    query.bindValue(":EXTENSION", im.m_extension);
    query.bindValue(":DATE",      static_cast<qint64>(im.m_date.count()));
    query.bindValue(":FS",        im.m_device);
    query.bindValue(":HIDDEN",    im.m_isHidden);
    query.bindValue(":ORIENT",    im.m_orientation);
    query.bindValue(":COVER",     FS::DbId(im.m_userThumbnail));
    query.bindValueNoNull(":COMMENT", im.m_comment);

    if (query.exec())
        return true;

    MythDB::DBError(DBLOC, query);
    return false;
}


/*!
 * \brief Remove images/dirs from database
 * \details Item does not need to exist in db
 * \param imList List of items to delete
 * \return QStringList List of ids that were successfully removed
 */
template <class FS>
QStringList ImageDb<FS>::RemoveFromDB(const ImageList &imList) const
{
    QStringList ids;
    if (!imList.isEmpty())
    {
        for (const auto& im : std::as_const(imList))
            ids << QString::number(FS::DbId(im->m_id));

        QString idents = ids.join(",");
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare(QString("DELETE IGNORE FROM %1 WHERE file_id IN (%2);")
                      .arg(m_table, idents));

        if (!query.exec())
        {
            MythDB::DBError(DBLOC, query);
            return {};
        }
    }
    return ids;
}


/*!
 * \brief Sets hidden status of an image/dir in database
 * \param hide True = hidden, False = unhidden
 * \param ids List of item ids
 * \return bool False if db update failed
 */
template <class FS>
bool ImageDb<FS>::SetHidden(bool hide, const QString &ids) const
{
    if (ids.isEmpty())
        return false;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(QString("UPDATE %1 SET "
                          "hidden = :HIDDEN "
                          "WHERE file_id IN (%2);").arg(m_table, FS::DbIds(ids)));
    query.bindValue(":HIDDEN", hide ? 1 : 0);

    if (query.exec())
        return true;

    MythDB::DBError(DBLOC, query);
    return false;
}


/*!
 * \brief Set the thumbnail(s) to be used for a dir
 * \param dir Dir id
 * \param id Image id to use as cover/thumbnail
 */
template <class FS>
bool ImageDb<FS>::SetCover(int dir, int id) const
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(QString("UPDATE %1 SET "
                          "angle = :COVER "
                          "WHERE file_id = :DIR").arg(m_table));
    query.bindValue(":COVER", FS::DbId(id));
    query.bindValue(":DIR", FS::DbId(dir));
    \
    if (query.exec())
        return true;

    MythDB::DBError(DBLOC, query);
    return false;
}


/*!
 \brief Sets image orientation in Db
 \param id Image id
 \param orientation Exif orientation code
*/
template <class FS>
bool ImageDb<FS>::SetOrientation(int id, int orientation) const
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(QString("UPDATE %1 SET ").arg(m_table) +
                  "orientation = :ORIENTATION "
                  "WHERE file_id = :ID");
    query.bindValue(":ORIENTATION", orientation);
    query.bindValue(":ID", FS::DbId(id));
    \
    if (query.exec())
        return true;

    MythDB::DBError(DBLOC, query);
    return false;
}


/*!
 \brief Read selected database images/dirs
 \param[out] dirs List of the dirs
 \param[out] files List of the files
 \param selector SQL clause specifying selection and ordering of images
 \return int Number of items retreved
*/
template <class FS>
int ImageDb<FS>::ReadImages(ImageList &dirs, ImageList &files,
                            const QString &selector) const
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(QString("SELECT %1 FROM %2 WHERE %3")
                  .arg(kDBColumns, m_table, selector));
    if (!query.exec())
    {
        MythDB::DBError(DBLOC, query);
        return -1;
    }

    while (query.next())
    {
        ImagePtr im(CreateImage(query));

        if (im->IsFile())
            files.append(im);
        else
            dirs.append(im);
    }
    return query.size();
}


/*!
 \brief Return counts of dirs, pics, videos and size in the subtree of a dir.
 \param[in] id Dir id
 \param[in] all Sum whole table (without filtering on dir path)
 \param[in,out] dirs Number of dirs in parent sub-tree
 \param[in,out] pics Number of pictures in parent sub-tree
 \param[in,out] videos Number of videos in parent sub-tree
 \param[in,out] sizeKb Size in KiB of parent sub-tree
*/
template <class FS>
void ImageDb<FS>::GetDescendantCount(int id, bool all, int &dirs,
                                     int &pics, int &videos, int &sizeKb) const
{
    QString whereClause;
    if (!all)
    {
        whereClause = "WHERE filename LIKE "
                      "( SELECT CONCAT(filename, '/%') "
                      "  FROM %2 WHERE file_id = :ID);";
    }

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(QString("SELECT SUM(type <= :FLDR) AS Fldr, "
                          "       SUM(type =  :PIC)  AS Pics, "
                          "       SUM(type =  :VID)  AS Vids, "
                          "       SUM(size / 1024) "
                          "FROM %2 %1;").arg(whereClause, m_table));

    query.bindValue(":FLDR", kDirectory);
    query.bindValue(":PIC",  kImageFile);
    query.bindValue(":VID",  kVideoFile);
    if (!all)
        query.bindValue(":ID", FS::DbId(id));

    if (!query.exec())
    {
        MythDB::DBError(DBLOC, query);
    }
    else if (query.next())
    {
        dirs   += query.value(0).toInt();
        pics   += query.value(1).toInt();
        videos += query.value(2).toInt();
        sizeKb += query.value(3).toInt();
    }
}


/*!
 * \brief SG database constructor
 */
ImageDbSg::ImageDbSg() : ImageDb(DB_TABLE)
{
    // Be has a single SG device
    OpenDevice(IMAGE_STORAGE_GROUP, STORAGE_GROUP_MOUNT);
}


/*!
 \brief Local database constructor
*/
ImageDbLocal::ImageDbLocal()
    : ImageDb(QString("`%1_%2`").arg(DB_TABLE, gCoreContext->GetHostName()))
{
    // Remove any table leftover from a previous FE crash
    DropTable();
}


/*!
 \brief Remove local image table
*/
void ImageDbLocal::DropTable()
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(QString("DROP TABLE IF EXISTS %1;").arg(m_table));
    if (query.exec())
        m_dbExists = false;
    else
        MythDB::DBError(DBLOC, query);
}


/*!
 \brief Create local database table, if it doesn't exist
*/
bool ImageDbLocal::CreateTable()
{
    if (m_dbExists)
        return true;

    MSqlQuery query(MSqlQuery::InitCon());

    // Create temporary table
    query.prepare(QString("CREATE TABLE %1 LIKE %2;").arg(m_table, DB_TABLE));
    if (query.exec())
    {
        // Store it in memory only
        query.prepare(QString("ALTER TABLE %1 ENGINE = MEMORY;").arg(m_table));
        if (query.exec())
        {
            m_dbExists = true;
            LOG(VB_FILE, LOG_DEBUG, QString("Created Db table %1").arg(m_table));
            return true;
        }
    }
    MythDB::DBError(DBLOC, query);

    // Clean up after failure
    query.prepare(QString("DROP TABLE IF EXISTS %1;").arg(m_table));
    query.exec();
    return false;
}


/*!
 * \brief Task to read all metadata from file
 */
class ReadMetaThread : public QRunnable
{
public:
    ReadMetaThread(ImagePtrK im, QString path)
        : m_im(std::move(im)), m_path(std::move(path)) {}

    void run() override // QRunnable
    {
        QStringList tags;
        QString     orientation;
        QString     size;

        // Read metadata for files only
        if (m_im->IsFile())
        {
            ImageMetaData *metadata = (m_im->m_type == kVideoFile)
                    ? ImageMetaData::FromVideo(m_path)
                    : ImageMetaData::FromPicture(m_path);
            tags        = metadata->GetAllTags();
            orientation = Orientation(m_im->m_orientation).Description();
            size        = ImageAdapterBase::FormatSize(m_im->m_size / 1024);
            delete metadata;
        }

        // Add identifier at front
        tags.prepend(QString::number(m_im->m_id));

        // Include file info
        tags << ImageMetaData::ToString(EXIF_MYTH_HOST,   "Host",
                                        gCoreContext->GetHostName());
        tags << ImageMetaData::ToString(EXIF_MYTH_PATH,   "Path",
                                        ImageAdapterBase::PathOf(m_path));
        tags << ImageMetaData::ToString(EXIF_MYTH_NAME,   "Name",
                                        ImageAdapterBase::BaseNameOf(m_path));
        tags << ImageMetaData::ToString(EXIF_MYTH_SIZE,   "Size", size);
        tags << ImageMetaData::ToString(EXIF_MYTH_ORIENT, "Orientation",
                                        orientation);

        MythEvent me("IMAGE_METADATA", tags);
        gCoreContext->SendEvent(me);
    }

private:
    ImagePtrK m_im;
    QString   m_path;
};


/*!
 \brief Read meta data for an image
 \details Reads exif tags from a picture or FFMPEG video tags
 \param id Image id
 \return QStringList Error message or "OK", seperator token,
 list of \<tag name\>\<seperator\><tag value\>.
 Clients must use the embedded seperator to split the tags.
*/
template <class DBFS>
QStringList ImageHandler<DBFS>::HandleGetMetadata(const QString &id) const
{
    // Find image in DB
    ImageList files;
    ImageList dirs;
    if (DBFS::GetImages(id, files, dirs) != 1)
        RESULT_ERR("Image not found", QString("Unknown image %1").arg(id))

    ImagePtr im = files.isEmpty() ? dirs[0] : files[0];

    QString absPath = DBFS::GetAbsFilePath(im);
    if (absPath.isEmpty())
        RESULT_ERR("Image not found",
                   QString("File %1 not found").arg(im->m_filePath))

    auto *worker = new ReadMetaThread(im, absPath);

    MThreadPool::globalInstance()->start(worker, "ImageMetaData");

    RESULT_OK(QString("Fetching metadata for %1").arg(id))
}


/*!
 \brief Change name of an image/dir
 \details Renames file, updates db and thumbnail
 \param id File/dir id
 \param newBase New file basename
 \return QStringList Error message or "OK"
*/
template <class DBFS>
QStringList ImageHandler<DBFS>::HandleRename(const QString &id,
                                             const QString &newBase) const
{
    // Sanity check new name
    if (newBase.isEmpty() || newBase.contains("/") || newBase.contains("\\"))
        RESULT_ERR("Invalid name", QString("Invalid name %1").arg(newBase))

    // Find image in DB
    ImageList files;
    ImageList dirs;
    if (DBFS::GetImages(id, files, dirs) != 1)
        RESULT_ERR("Image not found", QString("Image %1 not in Db").arg(id))

    ImagePtr im = files.isEmpty() ? dirs[0] : files[0];

    // Find file
    QString oldPath = DBFS::GetAbsFilePath(im);
    if (oldPath.isEmpty())
        RESULT_ERR("Image not found",
                   QString("File %1 not found").arg(im->m_filePath))

    // Generate new filename
    QFileInfo oldFi = QFileInfo(oldPath);
    QString newName = im->IsDirectory()
            ? newBase : QString("%1.%2").arg(newBase, oldFi.suffix());

    im->m_filePath = DBFS::ConstructPath(DBFS::PathOf(im->m_filePath), newName);

    // Ensure no SG duplicate files are created. (Creating clone dirs is ok)
    if (im->IsFile())
    {
        QString existPath = DBFS::GetAbsFilePath(im);
        if (!existPath.isEmpty())
            RESULT_ERR("Filename already used",
                       QString("Renaming %1 to %2 will create a duplicate of %3")
                          .arg(oldPath, im->m_filePath, existPath))
    }

    // Rename file or directory
    QString newPath = oldFi.dir().absoluteFilePath(newName);
    if (!QFile::rename(oldPath, newPath))
        RESULT_ERR("Rename failed",
                   QString("Rename of %1 -> %2 failed").arg(oldPath, newPath))

    if (im->IsDirectory())
    {
        // Dir name change affects path of all sub-dirs & files and their thumbs
        HandleScanRequest("START");
    }
    else // file
    {
        // Update db
        DBFS::UpdateDbImage(*im);

        // Image is modified, not deleted
        QStringList mesg("");
        mesg << QString::number(im->m_id);

        // Rename thumbnail.
        m_thumbGen->MoveThumbnail(im);

        // Notify clients of changed image
        DBFS::Notify("IMAGE_DB_CHANGED", mesg);
    }
    RESULT_OK(QString("Renamed %1 -> %2").arg(oldPath, newPath))
}


/*!
 \brief Deletes images/dirs
 \details Removes files and dirs, updates db and thumbnails. Dirs containing
 other files will not be deleted. Only fails if nothing is deleted.
 \param ids Csv list of dir/file ids
 \return QStringList Error message or "OK"
*/
template <class DBFS>
QStringList ImageHandler<DBFS>::HandleDelete(const QString &ids) const
{
    // Get subtree of all files
    ImageList files;
    ImageList dirs;
    // Dirs will be in depth-first order, (subdirs after parent)
    DBFS::GetDescendants(ids, files, dirs);

    // Remove files from filesystem first
    RemoveFiles(files);
    // ... then dirs, which should now be empty
    RemoveFiles(dirs);

    // Fail if nothing deleted
    if (files.isEmpty() && dirs.isEmpty())
        RESULT_ERR("Delete failed", QString("Delete of %1 failed").arg(ids))

    // Update Db
    DBFS::RemoveFromDB(files + dirs);

    // Clean up thumbnails
    QStringList mesg(m_thumbGen->DeleteThumbs(files));

    // Notify clients of deleted ids
    DBFS::Notify("IMAGE_DB_CHANGED", mesg);

    return QStringList("OK");
}


/*!
 \brief Creates images for files created by a copy operation
 \details Creates skeleton database entries from image state definitions in order
 to retain state of the copied images. Initiates a scan to populate them fully
 and generate thumbnails. This retains
 \param defs A list of image definitions in the form
 \<id\>\<sep\>\<type\>\<sep\>\<filepath\>\<sep\>\<hidden\>
 \<sep\>\<orientation\>\<sep\>\<cover id\>
 where \<sep\> is the first list item.
 Dirs must follow their children (files & subdirs)
 \return QStringList Error message or "OK"
*/
template <class DBFS>
QStringList ImageHandler<DBFS>::HandleDbCreate(QStringList defs) const
{
    if (defs.isEmpty())
        RESULT_ERR("Copy Failed", "Empty defs")

    // First item is the field seperator
    const QString separator = defs.takeFirst();

    // Convert cover ids to their new equivalent. Map<source id, new id>
    // Dirs follow their children so new cover ids will be defined before they
    // are used
    QHash<QString, int> idMap;

    // Create skeleton Db images using copied settings.
    // Scanner will update other attributes
    ImageItem im;
    for (const auto& def : std::as_const(defs))
    {
        QStringList aDef = def.split(separator);

        // Expects id, type, path, hidden, orientation, cover
        if (aDef.size() != 6)
        {
            // Coding error
            LOG(VB_GENERAL, LOG_ERR,
                LOC + QString("Bad definition: (%1)").arg(def));
            continue;
        }

        LOG(VB_FILE, LOG_DEBUG, LOC + QString("Creating %1").arg(aDef.join(",")));

        im.m_type          = aDef[1].toInt();
        im.m_filePath      = aDef[2];
        im.m_isHidden      = (aDef[3].toInt() != 0);
        im.m_orientation   = aDef[4].toInt();
        im.m_userThumbnail = idMap.value(aDef[5]);

        // Don't insert duplicate filepaths
        int newId = DBFS::InsertDbImage(im, true);

        // Record old->new id map in case it's being used as a cover
        idMap.insert(aDef[0], newId);
    }
    HandleScanRequest("START");

    RESULT_OK("Created Db images")
}


/*!
 \brief Updates images that have been renamed.
 \details Updates filepaths of moved images, renames thumbnail and initiates
 scanner to repair other attributes
 \param ids Csv of image ids to rename
 \param srcPath Images current parent path
 \param destPath Images new parent path
 \return QStringList Error message or "OK"
*/
template <class DBFS>
QStringList ImageHandler<DBFS>::HandleDbMove(const QString &ids,
                                             const QString &srcPath,
                                             QString destPath) const
{
    // Sanity check new path
    if (destPath.contains(".."))
        RESULT_ERR("Invalid path", QString("Invalid path %1").arg(destPath))

    // Get subtrees of renamed files
    ImageList images;
    ImageList dirs;
    ImageList files;
    bool ok = DBFS::GetDescendants(ids, files, dirs);
    images << dirs << files;

    if (!ok || images.isEmpty())
        RESULT_ERR("Image not found", QString("Images %1 not in Db").arg(ids))

    if (!destPath.isEmpty() && !destPath.endsWith(QChar('/')))
        destPath.append("/");

    // Update path of images only. Scanner will repair parentId
    for (const auto& im : std::as_const(images))
    {
        QString old = im->m_filePath;

        if (srcPath.isEmpty())
        {
            // Image in SG root
            im->m_filePath.prepend(destPath);
        }
        else if (im->m_filePath.startsWith(srcPath))
        {
            // All other images
            im->m_filePath.replace(srcPath, destPath);
        }
        else
        {
            // Coding error
            LOG(VB_GENERAL, LOG_ERR,
                LOC + QString("Bad image: (%1 -> %2)").arg(srcPath, destPath));
            continue;
        }

        LOG(VB_FILE, LOG_DEBUG,
            LOC + QString("Db Renaming %1 -> %2").arg(old, im->m_filePath));

        DBFS::UpdateDbImage(*im);

        // Rename thumbnail
        if (im->IsFile())
            m_thumbGen->MoveThumbnail(im);
    }
    HandleScanRequest("START");

    RESULT_OK(QString("Moved %1 from %2 -> %3").arg(ids, srcPath, destPath))
}


/*!
 \brief Hides/unhides images/dirs
 \details Updates hidden status in db and updates clients
 \param hide hide flag: 0 = Show, 1 = Hide
 \param ids Csv list of file/dir ids
 \return QStringList Error message or "OK"
*/
template <class DBFS>
QStringList ImageHandler<DBFS>::HandleHide(bool hide, const QString &ids) const
{
    if (!DBFS::SetHidden(hide, ids))
        RESULT_ERR("Hide failed", QString("Db hide failed for %1").arg(ids))

    // Send changed ids only (none deleted)
    QStringList mesg = QStringList("") << ids;
    DBFS::Notify("IMAGE_DB_CHANGED", mesg);

    RESULT_OK(QString("Images %1 now %2hidden").arg(ids, hide ? "" : "un"))
}


/*!
 \brief Change orientation of pictures by applying a transformation
 \details Updates orientation in Db and thumbnail. Does not update file Exif data.
 Only fails if nothing is modified.
 \param transform transformation id,
 \param ids Csv list of file ids
 \return QStringList Error message or "OK"
*/
template <class DBFS>
QStringList ImageHandler<DBFS>::HandleTransform(int transform,
                                                const QString &ids) const
{
    if (transform < kResetToExif || transform > kFlipVertical)
        RESULT_ERR("Transform failed", QString("Bad transform %1").arg(transform))

    ImageList files;
    ImageList dirs;
    if (DBFS::GetImages(ids, files, dirs) < 1 || files.isEmpty())
        RESULT_ERR("Image not found", QString("Images %1 not in Db").arg(ids))

    // Update db
    for (const auto& im : std::as_const(files))
    {
        int old           = im->m_orientation;
        im->m_orientation = Orientation(im->m_orientation).Transform(transform);

        // Update Db
        if (DBFS::SetOrientation(im->m_id, im->m_orientation))
        {
            LOG(VB_FILE, LOG_DEBUG, LOC + QString("Transformed %1 from %2 to %3")
                .arg(im->m_filePath).arg(old).arg(im->m_orientation));
        }
    }

    // Images are changed, not deleted
    QStringList mesg("");

    // Clean up thumbnails
    mesg << m_thumbGen->DeleteThumbs(files);

    // Notify clients of changed images
    DBFS::Notify("IMAGE_DB_CHANGED", mesg);

    return QStringList("OK");
}


/*!
 \brief Creates new image directories
 \details Creates dirs in filesystem and optionally updates Db.
 \param destId Parent dir
 \param rescan Whether to start a scan after creating the dirs
 \param relPaths List of relative paths of new dirs
 \return QStringList Error message or "OK"
*/
template <class DBFS>
QStringList ImageHandler<DBFS>::HandleDirs(const QString &destId,
                                           bool rescan,
                                           const QStringList &relPaths) const
{
    // Find image in DB
    ImageList files;
    ImageList dirs;
    if (DBFS::GetImages(destId, files, dirs) != 1 || dirs.isEmpty())
        RESULT_ERR("Destination not found",
                   QString("Image %1 not in Db").arg(destId))

    // Find dir. SG device (Photographs) uses most-free filesystem
    QString destPath = DBFS::GetAbsFilePath(dirs[0]);
    if (destPath.isEmpty())
        RESULT_ERR("Destination not found",
                   QString("Dest dir %1 not found").arg(dirs[0]->m_filePath))

    QDir destDir(destPath);
    bool succeeded = false;
    for (const auto& relPath : std::as_const(relPaths))
    {
        // Validate dir name
        if (relPath.isEmpty() || relPath.contains("..") || relPath.startsWith(QChar('/')))
            continue;

        QString newPath = DBFS::ConstructPath(destDir.absolutePath(), relPath);
        if (!destDir.mkpath(relPath))
        {
            LOG(VB_GENERAL, LOG_ERR,
                LOC + QString("Failed to create dir %1").arg(newPath));
            continue;
        }
        LOG(VB_FILE, LOG_DEBUG, LOC + QString("Dir %1 created").arg(newPath));
        succeeded = true;
    }

    if (!succeeded)
        // Failures should only occur due to user input
        RESULT_ERR("Invalid Name", QString("Invalid name %1")
                   .arg(relPaths.join(",")))

    if (rescan)
        // Rescan to detect new dir
        HandleScanRequest("START");

    return QStringList("OK");
}


/*!
 \brief Updates/resets cover thumbnail for an image dir
 \param dir Directory id
 \param cover Id to use as cover. 0 resets dir to use its own thumbnail
 \return QStringList Error message or "OK"
*/
template <class DBFS>
QStringList ImageHandler<DBFS>::HandleCover(int dir, int cover) const
{
    if (!DBFS::SetCover(dir, cover))
        RESULT_ERR("Set Cover failed",
                   QString("Failed to set %1 to cover %2").arg(dir).arg(cover))

    // Image has changed, nothing deleted
    QStringList mesg = QStringList("") << QString::number(dir);
    DBFS::Notify("IMAGE_DB_CHANGED", mesg);

    RESULT_OK(QString("Cover of %1 is now %2").arg(dir).arg(cover));
}


/*!
 \brief Updates exclusion list for images
 \details Stores new exclusions setting & rescans. Exclusions is a global setting
 that dictates which files the scanner ignores. However it is set by any client
 (last writer wins). Glob characters * and ? are valid.
 \param exclusions Csv list of exclusion patterns
 \return QStringList Error message or "OK"
*/
template <class DBFS>
QStringList ImageHandler<DBFS>::HandleIgnore(const QString &exclusions) const
{
    // Save new setting. FE will have already saved it but not cleared the cache
    gCoreContext->SaveSettingOnHost("GalleryIgnoreFilter", exclusions, nullptr);

    // Rescan
    HandleScanRequest("START");

    RESULT_OK(QString("Using exclusions '%1'").arg(exclusions))
}


/*!
 \brief Process scan requests.
 \details Handles start scan, stop scan, clear Db and scan progress queries
 \param command IMAGE_SCAN, START | STOP | QUERY | CLEAR
 \param devId Device id. Only used for CLEAR
 \return QStringList Error message or "OK"
*/
template <class DBFS>
QStringList ImageHandler<DBFS>::HandleScanRequest(const QString &command,
                                                  int devId) const
{
    if (!m_scanner)
        RESULT_ERR("Missing Scanner", "Missing Scanner");

    if (command == "START")
    {
        // Must be dormant to start a scan
        if (m_scanner->IsScanning())
            RESULT_ERR("", "Scanner is busy");

        m_scanner->ChangeState(true);
        RESULT_OK("Scan requested");
    }
    else if (command == "STOP")
    {
        // Must be scanning to interrupt
        if (!m_scanner->IsScanning())
            RESULT_ERR("Scanner not running", "Scanner not running");

        m_scanner->ChangeState(false);
        RESULT_OK("Terminate scan requested");
    }
    else if (command == "QUERY")
    {
        return QStringList("OK") << m_scanner->GetProgress();
    }
    else if (command.startsWith(QString("DEVICE")))
    {
        m_scanner->EnqueueClear(devId, command);
        RESULT_OK(QString("Clearing device %1 %2").arg(command).arg(devId))
    }
    RESULT_ERR("Unknown command", QString("Unknown command %1").arg(command));
}


/*!
 \brief Creates thumbnails on-demand
 \details Display requests are the highest priority. Thumbnails required for an
image node will be created before those that are part of a directory thumbnail.
A THUMBNAIL_CREATED event is broadcast for each image.
 \param message For Directory flag, image id
*/
template <class DBFS>
QStringList ImageHandler<DBFS>::HandleCreateThumbnails
(const QStringList &message) const
{
    if (message.size() != 2)
        RESULT_ERR("Unknown Command",
                   QString("Bad request: %1").arg(message.join("|")))

    int priority = message.at(0).toInt()
                ? kDirRequestPriority : kPicRequestPriority;

    // get specific image details from db
    ImageList files;
    ImageList dirs;
    DBFS::GetImages(message.at(1), files, dirs);

    for (const auto& im : std::as_const(files))
        // notify clients when done; highest priority
        m_thumbGen->CreateThumbnail(im, priority, true);

    return QStringList("OK");
}


/*!
 \brief Deletes images and dirs from the filesystem
 \details Dirs will only be deleted if empty. Items are deleted in reverse
 order so that parent dirs at front of list will be empty once their subdirs
 at back of list have been deleted. Files/dirs that failed to delete
 are removed from the list.
 \param[in,out] images List of images/dirs to delete. On return the files that
 were successfully deleted.
*/
template <class DBFS>
void ImageHandler<DBFS>::RemoveFiles(ImageList &images) const
{
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    QMutableVectorIterator<ImagePtr> it(images);
#else
    QMutableListIterator<ImagePtr> it(images);
#endif
    it.toBack();
    while (it.hasPrevious())
    {
        ImagePtrK im = it.previous();

        // Remove file or directory
        QString absFilename = DBFS::GetAbsFilePath(im);

        bool success = !absFilename.isEmpty()
                && (im->IsFile() ? QFile::remove(absFilename)
                                 : QDir::root().rmdir(absFilename));
        if (success)
            LOG(VB_FILE, LOG_DEBUG, LOC + QString("Deleted %1").arg(absFilename));
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Can't delete %1").arg(absFilename));
            // Remove from list
            it.remove();
        }
    }
}


/*!
 \brief Generate SQL type filter clause
 \param type Filter pic/video/both
 \return QString SQL filter clause
*/
QString ImageDbReader::TypeSelector(int type)
{
    switch (type)
    {
    case kPicOnly:     return QString("AND type != %1").arg(kVideoFile);
    case kVideoOnly:   return QString("AND type != %1").arg(kImageFile);
    case kPicAndVideo: return "";
    }
    return "";
}


/*!
 \brief Sets filter/ordering SQL clause used when reading database according
 to current filter/sort settings
 \details Filters by 'show hidden' & 'show pics/vids/all'. Orders files & dirs
*/
void ImageDbReader::SetRefinementClause()
{
    m_refineClause = QString("%2 %3 "
                             "ORDER BY "
                             "CASE WHEN type <= %1 THEN %4, "
                             "CASE WHEN type >  %1 THEN %5 ")
            .arg(kDirectory)
            .arg(m_showHidden ? "" : "AND hidden = 0",
                 TypeSelector(m_showType),
                 OrderSelector(m_dirOrder),
                 OrderSelector(m_fileOrder));
}


/*!
 \brief Generate SQL ordering clause
 \param order Order criteria
 \return QString SQL order caluse
*/
QString ImageDbReader::OrderSelector(int order)
{
    // prepare the sorting statement
    switch (order)
    {
    default:
    case kSortByNameAsc:     return "name END ASC";
    case kSortByNameDesc:    return "name END DESC";
    case kSortByModTimeAsc:  return "modtime END ASC";
    case kSortByModTimeDesc: return "modtime END DESC";
    case kSortByExtAsc:      return "extension END ASC,  name ASC";
    case kSortByExtDesc:     return "extension END DESC, name DESC";
    case kSortBySizeAsc:     return "size END ASC,  name ASC";
    case kSortBySizeDesc:    return "size END DESC, name DESC";
    case kSortByDateAsc:     return "IF(date=0, modtime, date) END ASC";
    case kSortByDateDesc:    return "IF(date=0, modtime, date) END DESC";
    }
}


/*!
 \brief Return images (local and/or remote) for a dir and its direct children
 \param[in] id Dir id
 \param[out] parent Parent image
 \param[in,out] files Child files, filtered & ordered iaw current settings.
 \param[in,out] dirs Child dirs, filtered & ordered iaw current settings.
 \return int Number of images, including parent
*/
int ImageDbReader::GetDirectory(int id, ImagePtr &parent,
                                ImageList &files, ImageList &dirs) const
{
    // Only Root node will invoke both Db queries but result set will be small
    // For Root the SG is always ordered before local devices
    // Root node has no Db entry so the 2 queries will not overwrite the parent.
    int count = 0;
    if (!ImageItem::IsLocalId(id))
        count = m_remote->GetDirectory(id, parent, files, dirs, m_refineClause);
    if (m_dbExists && ImageItem::IsLocalParent(id))
        count += ImageHandler::GetDirectory(id, parent, files, dirs, m_refineClause);

    if (id == GALLERY_DB_ID)
    {
        // Add a Root node
        parent = ImagePtr(new ImageItem(GALLERY_DB_ID));
        parent->m_parentId = GALLERY_DB_ID;
        parent->m_type     = kDevice;

        ++count;
    }
    return count;
}


/*!
 \brief Returns images (local or remote but not a combination)
 \param[in] ids Image ids
 \param[in,out] files List of files, filtered & ordered iaw current settings.
 \param[in,out] dirs List of dirs, filtered & ordered iaw current settings.
 \return int Number of images
*/
int ImageDbReader::GetImages(const ImageIdList& ids,
                             ImageList &files, ImageList &dirs) const
{
    // Ids are either all local or all remote. GALLERY_DB_ID not valid
    StringPair lists = ImageItem::PartitionIds(ids);

    if (!lists.second.isEmpty())
        return m_remote->GetImages(lists.second, files, dirs, m_refineClause);
    if (m_dbExists && !lists.first.isEmpty())
        return ImageHandler::GetImages(lists.first, files, dirs, m_refineClause);
    return 0;
}


/*!
 \brief Return (local or remote) images that are direct children of a dir
 \param[in] id Directory id
 \param[in,out] files List of files, filtered & ordered iaw current settings.
 \param[in,out] dirs List of dirs, filtered & ordered iaw current settings.
 \return int Number of Images
*/
int ImageDbReader::GetChildren(int id, ImageList &files, ImageList &dirs) const
{
    int count = 0;
    if (!ImageItem::IsLocalId(id))
        count = m_remote->GetChildren(QString::number(id), files, dirs,
                                      m_refineClause);
    if (m_dbExists && ImageItem::IsLocalParent(id))
        count += ImageHandler::GetChildren(QString::number(id), files, dirs,
                                           m_refineClause);
    return count;
}


/*!
 \brief Return all (local or remote) images that are direct children of a dir
 \param[in] ids Directory ids, GALLERY_DB_ID not valid
 \param[in,out] files List of files, unfiltered & unordered
 \param[in,out] dirs List of dirs, unfiltered & unordered
*/
void ImageDbReader::GetDescendants(const ImageIdList &ids,
                                   ImageList &files, ImageList &dirs) const
{
    // Ids are either all local or all remote
    StringPair lists = ImageItem::PartitionIds(ids);

    if (!lists.second.isEmpty())
        m_remote->GetDescendants(lists.second, files, dirs);
    if (m_dbExists && !lists.first.isEmpty())
        ImageHandler::GetDescendants(lists.first, files, dirs);
}


/*!
 \brief Return all files (local or remote) in the sub-trees of a dir
 \param[in] id Dir id
 \param[in,out] files List of images within sub-tree. Ordered & filtered iaw current
 settings
*/
void ImageDbReader::GetImageTree(int id, ImageList &files) const
{
    if (!ImageItem::IsLocalId(id))
        m_remote->GetImageTree(id, files, m_refineClause);
    if (m_dbExists && ImageItem::IsLocalParent(id))
        ImageHandler::GetImageTree(id, files, m_refineClause);
}


/*!
 \brief Return counts of dirs, pics and videos in the subtree of a dir. Also dir size
 \param[in] id Dir id
 \param[in,out] dirs Number of dirs in parent sub-tree
 \param[in,out] pics Number of pictures in parent sub-tree
 \param[in,out] videos Number of videos in parent sub-tree
 \param[in,out] sizeKb Size in KiB of parent sub-tree
*/
void ImageDbReader::GetDescendantCount(int id, int &dirs, int &pics,
                                       int &videos, int &sizeKb) const
{
    if (id == GALLERY_DB_ID)
    {
        // Sum both unfiltered tables
        m_remote->GetDescendantCount(id, true, dirs, pics, videos, sizeKb);
        if (m_dbExists)
            ImageHandler::GetDescendantCount(id, true, dirs, pics, videos, sizeKb);
    }
    else if (!ImageItem::IsLocalId(id))
    {
        // Don't filter on SG path (it's blank)
        m_remote->GetDescendantCount(id, id == PHOTO_DB_ID,
                                     dirs, pics, videos, sizeKb);
    }
    else if (m_dbExists)
    {
        // Always filter on device/dir
        ImageHandler::GetDescendantCount(id, false, dirs, pics, videos, sizeKb);
    }
}


//! Backend Gallery instance
ImageManagerBe *ImageManagerBe::s_instance = nullptr;
//! Frontend Gallery instance
ImageManagerFe *ImageManagerFe::s_instance = nullptr;


/*!
 \brief Get Backend Gallery
 \return Backend Gallery singleton
*/
ImageManagerBe* ImageManagerBe::getInstance()
{
    if (!s_instance)
        s_instance = new ImageManagerBe();
    return s_instance;
}


/*!
 \brief Get Frontend Gallery
 \return Frontend Gallery singleton
*/
ImageManagerFe& ImageManagerFe::getInstance()
{
    if (!s_instance)
    {
        // Use saved settings
        s_instance = new ImageManagerFe
                (gCoreContext->GetNumSetting("GalleryImageOrder"),
                 gCoreContext->GetNumSetting("GalleryDirOrder"),
                 gCoreContext->GetBoolSetting("GalleryShowHidden"),
                 gCoreContext->GetNumSetting("GalleryShowType"),
                 gCoreContext->GetSetting("GalleryDateFormat"));
    }
    return *s_instance;
}


/*!
 \brief Create thumbnails or verify that they already exist
 \details A THUMB_AVAILABLE event will be generated for each thumbnail as
  soon as it exists.
 \param ids Images requiring thumbnails
 \param forFolder True if thumbnail is required for a directory image. UI
 aesthetics consider this to be lower priority than picture/video thumbnails.
*/
void ImageManagerFe::CreateThumbnails(const ImageIdList &ids, bool forFolder)
{
    // Split images into <locals, remotes>
    StringPair lists = ImageItem::PartitionIds(ids);

    if (!lists.second.isEmpty())
    {
        LOG(VB_FILE, LOG_DEBUG, LOC +
            QString("Sending CREATE_THUMBNAILS %1 (forFolder %2)")
            .arg(lists.second).arg(forFolder));

        QStringList message;
        message << QString::number(static_cast<int>(forFolder)) << lists.second;
        gCoreContext->SendEvent(MythEvent("CREATE_THUMBNAILS", message));
    }

    if (!lists.first.isEmpty())
    {
        LOG(VB_FILE, LOG_DEBUG, LOC +
            QString("Creating local thumbnails %1 (forFolder %2)")
            .arg(lists.first).arg(forFolder));

        QStringList message;
        message << QString::number(static_cast<int>(forFolder)) << lists.first;
        HandleCreateThumbnails(message);
    }
}


/*!
 \brief Handle scanner start/stop commands
 \param start True to start a scan, False to stop a scan
 \param local True to start scan of local devices, False to scan storage group
 \return QString Error message or empty
*/
QString ImageManagerFe::ScanImagesAction(bool start, bool local)
{
    QStringList command;
    command << (start ? "START" : "STOP");

    if (!local)
    {
        command.push_front("IMAGE_SCAN");
        bool ok = gCoreContext->SendReceiveStringList(command, true);
        return ok ? "" : command[1];
    }

    // Create database on first scan
    if (!CreateTable())
        return "Couldn't create database";

    QStringList err = HandleScanRequest(command[0]);
    return err[0] == "OK" ? "" : err[1];
}


/*!
 * \brief Returns storage group scanner status
 * \return QStringList State ("ERROR" | "OK"), SG scanner id "1",
 * Progress count, Total
 */
QStringList ImageManagerFe::ScanQuery()
{
    QStringList strList;
    strList << "IMAGE_SCAN" << "QUERY";

    if (!gCoreContext->SendReceiveStringList(strList))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Scan query failed : %1")
            .arg(strList.join(",")));
    }
    return strList;
}


/*!
 * \brief Hide/unhide images
 * \param hidden True to hide, False to show images
 * \param ids List of image ids
 * \return QString Error message, if not empty
 */
QString ImageManagerFe::HideFiles(bool hidden, const ImageIdList &ids)
{
    // Split images into <locals, remotes>
    StringPair lists = ImageItem::PartitionIds(ids);
    QString result = "";

    if (!lists.second.isEmpty())
    {
        QStringList message;
        message << "IMAGE_HIDE" << QString::number(static_cast<int>(hidden)) << lists.second;

        if (!gCoreContext->SendReceiveStringList(message, true))
            result = message[1];
    }

    if (!lists.first.isEmpty())
    {
        QStringList err = HandleHide(hidden, lists.first);
        if (err[0] != "OK")
            result = err[1];
    }
    return result;
}


/*!
 * \brief Apply an orientation transform to images
 * \param  transform Transformation to apply
 * \param  ids List of image ids
 * \return QString Error message, if not empty
 */
QString ImageManagerFe::ChangeOrientation(ImageFileTransform transform,
                                          const ImageIdList &ids)
{
    // Split images into <locals, remotes>
    StringPair lists = ImageItem::PartitionIds(ids);
    QString result = "";

    if (!lists.second.isEmpty())
    {
        QStringList message;
        message << "IMAGE_TRANSFORM" << QString::number(transform) << lists.second;

        if (!gCoreContext->SendReceiveStringList(message, true))
            result = message[1];
    }

    if (!lists.first.isEmpty())
    {
        QStringList err = HandleTransform(transform, lists.first);
        if (err[0] != "OK")
            result = err[1];
    }
    return result;
}


/*!
 * \brief Set image to use as a cover thumbnail(s)
 * \param  parent Id of directory to change
 * \param  cover Image id to use as cover
 * \return QString Error message, if not empty
 */
QString ImageManagerFe::SetCover(int parent, int cover)
{
    if (!ImageItem::IsLocalId(parent))
    {
        QStringList message;
        message << "IMAGE_COVER" << QString::number(parent) << QString::number(cover);

        bool ok = gCoreContext->SendReceiveStringList(message, true);
        return ok ? "" : message[1];
    }

    QStringList err = HandleCover(parent, cover);
    return err[0] == "OK" ? "" : err[1];
}


/*!
 * \brief Requests all exif/ffmpeg tags for an image, which returns by event
 * \param  id An image id
 */
void ImageManagerFe::RequestMetaData(int id)
{
    if (ImageItem::IsLocalId(id))
        HandleGetMetadata(QString::number(id));
    else
        gCoreContext->SendEvent(MythEvent("IMAGE_GET_METADATA", QString::number(id)));
}


//! Clear database & thumbnails of Storage Group images
void ImageManagerFe::ClearStorageGroup()
{
    QStringList message("IMAGE_SCAN");
    message << "DEVICE CLEAR ALL";
    gCoreContext->SendReceiveStringList(message, true);
}


/*!
 * \brief Set directories to ignore during scans of the storage group
 * \param excludes Comma separated list of dir names/patterns to exclude. Glob
 * characters * and ? permitted.
 * \return QString Error message, if not empty
 */
QString ImageManagerFe::IgnoreDirs(const QString &excludes)
{
    QStringList message("IMAGE_IGNORE");
    message << excludes;
    bool ok = gCoreContext->SendReceiveStringList(message, true);
    return ok ? "" : message[1];
}


/*!
 \brief Create directories
 \param parent Dir in which to create new dirs
 \param names List of dir names
 \param rescan Whether to scan after creating dirs
 \return QString Error message, if not empty
*/
QString ImageManagerFe::MakeDir(int parent, const QStringList &names, bool rescan)
{
    QString destId = QString::number(parent);

    if (!ImageItem::IsLocalId(parent))
    {
        QStringList message("IMAGE_CREATE_DIRS");
        message << destId << QString::number(static_cast<int>(rescan)) << names;
        bool ok = gCoreContext->SendReceiveStringList(message, true);
        return ok ? "" : message[1];
    }
    QStringList err = HandleDirs(destId, rescan, names);
    return (err[0] == "OK") ? "" : err[1];
}


/*!
 * \brief Rename an image
 * \param  im An image
 * \param  name New name of the file/dir (basename only, no path or extension)
 * \return QString Error message, if not empty
 */
QString ImageManagerFe::RenameFile(const ImagePtrK& im, const QString &name)
{
    if (!im->IsLocal())
    {
        QStringList message("IMAGE_RENAME");
        message << QString::number(im->m_id) << name;
        bool ok = gCoreContext->SendReceiveStringList(message, true);
        return ok ? "" : message[1];
    }
    QStringList err = HandleRename(QString::number(im->m_id), name);
    return (err[0] == "OK") ? "" : err[1];
}


/*!
 * \brief Copies database images (but not the files themselves).
 * \param destId Id of an image directory
 * \param images List of images to copy
 * \return QString Error message
 */
QString ImageManagerFe::CreateImages(int destId, const ImageListK &images)
{
    if (images.isEmpty())
        return "";

    // Define field seperator & include it in message
    const QString seperator("...");
    QStringList imageDefs(seperator);
    ImageIdList ids;
    for (const auto& im : std::as_const(images))
    {
        ids << im->m_id;

        // Copies preserve hide state, orientation & cover
        QStringList aDef;
        aDef << QString::number(im->m_id)
             << QString::number(im->m_type)
             << im->m_filePath
             << QString::number(static_cast<int>(im->m_isHidden))
             << QString::number(im->m_orientation)
             << QString::number(im->m_userThumbnail);

        imageDefs << aDef.join(seperator);
    }

    // Images are either all local or all remote
    if (ImageItem::IsLocalId(destId))
    {
        QStringList err = HandleDbCreate(imageDefs);
        return (err[0] == "OK") ? "" : err[1];
    }
    imageDefs.prepend("IMAGE_COPY");
    bool ok = gCoreContext->SendReceiveStringList(imageDefs, true);
    return ok ? "" : imageDefs[1];
}


/*!
 * \brief Moves database images (but not the files themselves).
 * \param destDir Destination image directory
 * \param images List of images to copy
 * \param srcPath Original parent path
 * \return QString Error message
 */
QString ImageManagerFe::MoveDbImages(const ImagePtrK& destDir, ImageListK &images,
                                     const QString &srcPath)
{
    QStringList idents;
    for (const auto& im : std::as_const(images))
        idents << QString::number(im->m_id);

    // Images are either all local or all remote
    if (destDir->IsLocal())
    {
        QStringList err = HandleDbMove(idents.join(","), srcPath,
                                       destDir->m_filePath);
        return (err[0] == "OK") ? "" : err[1];
    }

    QStringList message("IMAGE_MOVE");
    message << idents.join(",") << srcPath << destDir->m_filePath;
    bool ok = gCoreContext->SendReceiveStringList(message, true);
    return ok ? "" : message[1];
}


/*!
 * \brief Delete images
 * \param  ids List of image ids
 * \return QString Error message, if not empty
 */
QString ImageManagerFe::DeleteFiles(const ImageIdList &ids)
{
    StringPair lists = ImageItem::PartitionIds(ids);

    QString result = "";
    if (!lists.second.isEmpty())
    {
        QStringList message("IMAGE_DELETE");
        message << lists.second;

        bool ok = gCoreContext->SendReceiveStringList(message, true);
        if (!ok)
            result = message[1];
    }
    if (!lists.first.isEmpty())
    {
        QStringList err = HandleDelete(lists.first);
        if (err[0] != "OK")
            result = err[1];
    }
    return result;
}


/*!
 \brief Return a timestamp/datestamp for an image or dir
 \details Uses exif timestamp if defined, otherwise file modified date
 \param im Image or dir
 \return QString Time or date string formatted as per Myth general settings
*/
QString ImageManagerFe::LongDateOf(const ImagePtrK& im)
{
    if (im->m_id == GALLERY_DB_ID)
        return "";

    std::chrono::seconds secs = 0s;
    uint format = MythDate::kDateFull | MythDate::kAddYear;

    if (im->m_date > 0s)
    {
        secs = im->m_date;
        format |= MythDate::kTime;
    }
    else
    {
        secs = im->m_modTime;
    }

    return MythDate::toString(QDateTime::fromSecsSinceEpoch(secs.count()), format);
}


/*!
 \brief Return a short datestamp for thumbnail captions
 \details Uses exif date if defined, otherwise file modified date
 \param im Image or dir
 \return QString Date formatted as per Gallery caption date format.
*/
QString ImageManagerFe::ShortDateOf(const ImagePtrK& im) const
{
    if (im->m_id == GALLERY_DB_ID)
        return "";

    std::chrono::seconds secs(im->m_date > 0s ? im->m_date : im->m_modTime);
    return QDateTime::fromSecsSinceEpoch(secs.count()).date().toString(m_dateFormat);
}


/*!
   \brief Return translated device name
   \param im Image
   \return Translated name of the device
 */
QString ImageManagerFe::DeviceCaption(ImageItemK &im) const
{
    if (im.m_id == GALLERY_DB_ID)
        return tr("Gallery");
    if (im.m_id == PHOTO_DB_ID)
        return tr("Photographs");
    return im.IsLocal() ? DeviceName(im.m_device)
                        : m_remote->DeviceName(im.m_device);
}


/*!
 \brief Return a displayable name (with optional path) for an image.
 \details Uses device name rather than mount path for local devices
 \param im Image
 \param getPath If true, name will include path. Otherwise only the basename
 \return QString Filepath for display
*/
QString ImageManagerFe::CrumbName(ImageItemK &im, bool getPath) const
{
    if (im.IsDevice())
        return DeviceCaption(im);

    if (!getPath)
        return im.m_baseName;

    QString dev;
    QString path(im.m_filePath);

    if (im.IsLocal())
    {
        // Replace local mount path with device name
        path.remove(0, DeviceMount(im.m_device).size());
        dev = DeviceName(im.m_device);
    }
    return dev + path.replace("/", " > ");
}


void ImageManagerFe::CloseDevices(int devId, bool eject)
{
    QString reason { "DEVICE REMOVE" };
    if (devId == DEVICE_INVALID)
        reason = "DEVICE CLOSE ALL";
    else if (eject)
        reason = "DEVICE EJECT";
    HandleScanRequest(reason, devId);
}


/*!
 * \brief Detect and scan local devices
 * \return True if local devices exist
 */
bool ImageManagerFe::DetectLocalDevices()
{
    MediaMonitor *monitor = MediaMonitor::GetMediaMonitor();
    if (!monitor)
        return false;

    // Detect all local media
    QList<MythMediaDevice*> devices
            = monitor->GetMedias(MEDIATYPE_DATA | MEDIATYPE_MGALLERY);

    for (auto *dev : std::as_const(devices))
    {
        if (monitor->ValidateAndLock(dev) && dev->isUsable())
            OpenDevice(dev->getDeviceModel(), dev->getMountPath(), dev);
        else
            monitor->Unlock(dev);
    }

    if (DeviceCount() > 0)
    {
        // Close devices that are no longer present
        QList absentees = GetAbsentees();
        for (int devId : std::as_const(absentees))
            CloseDevices(devId);

        // Start local scan
        QString err = ScanImagesAction(true, true);
        if (!err.isEmpty())
            LOG(VB_GENERAL, LOG_ERR, LOC + err);
    }
    return DeviceCount() > 0;
}


/*!
 * \brief Manage events for local devices
 * \param event Appeared/disappeared events
 */
void ImageManagerFe::DeviceEvent(MythMediaEvent *event)
{
    MediaMonitor *monitor = MediaMonitor::GetMediaMonitor();

    if (!event || !monitor)
        return;

    MythMediaDevice *dev = event->getDevice();
    if (!dev)
        return;

    MythMediaType   type   = dev->getMediaType();
    MythMediaStatus status = dev->getStatus();

    LOG(VB_FILE, LOG_DEBUG, LOC +
        QString("Media event for %1 (%2) at %3, type %4, status %5 (was %6)")
        .arg(dev->getDeviceModel(), dev->getVolumeID(), dev->getMountPath())
        .arg(type).arg(status).arg(event->getOldStatus()));

    if (!(type & (MEDIATYPE_DATA | MEDIATYPE_MIXED | MEDIATYPE_MGALLERY)))
    {
        LOG(VB_FILE, LOG_DEBUG, LOC +
            QString("Ignoring event - wrong type %1").arg(type));
        return;
    }

    if (status == MEDIASTAT_USEABLE || status == MEDIASTAT_MOUNTED)
    {
        // New device. Lock it & scan
        if (monitor->ValidateAndLock(dev))
        {
            OpenDevice(dev->getDeviceModel(), dev->getMountPath(), dev);
            ScanImagesAction(true, true);
        }
        else
        {
            monitor->Unlock(dev);
        }
        return;
    }

    // Device has disappeared
    int devId = LocateMount(dev->getMountPath());
    if (devId != DEVICE_INVALID)
        CloseDevices(devId);
}


QString ImageManagerFe::CreateImport()
{
    auto *tmp = new QTemporaryDir(QDir::tempPath() % "/" % IMPORTDIR % "-XXXXXX");
    if (!tmp->isValid())
    {
        delete tmp;
        return "";
    }

    QString time(QDateTime::currentDateTime().toString("mm:ss"));
    OpenDevice("Import " + time, tmp->path(), nullptr, tmp);
    return tmp->path();
}


// Must define the valid template implementations to generate code for the
// instantiations (as they are defined in the cpp rather than header).
// Otherwise the linker will fail with undefined references...
template class ImageDb<ImageAdapterSg>;
template class ImageDb<ImageAdapterLocal>;
template class ImageHandler<ImageDbSg>;
template class ImageHandler<ImageDbLocal>;
