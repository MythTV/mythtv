//! \file
//! \brief Provides access to database and storage group files for images
//! \details Encapsulates (most) database and storage group dependencies

#ifndef IMAGEUTILS_H
#define IMAGEUTILS_H

// Qt headers
#include <QDir>
#include <QMap>

// MythTV headers
#include <mythcontext.h>
#include <mythdbcon.h>
#include <mythmetaexp.h>
#include <storagegroup.h>


// Builtin storage groups
#define IMAGE_STORAGE_GROUP         "Photographs"
#define THUMBNAIL_STORAGE_GROUP     "Temp"
// Filesystem dir used by TEMP SG
#define TEMP_DIR                    "tmp"
// Subdir within BE Myth tmp to be used for thumbnails
#define THUMBNAIL_DIR               IMAGE_STORAGE_GROUP

// Id of the root node representing the Storage Group
#define ROOT_DB_ID 1

//! Image ordering
enum ImageSortOrder {
    kSortByNone        = 0, //!< Undefined order
    kSortByNameAsc     = 1, //!< Name A-Z
    kSortByNameDesc    = 2, //!< Name Z-A
    kSortByModTimeAsc  = 3, //!< File modified time Earliest - Latest
    kSortByModTimeDesc = 4, //!< File modified time Latest - Earliest
    kSortByExtAsc      = 5, //!< Extension A-Z
    kSortByExtDesc     = 6, //!< Extension Z-A
    kSortBySizeAsc     = 7, //!< File size Smallest - Largest
    kSortBySizeDesc    = 8, //!< File size Largest - Smallest
    kSortByDateAsc     = 9, //!< Exif date Earliest - Latest
    kSortByDateDesc    = 10 //!< Exif date Latest - Earliest
};

//! Type of image to display
enum ImageDisplayType {
    kPicAndVideo = 0, //!< Show Pictures & Videos
    kPicOnly     = 1, //!< Hide videos
    kVideoOnly   = 2  //!< Hide pictures
};


//! Type of image node
// We need to use other names to avoid getting coflicts with the videolist.h file
enum ImageNodeType {
    kUnknown        = 0, //!< Shouldn't occur
    kBaseDirectory  = 1, //!< "Gallery" root node of all images
    kUpDirectory    = 2, //!< Dir/Parent currently viewed
    kSubDirectory   = 3, //!< Child sub-dirs
    kImageFile      = 4, //!< A picture
    kVideoFile      = 5  //!< A video
};


//! Represents a picture, video or directory
class META_PUBLIC ImageItem
{
public:
    ImageItem();
    ImageItem(const ImageItem &);

    // Db key
    int         m_id;            //!< Uniquely identifies an image or dir. Assigned by db

    // Db File attributes
    QString     m_name;          //!< File/Dir name (no path)
    QString     m_path;          //!< Path relative to storage group
    int         m_parentId;      //!< Id of parent dir
    int         m_type;          //!< Type of node
    int         m_modTime;       //!< Filesystem modified datestamp
    int         m_size;          //!< Filesize (images only)
    QString     m_extension;     //!< File extension (images only)
    uint32_t    m_date;          //!< Image creation date, from metadata
    int         m_orientation;   //!< Camera orientation, from metadata
    QString     m_comment;       //!< User comment, from metadata

    // Db User attributes
    bool        m_isHidden;      //!< If true, image won't be shown
    int         m_userThumbnail; //!< Id of an image/dir to use as thumbnail (dirs only)

    // Derived attributes
    QString     m_fileName;      //!< File path relative to storage group
    QString     m_thumbPath;     //!< Path of thumbnail, relative to BE cache
    QStringList m_thumbNails;    //!< BE URLs to use for thumbnails
    QList<int>  m_thumbIds;      //!< Image ids corresponding to above thumbnails
    int         m_dirCount;      //!< Number of child sub-dirs (dirs only)
    int         m_fileCount;     //!< Number of child images (dirs only)

    bool IsDirectory()  const { return m_type <= kSubDirectory; }
    bool IsFile()       const { return m_type >  kSubDirectory; }
};

Q_DECLARE_METATYPE(ImageItem*)


// Convenience containers
typedef QMap<QString, ImageItem *>  ImageMap;
typedef QList<ImageItem *>          ImageList;
typedef QList<int>                  ImageIdList;


//! General functions
class META_PUBLIC ImageUtils
{
public:

    static QString ThumbPathOf(ImageItem *);
    static QString ImageDateOf(ImageItem *);
};


