//! \file
//! \brief Manages a collection of images
//! \details Provides a generic Gallery comprising;
//! a database API for reading images,
//! a scanner to synchronise the database to a filesystem
//! a thumbnail generator to manage thumbnails for each database image
//! handlers for image operations

/* Myth uses two Gallery instances;
 *
 * One runs on the BE to manage the 'Photographs' Storage Group. The (permanent)
 * database table is synchronised to SG files, thumbnails are generated and stored
 * in the Temp SG, and BE messages for these images are handled.
 *
 * A separate instance runs on each FE to manage local/removeable media. This uses
 * a temporary, memory Db table that is synchronised to images on USB/CDs etc,
 * as they are mounted. Thumbnails are generated/stored on the FE and operations on
 * these images are handled locally. The Db table and all thumbnails are removed
 * when the FE exits.
 *
 * The UI integrates images/functions from both instances seamlessly.
 *
 * Commonality is provided by using an adapter layer for database and filesystem access.
 * Functionality is segregated into local classes, which are layered on an
 * adapter to assemble singletons for a BE manager & FE manager - the only
 * elements intended for external use.
 *
 *                              Device manager
 *                                    |
 *                              Common Adapter
 *                              /             \
 *                  BE adapter                  FE adapter
 *                       |      Common Db API       |
 *                       |      /             \     |
 *                  BE Db functions             FE Db functions
 *                       |      Common Handler      |
 *                       |      /             \     |
 *                  BE manager                  UI Db API
 *                                                  |
 *                                              FE manager
 *
 * Implemented using templates rather than polymorphism for speed/efficiency.
 */

#ifndef IMAGEMANAGER_H
#define IMAGEMANAGER_H

#include <utility>

// Qt headers
#include <QTemporaryDir>

// MythTV headers
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdirs.h"
#include "libmythbase/storagegroup.h"

#include "imagescanner.h"
#include "imagemetadata.h"


// Builtin storage groups as per storagegroup.cpp
static constexpr const char* IMAGE_STORAGE_GROUP      { "Photographs" };
static constexpr const char* THUMBNAIL_STORAGE_GROUP  { "Temp" };

// Filesystem dir within config dir used by TEMP SG
static constexpr const char* TEMP_SUBDIR              { "tmp" };
// Filesystem dir within tmp config dir where thumbnails reside
static constexpr const char* THUMBNAIL_SUBDIR         { "Images" };

static constexpr int DEVICE_INVALID { -1 };

class MythMediaDevice;
class MythMediaEvent;

//! Display filter
enum ImageDisplayType : std::uint8_t {
    kPicAndVideo = 0, //!< Show Pictures & Videos
    kPicOnly     = 1, //!< Hide videos
    kVideoOnly   = 2  //!< Hide pictures
};

class Device;

//! Manages image sources, ie. SG, local media (USBs, CDs etc), import directories.
//! A single SG device is opened on first use & never closed (no cleanup) .
//! Imports are created by user & deleted when 'ejected' or FE exits, which cleans
//! up thumbnails and imported images.
//! Gallery scans local devices (MediaMonitor) whenever the UI starts.
//! A local device is opened when first detected, closed when Gallery UI exits
//! (to allow unmounting) but persists for subsequent Gallery use.
//! It and its thumbnails are deleted by FE exit, user 'eject', system unmount
//! or if it is no longer present when Gallery re-starts
class META_PUBLIC DeviceManager
{
public:
    QStringList CloseDevices(int devId, const QString &action);
    QString DeviceMount(int devId) const;
    QString DeviceName(int devId) const;
    int     DeviceCount() const   { return m_devices.size(); }
    QString     ThumbDir(int fs) const;

protected:
    int     OpenDevice(const QString &name, const QString &mount,
                       MythMediaDevice *media = nullptr,
                       QTemporaryDir   *dir = nullptr);

    int         LocateMount(const QString &mount) const;
    StringMap   GetDeviceDirs() const;
    QList<int>  GetAbsentees();

