#include "imageutils.h"

#include <QByteArray>
#include <QMutableListIterator>
#include <QImageReader>

#include <dbaccess.h>
#include <mythdirs.h>


/**
 *  \brief  Constructor
 */
ImageItem::ImageItem() :
    m_id(0),
    m_name(""),
    m_path(""),
    m_parentId(0),
    m_type(0),
    m_modTime(0),
    m_size(0),
    m_extension(""),
    m_date(0),
    m_orientation(0),
    m_comment(""),
    m_isHidden(false),
    m_userThumbnail(0),
    m_fileName(""),
    m_thumbPath(""),
    m_dirCount(0),
    m_fileCount(0)
{
}


ImageItem::ImageItem(const ImageItem &im) :
    m_id(im.m_id),
    m_name(im.m_name),
    m_path(im.m_path),
    m_parentId(im.m_parentId),
    m_type(im.m_type),
    m_modTime(im.m_modTime),
    m_size(im.m_size),
    m_extension(im.m_extension),
    m_date(im.m_date),
    m_orientation(im.m_orientation),
    m_comment(im.m_comment),
    m_isHidden(im.m_isHidden),
    m_userThumbnail(im.m_userThumbnail),
    m_fileName(im.m_fileName),
    m_thumbPath(im.m_thumbPath),
    m_thumbNails(im.m_thumbNails),
    m_thumbIds(im.m_thumbIds),
    m_dirCount(im.m_dirCount),
    m_fileCount(im.m_fileCount)
{
}


/*!
 * \brief Get path for an image thumbnail
 * \param im The image
 * \return QString Thumbnail path
 */
QString ImageUtils::ThumbPathOf(ImageItem *im)
{
    // Thumbnails of videos are a JPEG snapshot with jpg suffix appended
    QString ext = im->m_type == kVideoFile ? ".jpg" : "";

    // Create the relative path and filename to the thumbnail image or thumbdir
    return QString("%1/%2%3").arg(THUMBNAIL_DIR, im->m_fileName, ext);
}


/*!
 \brief Return a timestamp/datestamp for an image or dir
 \details Uses exif timestamp if defined, otherwise file modified date
 \param im Image or dir
 \return QString Exif Timestamp text for images, file modified datestamp text
 for dirs and images with no exif
*/
QString ImageUtils::ImageDateOf(ImageItem *im)
{
    return im->m_date > 0
            ? QDateTime::fromTime_t(im->m_date)
              .toString(Qt::DefaultLocaleShortDate)
            : QDateTime::fromTime_t(im->m_modTime).date()
              .toString(Qt::DefaultLocaleShortDate);
}


/*!
 \brief Constructor
*/
ImageSg::ImageSg()
    : m_hostname(gCoreContext->GetMasterHostName()),
      m_hostport(gCoreContext->GetMasterServerPort())
{
    m_sgImages = StorageGroup(IMAGE_STORAGE_GROUP, m_hostname, false);
}


//! Storage Group singleton
ImageSg* ImageSg::m_instance = NULL;


/*!
 \brief Return singleton
 \return ImageSg Images SG object
*/
ImageSg* ImageSg::getInstance()
{
    if (!m_instance)
        m_instance = new ImageSg();
    return m_instance;
}


/*!
 * \brief Get filters for detecting recognised images/videos
 * \details Supported pictures are determined by QImage; supported videos
 * are determined from the mythplayer settings (Video Associations)
 * \sa http://qt-project.org/doc/qt-4.8/qimagereader.html#supportedImageFormats
 * \return QDir A QDir initialised with filters for detecting images/videos
 */
