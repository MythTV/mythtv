//! \file
//! \brief Common types used by Gallery

#ifndef IMAGETYPES_H
#define IMAGETYPES_H

#include <QStringList>
#include <QSharedPointer>
#include <QPair>
#include <QList>
#include <QHash>
#include <QMap>
#include <QMetaType>

#include "libmythbase/mythchrono.h"
#include "mythmetaexp.h"

// Define this to log/count creation/deletion of ImageItem heap objects.
// These are created liberally when processing images and are liable to leak.
// At idle there should be precisely 1 ImageItem for every image/dir displayed.
//#define MEMORY_DEBUG active
#ifdef MEMORY_DEBUG
#include <QDebug>
#endif

// Id of the (virtual) Gallery root node
static constexpr int GALLERY_DB_ID { 0 };
// Id of the Storage Group (Photographs) node
static constexpr int PHOTO_DB_ID { 1 };


//! Type of image node
// We need to use other names to avoid getting coflicts with the videolist.h file
enum ImageNodeType : std::uint8_t {
    kUnknown   = 0, //!< Unprocessable file type
    kDevice    = 1, //!< Storage Group and local mounted media
    kCloneDir  = 2, //!< A device sub dir comprised from multiple SG dirs
    kDirectory = 3, //!< A device sub directory
    kImageFile = 4, //!< A picture
    kVideoFile = 5  //!< A video
};


//! Image ordering
enum ImageSortOrder : std::uint8_t {
    kSortByNameAsc     = 1, //!< Name A-Z
    kSortByNameDesc    = 2, //!< Name Z-A
    kSortByModTimeAsc  = 3, //!< File modified time Earliest -> Latest
    kSortByModTimeDesc = 4, //!< File modified time Latest -> Earliest
    kSortByExtAsc      = 5, //!< Extension A-Z
    kSortByExtDesc     = 6, //!< Extension Z-A
    kSortBySizeAsc     = 7, //!< File size Smallest -> Largest
    kSortBySizeDesc    = 8, //!< File size Largest -> Smallest
    kSortByDateAsc     = 9, //!< Exif date Earliest -> Latest
    kSortByDateDesc    = 10 //!< Exif date Latest -> Earliest
};


// Convenience types
using ImageIdList = QList<int>;
using StringPair  = QPair<QString, QString>;
using NameHash    = QHash<QString, QString>;
using StringMap   = QMap<int, QString>;
using ThumbPair   = QPair<int, QString>;


//! Represents a picture, video or directory
class META_PUBLIC ImageItem
{
public:
    explicit ImageItem(int id = 0)
        : m_id(id)
#ifndef MEMORY_DEBUG
    {}
#else
    { qDebug() << QString("Created %1 (%2)").arg(m_id).arg(++GetItemCount()); }

    ~ImageItem()
    { qDebug() << QString("Deleted %1 (%2)").arg(m_id).arg(--GetItemCount()); }

    // Embeds a static var in header to avoid the need for a cpp
    static int& GetItemCount() { static int count; return count; }
#endif


    //! Uniquely identifies an image (file/dir). Assigned by Db auto-incremnent
    //! Remote (Storage Group) images are positive - as in Db.
    //! Local images are negative - the negated Db id
    int              m_id;

    // Db File attributes
    QString          m_baseName;    //!< File/Dir name with extension (no path)
    QString          m_filePath;    //!< Absolute for local images. Usually SG-relative for remotes
    QString          m_extension;   //!< Image file extension
    int              m_device      { 0 }; //!< Id of media device. Always 0 (SG) for remotes, 1+ for local devices
    int              m_parentId    { 0 }; //!< Id of parent dir
    int              m_type        { 0 }; //!< Type of node: dir, video etc
    std::chrono::seconds m_modTime { 0s }; //!< Filesystem modified datestamp
    int              m_size        { 0 }; //!< Filesize (files only)
    std::chrono::seconds m_date    { 0s }; //!< Image creation date, from Exif metadata
    int              m_orientation { 0 }; //!< Image orientation
    QString          m_comment;     //!< User comment, from Exif metadata

    // Db User attributes
    bool             m_isHidden      { false }; //!< If true, image won't be shown
    int              m_userThumbnail {     0 }; //!< Id of thumbnail to use as cover (dirs only)

    // Derived attributes
    QString          m_url;        //! Myth URL of image (abs filepath for locals)
    QString          m_thumbPath;  //!< Absolute path of thumbnail
    QList<ThumbPair> m_thumbNails; //! Id & URLs of thumbnail(s). 1 for a file, 4 for dirs
    int              m_dirCount  { 0 }; //!< Number of child sub-dirs (dirs only)
    int              m_fileCount { 0 }; //!< Number of child images (dirs only)

    // Convenience functions
    bool IsDevice()     const { return m_type == kDevice; }
    bool IsDirectory()  const { return m_type <= kDirectory; }
    bool IsFile()       const { return m_type >  kDirectory; }
    bool IsLocal()      const { return IsLocalId(m_id); }

    //! Determine image type (local/remote) from its id. Root/Gallery is remote
    static bool    IsLocalId(int id)     { return id < GALLERY_DB_ID; }
    //! Parents of locals are locals or root
    static bool    IsLocalParent(int id) { return id <= GALLERY_DB_ID; }

    //! Converts a DB id (positive) to an id of a local image (negative)
    static int     ToLocalId(int id)     { return -id; }
    static QString ToLocalId(const QString &id) { return "-" + id; }

    //! Converts local image ids (negative) to Db ids (positive)
    static int     ToDbId(int id)        { return abs(id); }
    static QString ToDbId(QString ids)   { return ids.remove("-"); }


    /*!
     \brief Separates list of ids into a list of local ids and a list of remote ids
     \param ids List of local/remote image ids
     \return StringPair Pair <CSV of local ids, CSV of remote ids>
    */
    static StringPair PartitionIds(const ImageIdList &ids)
    {
        QStringList local;
        QStringList remote;
        for (int id : std::as_const(ids))
        {
            if (ImageItem::IsLocalId(id))
                local << QString::number(id);
            else
                remote << QString::number(id);
        }
        return qMakePair(local.join(","), remote.join(","));
    }

private:
    Q_DISABLE_COPY(ImageItem)
};

// Convenience containers
using ImagePtr  = QSharedPointer<ImageItem>;
using ImageList = QVector<ImagePtr>;
using ImageHash = QHash<QString, ImagePtr>;

// Read-only images alias
using ImageItemK = const ImageItem;
using ImagePtrK  = QSharedPointer<ImageItemK>;
using ImageListK = QList<ImagePtrK>;

Q_DECLARE_METATYPE(ImagePtrK)

#endif // IMAGETYPES_H