    DeviceManager() = default;
    ~DeviceManager();

private:
    using DeviceMap = QMap<int, Device*>;

    //! Device store
    DeviceMap m_devices;
};


//! Common filesystem facilities
class META_PUBLIC ImageAdapterBase : public DeviceManager
{
public:
    static QStringList SupportedImages();

    static QStringList SupportedVideos();

    //! Assembles a canonical file path without corrupting its absolute/relative nature.
    static QString ConstructPath(const QString &path, const QString &name)
    {  return path.isEmpty() ? name : path + "/" + name; }

    //! Extracts file name (incl extension) from a filepath
    static QString BaseNameOf(const QString &path)
    { QString result = path.section('/', -1); return result.isNull() ? "" : result; }

    //! Extracts path from a filepath
    static QString PathOf(const QString &path)
    { QString result = path.section('/', 0, -2); return result.isNull() ? "" : result; }

    static QString FormatSize(int sizeKib)
    {  return (sizeKib < 10000) ? QString("%L1 KiB").arg(sizeKib)
                                : QString("%L1 MiB").arg(sizeKib / 1024.0, 0, 'f', 1); }

    //! Get absolute filepath for thumbnail of an image
    static QString GetAbsThumbPath(const QString &devPath, const QString &path)
    {
        QString dirFmt = QString("%1/") % TEMP_SUBDIR % "/%2/%3";
        return dirFmt.arg(GetConfDir(), devPath, path);
    }

    //! Thumbnails of videos are a JPEG snapshot with jpg suffix appended
    static QString ThumbPath(const ImageItem &im)
    { return im.m_type != kVideoFile ? im.m_filePath : im.m_filePath + ".jpg"; }

    /*!
     * \brief Get filters for detecting recognised images/videos
     * \details Supported pictures are determined by QImage; supported videos
     * are determined from the mythplayer settings (Video Associations)
     * \sa http://qt-project.org/doc/qt-4.8/qimagereader.html#supportedImageFormats
     * \return QDir A QDir initialised with filters for detecting images/videos
     */
    QDir GetImageFilters() const { return m_dirFilter; }

    //! Determine file type from its extension
    ImageNodeType GetImageType(const QString &ext) const
    {
        if (m_imageFileExt.contains(ext))
            return kImageFile;
        if (m_videoFileExt.contains(ext))
            return kVideoFile;
        return kUnknown;
    }
protected:
    ImageAdapterBase();
    virtual ~ImageAdapterBase() = default;

private:
    //! A pre-configured dir for reading image/video files
    QDir        m_dirFilter;
    //! List of file extensions recognised as pictures
    QStringList m_imageFileExt;
    //! List of file extensions recognised as videos
    QStringList m_videoFileExt;
};


/*!
 \brief Filesystem adapter for Frontend, managing local devices iaw MediaMonitor
 \details Scanner will scan these devices.
 Creates images with negative ids and absolute filepaths
 Thumbnails are stored locally and deleted when FE exits.
*/
class ImageAdapterLocal : public ImageAdapterBase
{
public:
    ImageAdapterLocal() = default;

    ImageItem *CreateItem(const QFileInfo &fi, int parentId, int devId,
                          const QString &base) const;

    //! Returns local device dirs to scan
    StringMap GetScanDirs() const  { return GetDeviceDirs(); }

    //! Get absolute filepath for a local image
    static QString GetAbsFilePath(const ImagePtrK &im)
    { return im->m_filePath; }

    //! Construct URL of a local image, which is an absolute path
    static QString MakeFileUrl(const QString &path) { return path; }

    //! Construct URL of the thumbnail of a local image (An absolute path)
    static QString MakeThumbUrl(const QString &devPath, const QString &path = "")
    { return GetAbsThumbPath(devPath, path); }