QDir ImageSg::GetImageFilters()
{
    QStringList glob;

    // Determine supported picture formats
    m_imageFileExt.clear();
    foreach (const QByteArray &ext, QImageReader::supportedImageFormats())
    {
        m_imageFileExt << QString(ext);
        glob << "*." + ext;
    }

    // Determine supported video formats
    m_videoFileExt.clear();
    const FileAssociations::association_list faList =
        FileAssociations::getFileAssociation().getList();
    for (FileAssociations::association_list::const_iterator p =
             faList.begin(); p != faList.end(); ++p)
    {
        if (!p->use_default && p->playcommand == "Internal")
        {
            m_videoFileExt << QString(p->extension);
            glob << "*." + p->extension;
        }
    }

    QDir dir;

    // Apply filters to only detect image files
    dir.setNameFilters(glob);
    dir.setFilter(QDir::AllDirs | QDir::Files | QDir::Readable |
                  QDir::NoDotAndDotDot | QDir::NoSymLinks);

    // Sync files before dirs to improve thumb generation response
    // Order by time (oldest first) - this determines the order thumbs appear
    dir.setSorting(QDir::DirsLast | QDir::Time | QDir::Reversed);

    return dir;
}


/*!
 * \brief Get paths for a list of images
 * \param images List of images
 * \return ImagePaths Map of image names & their paths
 */
QString ImageSg::GetFilePath(ImageItem *im)
{
    QString path = m_sgImages.FindFile(im->m_fileName);
    if (path.isEmpty())
        LOG(VB_FILE, LOG_NOTICE,
            QString("Image: File %1 not found in Storage Group %2")
            .arg(im->m_fileName).arg(IMAGE_STORAGE_GROUP));
    return path;
}


/*!
 \brief Moves images and dirs within the storage group (filesystem)
 \details Uses renaming. Files never move filesystems within the Storage Group
 \param images List of images/dirs to move
 \param parent New parent directory
 \return bool True if at least 1 file was moved
*/
bool ImageSg::MoveFiles(ImageList &images, ImageItem *parent)
{
    bool changed = false;
    foreach (const ImageItem * im, images)
    {
        // Get SG dir of this file
        QString sgDir = m_sgImages.FindFileDir(im->m_fileName);
        if (sgDir.isEmpty())
        {
            LOG(VB_FILE, LOG_NOTICE,
                QString("Image: File %1 not found in Storage Group %2")
                .arg(im->m_fileName).arg(IMAGE_STORAGE_GROUP));
            continue;
        }

        // Use existing fs & name with destination path
        QString oldPath = QString("%1/%2").arg(sgDir, im->m_fileName);
        QString newPath = QString("%1/%2/%3").arg(sgDir, parent->m_fileName, im->m_name);

        // Move file
        if (QFile::rename(oldPath, newPath))
        {
            changed = true;
            LOG(VB_FILE, LOG_DEBUG, QString("Image: Moved %1 -> %2")
                .arg(oldPath, newPath));
        }
        else
            LOG(VB_FILE, LOG_ERR, QString("Image: Failed to move %1 to %2")
                .arg(oldPath, newPath));
    }
    return changed;
}


/*!
 * \brief Deletes images and dirs from the storage group (filesystem)
 * \details Dirs will only be deleted if empty. Files/dirs that failed
 * to delete will be removed from the list.
 * \param[in,out] images List of images/dirs to delete. On return the files that
 * were successfully deleted.
 */
void ImageSg::RemoveFiles(ImageList &images)
{
    QMutableListIterator<ImageItem *> it(images);
    it.toBack();
    while (it.hasPrevious())
    {
        ImageItem *im = it.previous();

        // Find file
        QString absFilename = m_sgImages.FindFile(im->m_fileName);

        if (absFilename.isEmpty())
        {
            LOG(VB_FILE, LOG_ERR,
                QString("Image: File %1 not found in Storage Group %2")
                .arg(im->m_fileName).arg(IMAGE_STORAGE_GROUP));
            it.remove();
            delete im;
            continue;
        }

        // Remove file
        bool ok;
        if (im->IsFile())
            ok = QFile::remove(absFilename);
        else
        {
            QDir dir;
            ok = dir.rmdir(absFilename);
        }
        if (!ok)
        {
            LOG(VB_FILE, LOG_ERR, QString("Can't delete %1").arg(im->m_fileName));
            // Remove from list
            it.remove();
            delete im;
        }
    }
}


// Standard query to be parsed by CreateImage
const QString dbQuery =
    "SELECT "
    "file_id, name, path, dir_id, type, modtime, size, extension, "
    "date, hidden, orientation, angle, filename FROM gallery_files "
    "WHERE %1 %2 %3";