//! Wrapped Images Storage Group providing filesystem access
class META_PUBLIC ImageSg
{
public:
    static ImageSg *getInstance();
    QDir            GetImageFilters();
    QString         GetFilePath(ImageItem*);
    QStringList     GetStorageDirs() { return m_sgImages.GetDirList(); }

    //! Generate URL of a thumbnail
    QString         GenerateThumbUrl(const QString &path)
    { return gCoreContext->GenMythURL(m_hostname, m_hostport, path, THUMBNAIL_STORAGE_GROUP);}

    //! Generate URL of an image
    QString         GenerateUrl(const QString &path)
    { return gCoreContext->GenMythURL(m_hostname, m_hostport, path, IMAGE_STORAGE_GROUP); }

    //! Determine type from an extension
    ImageNodeType   GetImageType(const QString &ext)
    {
        return m_imageFileExt.contains(ext)
                ? kImageFile
                : m_videoFileExt.contains(ext) ? kVideoFile : kUnknown;
    }

    bool MoveFiles(ImageList &, ImageItem *parent);
    void RemoveFiles(ImageList &);

    //! Images storage group
    StorageGroup       m_sgImages;

private:
    // A singleton as creating SGs is laborious
    ImageSg();
    static ImageSg *m_instance;
    QString         m_hostname, m_hostport;

    //! List of file extensions recognised as pictures
    QStringList    m_imageFileExt;
    //! List of file extensions recognised as videos
    QStringList    m_videoFileExt;

};


//! Provides read-only access to image database (for FE/clients).
//! Returned items are optionally filtered and sorted
class META_PUBLIC ImageDbReader
{
public:
    ImageDbReader(int order, bool showAll, int showType)
        : m_showHidden(showAll), m_showType(showType) { SetSortOrder(order); }

    void SetSortOrder(int order);
    int  GetSortOrder()                 { return m_order; }
    void SetVisibility(bool showHidden) { m_showHidden = showHidden; }
    bool GetVisibility()                { return m_showHidden; }
    void SetType(int showType)          { m_showType = showType; }
    int  GetType()                      { return m_showType; }

    int ReadDbItems(ImageList &images,
                    QString selector,
                    bool showAll = true,
                    bool ordered = false);

    /*!
     * \brief Read selected database images/dirs by id
     * \details Returns database items (mixed files/dirs) selected by id with options
     * for sort order and including currently hidden items.
     * \param[out] images List of images/dirs from Db
     * \param[in] ids Comma-separated list of image ids
     * \param[in] showAll If true, all items are extracted. Otherwise only items matching
     * the visibility filter are returned
     * \param[in] ordered If true, returned lists are ordered according to GallerySortOrder
     * setting. Otherwise they are in undefined (database) order.
     * \param[in] selector Db selection query clause
     * \return int Number of items matching query.
     */
    int ReadDbItemsById(ImageList &images,
                        const QString &ids,
                        bool showAll = true,
                        bool ordered = false,
                        const QString &selector = "TRUE")
    {
        QString idSelector = QString("file_id IN (%1) AND %2").arg(ids, selector);
        return ReadDbItems(images, idSelector, showAll, ordered);
    }

    /*!
     * \brief Read selected database images (no dirs) by id
     * \details Returns database images selected by id with options
     * for sort order and including currently hidden items.
     * \param[out] files List of images from Db
     * \param[in] ids Comma-separated list of image ids
     * \param[in] showAll If true, all items are extracted. Otherwise only items matching
     * the visibility filter are returned
     * \param[in] ordered If true, returned lists are ordered according to GallerySortOrder
     * setting. Otherwise they are in undefined (database) order.
     * \return int Number of items matching query.
     */
    int ReadDbFilesById(ImageList &files,
                        const QString &ids,
                        bool showAll = true,
                        bool ordered = false)
    { return ReadDbItemsById(files, ids, showAll, ordered, queryFiles[m_showType]); }

    /*!
     * \brief Read selected database dirs (no images) by id
     * \details Returns database dirs selected by id with options
     * for sort order and including currently hidden items.
     * \param[out] dirs List of dirs from Db
     * \param[in] ids Comma-separated list of image ids
     * \param[in] showAll If true, all items are extracted. Otherwise only items matching
     * the visibility filter are returned
     * \param[in] ordered If true, returned lists are ordered according to GallerySortOrder
     * setting. Otherwise they are in undefined (database) order.
     * \return int Number of items matching query.
     */
    int ReadDbDirsById(ImageList &dirs,
                       const QString &ids,
                       bool showAll = true,
                       bool ordered = false)
    { return ReadDbItemsById(dirs, ids, showAll, ordered, queryDirs); }