    static void Notify(const QString &mesg, const QStringList &extra) ;

protected:
    // Adapter functions used by Database for local images. Negate ids in & out
    static int     ImageId(int id)            { return ImageItem::ToLocalId(id); }
    static QString ImageId(const QString &id) { return ImageItem::ToLocalId(id); }
    static int     DbId(int id)               { return ImageItem::ToDbId(id); }
    static QString DbIds(const QString &ids)  { return ImageItem::ToDbId(ids); }
};


/*!
 \brief Filesystem adapter for Backend, managing Photographs storage group.
 \details Scanner will scan SG. Creates images with positive ids and relative filepaths
 Thumbnails are stored in Temp SG.
*/
class ImageAdapterSg : public ImageAdapterBase
{
public:
    ImageAdapterSg() :
        m_hostname(gCoreContext->GetMasterHostName()),
        m_hostport(MythCoreContext::GetMasterServerPort()),
        m_sg(StorageGroup(IMAGE_STORAGE_GROUP, m_hostname, false)) {}

    ImageItem *CreateItem(const QFileInfo &fi, int parentId, int devId,
                          const QString &base) const;
    StringMap  GetScanDirs() const;
    QString    GetAbsFilePath(const ImagePtrK &im) const;

    //! Construct URL of a remote image.
    QString MakeFileUrl(const QString &path) const
    { return MythCoreContext::GenMythURL(m_hostname, m_hostport, path,
                                      IMAGE_STORAGE_GROUP); }

    //! Construct URL of the thumbnail of a remote image
    QString MakeThumbUrl(const QString &devPath, const QString &path = "") const
    { return MythCoreContext::GenMythURL(m_hostname, m_hostport,
                                      devPath + "/" + path,
                                      THUMBNAIL_STORAGE_GROUP); }

    static void Notify(const QString &mesg, const QStringList &extra) ;

protected:
    // Adapter functions used by Database for remote images. Do nothing
    static int     ImageId(int id)            { return id; }
    static QString ImageId(const QString &id) { return id; }
    static int     DbId(int id)               { return id; }
    static QString DbIds(const QString &ids)  { return ids; }

private:
    //! Host of SG
    QString m_hostname;
    int m_hostport;
    //! Images storage group.
    // Marked mutable as storagegroup.h does not enforce const-correctness
    mutable StorageGroup m_sg;
};


//! Database API. Requires a filesystem adapter
//! Only handles a single id format, ie. local ids or remote ids
//! Provides ordering & filters (SQL does the work)
template <class FS>
class META_PUBLIC ImageDb : public FS
{
public:
    // Handler support
    int         GetImages(const QString &ids, ImageList &files, ImageList &dirs,
                          const QString &refine = "") const;
    bool        GetDescendants(const QString &ids,
                               ImageList &files, ImageList &dirs) const;
    int         InsertDbImage(ImageItemK &im, bool checkForDuplicate = false) const;
    bool        UpdateDbImage(ImageItemK &im) const;
    QStringList RemoveFromDB(const ImageList &imList) const;

    bool SetHidden(bool hide, const QString &ids) const;
    bool SetCover(int dir, int id) const;
    bool SetOrientation(int id, int orientation) const;

    // Scanner support
    bool ReadAllImages(ImageHash &files, ImageHash &dirs) const;
    void ClearDb(int devId, const QString &action);

    // ImageReader support
    int  GetChildren(const QString &ids, ImageList &files, ImageList &dirs,
                     const QString &refine = "") const;
    bool GetImageTree(int id, ImageList &files, const QString &refine) const;
    int  GetDirectory(int id, ImagePtr &parent, ImageList &files, ImageList &dirs,
                      const QString &refine) const;
    void GetDescendantCount(int id, bool all, int &dirs, int &pics,
                            int &videos, int &sizeKb) const;

protected:
    explicit ImageDb(QString table) : FS(), m_table(std::move(table)) {}

    ImageItem *CreateImage(const MSqlQuery &query) const;
    int        ReadImages(ImageList &dirs, ImageList &files,
                          const QString &selector) const;
    //! Db table name
    QString m_table;
};