//! Initialise static constant
QMap<int, QString> ImageDbReader::InitQueries()
{
    QMap<int, QString> map;
    map.insert(kPicOnly,     QString("type = %1").arg(kImageFile));
    map.insert(kVideoOnly,   QString("type = %1").arg(kVideoFile));
    map.insert(kPicAndVideo, QString("type > %1").arg(kSubDirectory));
    return map;
}


// Db query clauses to distinguish between pictures, videos & dirs
const QMap<int, QString> ImageDbReader::queryFiles = ImageDbReader::InitQueries();
const QString ImageDbReader::queryDirs = QString("type <= %1").arg(kSubDirectory);


/*!
 * \brief Generate Db query clause for sort order
 * \return QString Db clause
 */
void ImageDbReader::SetSortOrder(int order)
{
    m_order = order;

    // prepare the sorting statement
    switch (order)
    {
        case kSortByNameAsc:
            m_orderSelector = "ORDER BY name ASC"; break;
        case kSortByNameDesc:
            m_orderSelector = "ORDER BY name DESC"; break;
        case kSortByModTimeAsc:
            m_orderSelector = "ORDER BY modtime ASC, name ASC"; break;
        case kSortByModTimeDesc:
            m_orderSelector = "ORDER BY modtime DESC, name ASC"; break;
        case kSortByExtAsc:
            m_orderSelector = "ORDER BY extension ASC, name ASC"; break;
        case kSortByExtDesc:
            m_orderSelector = "ORDER BY extension DESC, name ASC"; break;
        case kSortBySizeAsc:
            m_orderSelector = "ORDER BY size ASC, name ASC"; break;
        case kSortBySizeDesc:
            m_orderSelector = "ORDER BY size DESC, name ASC"; break;
        case kSortByDateAsc:
            m_orderSelector = "ORDER BY date ASC, name ASC"; break;
        case kSortByDateDesc:
            m_orderSelector = "ORDER BY date DESC, name ASC"; break;
        case kSortByNone:
        default:
            m_orderSelector = "";
    }
}


/*!
 * \brief Create image metadata
 * \param query Db query result
 * \return ImageItem An image
 */
ImageItem *ImageDbReader::CreateImage(MSqlQuery &query)
{
    ImageItem *im = new ImageItem();

    im->m_id            = query.value(0).toInt();
    im->m_name          = query.value(1).toString();
    im->m_path          = query.value(2).toString();
    im->m_parentId      = query.value(3).toInt();
    im->m_type          = query.value(4).toInt();
    im->m_modTime       = query.value(5).toInt();
    im->m_size          = query.value(6).toInt();
    im->m_extension     = query.value(7).toString();
    im->m_date          = query.value(8).toUInt();
    im->m_isHidden      = query.value(9).toInt();
    im->m_orientation   = query.value(10).toInt();
    im->m_userThumbnail = query.value(11).toInt();
    im->m_comment       = query.value(12).toString();
    im->m_fileName      =
        QDir::cleanPath(QDir(im->m_path).filePath(im->m_name));

    im->m_thumbPath = ImageUtils::ThumbPathOf(im);

    return im;
}


/*!
 * \brief Read database images/dirs as list
 * \details Get selected database items (mixed files/dirs) in prescribed order,
 * optionally including currently hidden items.
 * \param[out] images List of images/dirs from Db
 * \param[in] selector Db selection query clause
 * \param[in] showAll If true, all items are extracted. Otherwise only items matching
 * the visibility filter are returned
 * \param[in] ordered If true, returned lists are ordered according to GallerySortOrder
 * setting. Otherwise they are in undefined (database) order.
 * \return int Number of items matching query.
 */