    /*!
     * \brief Read database images (no dirs) that are children of dirs
     * \details Returns database images that are direct children of specific dirs with
     * options for sort order and including currently hidden items.
     * \param[out] files List of images from Db
     * \param[in] ids Comma-separated list of parent dir ids
     * \param[in] showAll If true, all items are extracted. Otherwise only items matching
     * the visibility filter are returned
     * \param[in] ordered If true, returned lists are ordered according to GallerySortOrder
     * setting. Otherwise they are in undefined (database) order.
     * \return int Number of items matching query.
     */
    int ReadDbChildFiles(ImageList &files,
                         const QString &ids,
                         bool showAll = true,
                         bool ordered = false)
    {
        QString selector = QString("dir_id IN (%1) AND %2").arg(ids, queryFiles[m_showType]);
        return ReadDbItems(files, selector, showAll, ordered);
    }

    /*!
     * \brief Read database dirs (no images) that are children of dirs
     * \details Returns database dirs that are direct sub-directories of specific dirs with
     * options for sort order and including currently hidden items.
     * \param[out] dirs List of sub-dirs from Db
     * \param[in] ids Comma-separated list of parent dir ids
     * \param[in] showAll If true, all items are extracted. Otherwise only items matching
     * the visibility filter are returned
     * \param[in] ordered If true, returned lists are ordered according to GallerySortOrder
     * setting. Otherwise they are in undefined (database) order.
     * \return int Number of items matching query.
     */
    int ReadDbChildDirs(ImageList &dirs,
                        const QString &ids,
                        bool showAll = true,
                        bool ordered = false)
    {
        QString selector = QString("dir_id IN (%1) AND %2").arg(ids, queryDirs);
        return ReadDbItems(dirs, selector, showAll, ordered);
    }

    void ReadDbTree(ImageList &files,
                    ImageList &dirs,
                    QStringList ids,
                    bool showAll = true,
                    bool ordered = false);

protected:
    ImageItem *CreateImage(MSqlQuery &query);

    //! Db query clauses to distinguish between images & dirs
    static const QString queryDirs;
    static const QMap<int, QString> queryFiles;
    static QMap<int, QString> InitQueries();

    //! Filter for hidden files
    bool m_showHidden;
    //! Filter for pictures/videos
    int m_showType;
    //! Sort order for returned items
    int m_order;
    //! SQL clause for sort order
    QString m_orderSelector;
};


//! Provides read-write access to image database (for backends only).
//! Returned items are not filtered or sorted
class META_PUBLIC ImageDbWriter : private ImageDbReader
{
public:

    ImageDbWriter() : ImageDbReader(kSortByNone, true, kPicAndVideo) {}

    void ReadDbItems(ImageMap &files,
                     ImageMap &dirs,
                     const QString &selector = "TRUE");

    //! \sa ImageDbReader::ReadDbItems
    int ReadDbItems(ImageList &images, const QString &selector)
    { return ImageDbReader::ReadDbItems(images, selector, true, false); }

    //! \sa ImageDbReader::ReadDbItemsById
    int ReadDbItemsById(ImageList &images,
                        const QString &ids,
                        const QString &selector = "TRUE")
    { return ReadDbItems(images, QString("file_id IN (%1) AND %2").arg(ids, selector)); }

    //! \sa ImageDbReader::ReadDbFilesById
    int ReadDbFilesById(ImageList &files, const QString &ids)
    { return ReadDbItemsById(files, ids, queryFiles[kPicAndVideo]); }

    //! \sa ImageDbReader::ReadDbDirsById
    int ReadDbDirsById(ImageList &dirs, const QString &ids)
    { return ReadDbItemsById(dirs, ids, queryDirs); }

    //! \sa ImageDbReader::ReadDbChildFiles
    int ReadDbChildFiles(ImageList &files, const QString &ids)
    { return ImageDbReader::ReadDbChildFiles(files, ids, true, false); }

    //! \sa ImageDbReader::ReadDbChildDirs
    int ReadDbChildDirs(ImageList &dirs, const QString &ids)
    { return ImageDbReader::ReadDbChildDirs(dirs, ids, true, false); }

    //! \sa ImageDbReader::ReadDbTree
    void ReadDbTree(ImageList &files,
                    ImageList &dirs,
                    QStringList ids)
    { ImageDbReader::ReadDbTree(files, dirs, ids, true, false); }

    void        ClearDb();
    int         InsertDbDirectory(ImageItem &);
    bool        UpdateDbFile(ImageItem *);
    QStringList RemoveFromDB(const ImageList);
    bool        SetHidden(bool hide, QStringList &);
    void        SetCover(int dir, int id);
    void        SetOrientation(ImageItem *im);
};

#endif // IMAGEUTILS_H