//! A Database API with SG adapter for remote images
class META_PUBLIC ImageDbSg : public ImageDb<ImageAdapterSg>
{
public:
    ImageDbSg();
};


//! A Database with device adapter for local images
class META_PUBLIC ImageDbLocal : public ImageDb<ImageAdapterLocal>
{
protected:
    ImageDbLocal();
    ~ImageDbLocal() override          { DropTable(); }
    bool CreateTable();
    bool m_dbExists { false };

private:
    void DropTable();
};


//! A handler for image operations. Requires a database/filesystem adapter
template <class DBFS>
class META_PUBLIC ImageHandler : protected DBFS
{
public:
    QStringList HandleRename(const QString &id, const QString &newBase) const;
    QStringList HandleDelete(const QString &ids) const;
    QStringList HandleDbCreate(QStringList defs) const;
    QStringList HandleDbMove(const QString &ids, const QString &srcPath, QString destPath) const;
    QStringList HandleHide(bool hide, const QString &ids) const;
    QStringList HandleTransform(int transform, const QString &ids) const;
    QStringList HandleDirs(const QString &destId, bool rescan,
                           const QStringList &relPaths) const;
    QStringList HandleCover(int dir, int cover) const;
    QStringList HandleIgnore(const QString &exclusions) const;
    QStringList HandleScanRequest(const QString &command, int devId = DEVICE_INVALID) const;
    QStringList HandleCreateThumbnails(const QStringList &message) const;
    QStringList HandleGetMetadata(const QString &id) const;

protected:
    ImageHandler() : DBFS(),
        m_thumbGen(new ImageThumb<DBFS>(this)),
        m_scanner(new ImageScanThread<DBFS>(this, m_thumbGen)) {}

    ~ImageHandler() override
    {
        delete m_scanner;
        delete m_thumbGen;
    }

    void RemoveFiles(ImageList &images) const;

    ImageThumb<DBFS>      *m_thumbGen {nullptr}; //!< Thumbnail generator
    ImageScanThread<DBFS> *m_scanner  {nullptr}; //!< File scanner
};


/*!
 \brief The image manager to be used by the Backend
 \details A singleton created on first use and deleted when BE exits.
 Manages all storage group (remote) images
*/
class META_PUBLIC ImageManagerBe : protected QObject, public ImageHandler<ImageDbSg>
{
public:
    static ImageManagerBe* getInstance();

protected:
    ImageManagerBe()
    {
        // Cleanup & terminate child threads before application exits
        connect(qApp, &QCoreApplication::aboutToQuit, this, &QObject::deleteLater);
    }

    //! BE Gallery instance
    static ImageManagerBe *s_instance;
};


/*!
 \brief Provides read access to local & remote images
 \details Also manages UI filters (show hidden, show pics/vids) and UI ordering.
 All filtering and sorting is performed by SQL as images are retrieved.
*/
class META_PUBLIC ImageDbReader : protected ImageHandler<ImageDbLocal>
{
public:
    ~ImageDbReader() override { delete m_remote; }

    int  GetType() const        { return m_showType; }
    bool GetVisibility() const  { return m_showHidden; }

    void SetType(int showType)
    { m_showType = showType; SetRefinementClause(); }

    void SetSortOrder(int order, int dirOrder)
    { m_dirOrder = dirOrder; m_fileOrder = order; SetRefinementClause(); }

    void SetVisibility(bool showHidden)
    { m_showHidden = showHidden; SetRefinementClause(); }

    int  GetImages(const ImageIdList& ids, ImageList &files, ImageList &dirs) const;
    int  GetChildren(int id, ImageList &files, ImageList &dirs) const;
    int  GetDirectory(int id, ImagePtr &parent,
                      ImageList &files, ImageList &dirs) const;
    void GetDescendants(const ImageIdList &ids,
                        ImageList &files, ImageList &dirs) const;
    void GetImageTree(int id, ImageList &files) const;
    void GetDescendantCount(int id, int &dirs, int &pics, int &videos,
                            int &sizeKb) const;
protected:
    ImageDbReader(int order, int dirOrder, bool showAll, int showType)
        : m_remote(new ImageDbSg()),
          m_dirOrder(dirOrder),  m_fileOrder(order),
          m_showHidden(showAll), m_showType(showType)
    { SetRefinementClause(); }