int ImageDbReader::ReadDbItems(ImageList &images, QString selector,
                               bool showAll, bool ordered)
{
    QString   orderSelect  = ordered ? m_orderSelector : "";
    QString   hiddenSelect = showAll || m_showHidden ? "" : "AND hidden = 0";

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(dbQuery.arg(selector, hiddenSelect, orderSelect));
    if (!query.exec())
    {
        LOG(VB_GENERAL, LOG_ERR, MythDB::DBErrorMessage(query.lastError()));
        return 0;
    }

    while (query.next())
    {
        ImageItem *im = CreateImage(query);

        // Initialise image thumbnail
        if (im->IsFile())
        {
            im->m_thumbNails.append(im->m_thumbPath);
            im->m_thumbIds.append(im->m_id);
        }
        images.append(im);
    }
    return query.size();
}


/*!
 * \brief Read sub-tree of database images and dirs as lists
 * \details Returns database files and dirs contained in the sub-tree of
 * specific dirs. Optionally ordered and including currently hidden items.
 * \param[out] files List of images within sub-tree
 * \param[out] dirs List of dirs within sub-tree
 * \param[in] idList Comma-separated list of parent dir ids
 * \param[in] showAll If true, all items are extracted. Otherwise only items matching
 * the visibility filter are returned
 * \param[in] ordered If true, returned lists are ordered according to GallerySortOrder
 * setting. Otherwise they are in undefined (database) order.
 */
void ImageDbReader::ReadDbTree(ImageList &files,
                            ImageList &dirs,
                            QStringList idList,
                            bool showAll,
                            bool ordered)
{
    // Load starting files
    QString ids = idList.join(",");
    ReadDbFilesById(files, ids, showAll, ordered);
    ReadDbDirsById(dirs, ids, showAll, ordered);

    // Add all descendants
    ImageList subdirs;
    while (!idList.isEmpty())
    {
        QString selector =
            QString("dir_id IN (%1) AND %2").arg(idList.join(","));
        ReadDbItems(files, selector.arg(queryFiles[m_showType]), showAll, ordered);
        ReadDbItems(subdirs, selector.arg(queryDirs), showAll, ordered);
        dirs += subdirs;
        idList.clear();
        foreach (const ImageItem * im, subdirs)
        {
            if (im->IsDirectory())
                idList.append(QString::number(im->m_id));
        }
        subdirs.clear();
    }
}


/*!
 * \brief Read database images and dirs as map
 * \details Returns selected database items, separated into files and dirs. Results contain
 * no thumbnails (paths nor urls) and are mapped to item name and thus unordered.
 * \param[out] files Map of image names and metadata from Db
 * \param[out] dirs Map of dir names and metadata from Db
 * \param[in] selector Db selection query
 */
void ImageDbWriter::ReadDbItems(ImageMap &files, ImageMap &dirs,
                                const QString &selector)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(dbQuery.arg(selector, "", ""));

    if (!query.exec())
    {
        LOG(VB_GENERAL, LOG_ERR, MythDB::DBErrorMessage(query.lastError()));
        return;
    }

    while (query.next())
    {
        ImageItem *im = CreateImage(query);

        if (im->IsDirectory())
            dirs.insert(im->m_fileName, im);
        else
            files.insert(im->m_fileName, im);
    }
}


/*!
 * \brief Clear image database
 * \note Only backends should modify the database
 */
void ImageDbWriter::ClearDb()
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(QString("TRUNCATE gallery_files;"));

    if (!query.exec())
        LOG(VB_GENERAL, LOG_ERR, MythDB::DBErrorMessage(query.lastError()));
}


/*!
 * \brief Adds new dir to database.
 * \details Dir should not already exist.
 * \param im Image data for dir
 * \return int Database id for new dir
 * \note Only backends should modify the database
 */
int ImageDbWriter::InsertDbDirectory(ImageItem &im)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("INSERT INTO gallery_files ("
                  "name, path, dir_id, type, modtime, hidden "
                  ") VALUES ("
                  ":NAME, :PATH, :PARENT_ID, :TYPE, :MODTIME, :HIDDEN);");
    query.bindValue(":NAME", im.m_name);
    query.bindValue(":PATH", im.m_path);
    query.bindValue(":PARENT_ID", im.m_parentId);
    query.bindValue(":TYPE", im.m_type);
    query.bindValue(":MODTIME", im.m_modTime);
    query.bindValue(":HIDDEN", im.m_isHidden);

    if (!query.exec())
        MythDB::DBError("Error inserting, query: ", query);

    return query.lastInsertId().toInt();
}


/*!
 * \brief Updates or creates database image or dir
 * \details Item does not need to pre-exist
 * \sa ImageUtils::InsertDbDirectory
 * \param im Image or dir
 * \return bool False if db update failed
 * \note Only backends should modify the database
 */
bool ImageDbWriter::UpdateDbFile(ImageItem *im)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(QString("REPLACE INTO gallery_files ("
                          "file_id, name, path, dir_id, type, "
                          "modtime, size, extension, date, orientation, filename) "
                          "VALUES ("
                          ":ID, :NAME, :PATH, :DIR, :TYPE, :MODTIME, "
                          ":SIZE, :EXT, :DATE, :ORIENT, :COMMENT )"));
    query.bindValue(":ID", im->m_id);
    query.bindValue(":NAME", im->m_name);
    query.bindValue(":PATH", im->m_path.isNull() ? "" : im->m_path);
    query.bindValue(":DIR", im->m_parentId);
    query.bindValue(":TYPE", im->m_type);
    query.bindValue(":MODTIME", im->m_modTime);
    query.bindValue(":SIZE", im->m_size);
    query.bindValue(":EXT", im->m_extension);
    query.bindValue(":DATE", im->m_date);
    query.bindValue(":ORIENT", im->m_orientation);
    query.bindValue(":COMMENT", im->m_comment.isNull() ? "" : im->m_comment);
    // hidden & user thumb will be preserved for existing files
    // & initialised using db defaults (0/false/not set) for new files

    bool ok = query.exec();
    if (!ok)
        MythDB::DBError("Error updating, query: ", query);
    return ok;
}


/*!
 * \brief Remove images/dirs from database
 * \details Item does not need to exist in db
 * \param imList List of items to delete
 * \return QStringList List of item ids that were successfully removed
 * \note Only backends should modify the database
 */
QStringList ImageDbWriter::RemoveFromDB(const ImageList imList)
{
    QStringList ids;
    if (!imList.isEmpty())
    {
        foreach (const ImageItem * im, imList)
            ids << QString::number(im->m_id);

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare(QString("DELETE IGNORE FROM gallery_files "
                              "WHERE file_id IN (%2);")
                      .arg(ids.join(",")));

        if (!query.exec())
        {
            MythDB::DBError("Error deleting, query: ", query);
            return QStringList();
        }
    }
    return ids;
}


/*!
 * \brief Sets hidden status of an image/dir in database
 * \param hide True = hidden, False = unhidden
 * \param ids List of item ids
 * \return bool False if db update failed
 * \note Only backends should modify the database
 */
bool ImageDbWriter::SetHidden(bool hide, QStringList &ids)
{
    MSqlQuery query(MSqlQuery::InitCon());

    if (!ids.isEmpty())
    {
        query.prepare(QString("UPDATE gallery_files SET "
                              "hidden = :HIDDEN "
                              "WHERE file_id IN (%1);").arg(ids.join(",")));
        query.bindValue(":HIDDEN", hide ? 1 : 0);
\
        if (!query.exec())
        {
            MythDB::DBError("Error updating, query: ", query);
            return false;
        }
    }
    return true;
}


/*!
 * \brief Assign the thumbnails to be used for a dir in database
 * \param dir Dir id
 * \param id Image or dir id to use as cover/thumbnail
 * \note Only backends should modify the database
 */
void ImageDbWriter::SetCover(int dir, int id)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("UPDATE gallery_files SET "
                  "angle = :COVER "
                  "WHERE file_id = :DIR");
    query.bindValue(":COVER", id);
    query.bindValue(":DIR", dir);
    \
    if (!query.exec())
        MythDB::DBError("Error updating, query: ", query);
}


void ImageDbWriter::SetOrientation(ImageItem *im)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("UPDATE gallery_files SET "
                  "orientation = :ORIENTATION "
                  "WHERE file_id = :ID");
    query.bindValue(":ORIENTATION", im->m_orientation);
    query.bindValue(":ID", im->m_id);
    \
    if (!query.exec())
        MythDB::DBError("Error updating, query: ", query);
}