    void SetRefinementClause();

    static QString TypeSelector(int type);
    static QString OrderSelector(int order);

    ImageDbSg* m_remote;    //!< Remote database access

    int     m_dirOrder;     //!< Display ordering of dirs
    int     m_fileOrder;    //!< Display ordering of pics/videos
    bool    m_showHidden;   //!< Whether hidden images are displayed
    int     m_showType;     //!< Type of images to display - pic only/video only/both
    QString m_refineClause; //!< SQL clause for image filtering/ordering
};


/*!
 \brief The image manager for use by Frontends
 \details A singleton created on first use and deleted when FE exits.
 As sole API for UI, it handles local & remote images. Actions for local images
 are processed internally; actions for remote images are delegated to the Backend.
 Incorporates a remote Db adapter for read-only access to remote images.
 Manages all local images (on removeable media) locally.
*/
class META_PUBLIC ImageManagerFe : protected QObject, public ImageDbReader
{
    Q_DECLARE_TR_FUNCTIONS(ImageManagerFe);
public:
    static ImageManagerFe &getInstance();

    // UI actions on all images
    void        CreateThumbnails(const ImageIdList &ids, bool forFolder);
    QString     ScanImagesAction(bool start, bool local = false);
    static QStringList ScanQuery();
    QString     HideFiles(bool hidden, const ImageIdList &ids);
    QString     ChangeOrientation(ImageFileTransform transform, const ImageIdList &ids);
    QString     SetCover(int parent, int cover);
    void        RequestMetaData(int id);
    static QString     IgnoreDirs(const QString &excludes);
    QString     MakeDir(int parent, const QStringList &names, bool rescan = true);
    QString     RenameFile(const ImagePtrK& im, const QString &name);
    QString     CreateImages(int destId, const ImageListK &images);
    QString     MoveDbImages(const ImagePtrK& destDir, ImageListK &images, const QString &srcPath);
    QString     DeleteFiles(const ImageIdList &ids);
    static void ClearStorageGroup();
    void        CloseDevices(int devId = DEVICE_INVALID, bool eject = false);

    using ImageAdapterBase::ConstructPath;

    // Local Device management
    bool    DetectLocalDevices();
    void    DeviceEvent(MythMediaEvent *event);
    QString CreateImport();
    using  DeviceManager::DeviceCount;
    using  DeviceManager::OpenDevice;

    // UI helper functions
    void SetDateFormat(const QString &format)    { m_dateFormat = format; }
    static QString LongDateOf(const ImagePtrK &im) ;
    QString ShortDateOf(const ImagePtrK &im) const;
    QString DeviceCaption(ImageItemK &im) const;
    QString CrumbName(ImageItemK &im, bool getPath = false) const;

    //! Generate Myth URL for a local or remote path
    QString BuildTransferUrl(const QString &path, bool local) const
    { return local ? ImageDbReader::MakeFileUrl(path)
                   : m_remote->MakeFileUrl(path); }

protected:
    ImageManagerFe(int order, int dirOrder, bool showAll, int showType,
                   QString dateFormat)
        : ImageDbReader(order, dirOrder, showAll, showType),
          m_dateFormat(std::move(dateFormat))
    {
        // Cleanup & terminate child threads before application exits
        connect(qApp, &QCoreApplication::aboutToQuit, this, &QObject::deleteLater);
    }

    //! FE Gallery instance
    static ImageManagerFe *s_instance;

    //! UI format for thumbnail date captions
    QString    m_dateFormat;
};


#endif // IMAGEMANAGER_H
