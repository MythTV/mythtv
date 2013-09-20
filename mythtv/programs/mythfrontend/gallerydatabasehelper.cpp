// Qt headers

// MythTV headers
#include "mythcontext.h"
#include "mythdirs.h"

#include "gallerydatabasehelper.h"



GalleryDatabaseHelper::GalleryDatabaseHelper()
{

}



GalleryDatabaseHelper::~GalleryDatabaseHelper()
{

}



/** \fn     GalleryDatabaseHelper::GetStorageDirIDs(QStringList)
 *  \brief  Loads the directory ids of the storage groups from the database
 *  \return The list with the ids of the found directories
 */
// FIXME This doesn't do what it's supposed to do because we don't insert the
//       storage group root directories into the database! The storage group
//       needs to be inserted as the root node, with parentId of zero
QList<int> GalleryDatabaseHelper::GetStorageDirIDs()
{
    QList<int> sgIDs;

//     MSqlQuery query(MSqlQuery::InitCon());
//     query.prepare("SELECT dir_id FROM gallery_directories "
//                   "WHERE parent_id = 0;");
//
//
//     if (!query.exec())
//         LOG(VB_GENERAL, LOG_ERR, MythDB::DBErrorMessage(query.lastError()));
//
//     while (query.next())
//     {
//         sgIDs.append(query.value(0).toInt());
//     }

    return sgIDs;
}



/** \fn     GalleryDatabaseHelper::LoadParentDirectory(QList<ImageMetadata *>* , int)
 *  \brief  Loads the information from the database for a given directory
 *  \param  dbList The list where the results are stored
 *  \param  parentId The id of the given directory
 *  \return void
 */
void GalleryDatabaseHelper::LoadParentDirectory(QList<ImageMetadata *>* dbList, int parentId)
{
    dbList->clear();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT "
                  "dir_id, filename, name, path, parent_id, "
                  "dir_count, file_count, "
                  "hidden "
                  "FROM gallery_directories "
                  "WHERE dir_id = :PARENTID;");
    query.bindValue(":PARENTID", parentId);

    if (!query.exec())
        LOG(VB_GENERAL, LOG_ERR, MythDB::DBErrorMessage(query.lastError()));

    while (query.next())
    {
        ImageMetadata *im = new ImageMetadata();
        LoadDirectoryValues(query, im);

        // Overwrite the folder type
        im->m_type = kUpDirectory;
        dbList->append(im);
    }
}



/** \fn     GalleryDatabaseHelper::LoadDirectories(QMap<QString, ImageMetadata *>*)
 *  \brief  Loads all directory information from the database
 *  \param  dbList The list where the results are stored
 *  \return void
 */
void GalleryDatabaseHelper::LoadDirectories(QMap<QString, ImageMetadata *>* dbList)
{
    dbList->clear();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT "
                   "dir_id, filename, name, path, parent_id, "
                   "dir_count, file_count, hidden "
                   "FROM gallery_directories");

    if (!query.exec())
        LOG(VB_GENERAL, LOG_ERR, MythDB::DBErrorMessage(query.lastError()));

    while (query.next())
    {
        ImageMetadata *im = new ImageMetadata();
        LoadDirectoryValues(query, im);
        dbList->insert(im->m_fileName, im);
    }
}



/** \fn     GalleryDatabaseHelper::LoadDirectories(QList<ImageMetadata *>* , int)
 *  \brief  Loads all subdirectory information from the database for a given directory
 *  \param  dbList The list where the results are stored
 *  \param  parentId The id of the given directory
 *  \return void
 */
void GalleryDatabaseHelper::LoadDirectories(QList<ImageMetadata *>* dbList, int parentId)
{
    dbList->clear();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT "
                        "dir_id, filename, name, path, parent_id, "
                        "dir_count, file_count, "
                        "hidden "
                        "FROM gallery_directories "
                        "WHERE (parent_id = :PARENTID) "
                        "AND (hidden = '0' OR hidden = :HIDDEN) "
                        "ORDER BY name ASC;");
    query.bindValue(":PARENTID", parentId);
    query.bindValue(":HIDDEN", gCoreContext->GetNumSetting("GalleryShowHiddenFiles"));

    if (!query.exec())
        LOG(VB_GENERAL, LOG_ERR, MythDB::DBErrorMessage(query.lastError()));

    while (query.next())
    {
        ImageMetadata *im = new ImageMetadata();
        LoadDirectoryValues(query, im);
        dbList->append(im);
    }
}



/** \fn     GalleryDatabaseHelper::LoadFiles(QMap<QString, ImageMetadata *>*)
 *  \brief  Loads all file information from the database
 *  \param  dbList The list where the results are stored
 *  \return void
 */
void GalleryDatabaseHelper::LoadFiles(QMap<QString, ImageMetadata *>* dbList)
{
    dbList->clear();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT "
                    "file_id, filename, name, path, dir_id, "
                    "type, modtime, size, extension, "
                    "angle, date, zoom, hidden, orientation "
                    "FROM gallery_files");

    if (!query.exec())
        LOG(VB_GENERAL, LOG_ERR, MythDB::DBErrorMessage(query.lastError()));

    while (query.next())
    {
        ImageMetadata *im = new ImageMetadata();
        LoadFileValues(query, im);
        dbList->insert(im->m_fileName, im);
    }
}




/** \fn     GalleryDatabaseHelper::LoadFiles(QList<ImageMetadata *>* , int)
 *  \brief  Loads all file information from the database for a given directory
 *  \param  dbList The list where the results are stored
 *  \param  parentId The id of the given directory
 *  \return void
 */
void GalleryDatabaseHelper::LoadFiles(QList<ImageMetadata *>* dbList, int parentId)
{
    dbList->clear();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT "
                    "file_id, filename, name, path, dir_id, "
                    "type, modtime, size, extension, "
                    "angle, date, zoom, hidden, orientation "
                    "FROM gallery_files "
                    "WHERE (dir_id = :PARENTID) "
                    "AND (hidden = '0' OR hidden = :HIDDEN) "
                    "ORDER BY :ORDERBY");
    query.bindValue(":PARENTID", parentId);
    query.bindValue(":HIDDEN", gCoreContext->GetNumSetting("GalleryShowHiddenFiles"));
    query.bindValue(":ORDERBY", GetSortOrder());

    if (!query.exec())
        LOG(VB_GENERAL, LOG_ERR, MythDB::DBErrorMessage(query.lastError()));

    while (query.next())
    {
        ImageMetadata *im = new ImageMetadata();
        LoadFileValues(query, im);
        dbList->append(im);
    }
}



/** \fn     GalleryDatabaseHelper::InsertDirectory(ImageMetadata *)
 *  \brief  Saves information about a given directory in the database
 *  \param  im Information of the directory
 *  \return void
 */
int GalleryDatabaseHelper::InsertDirectory(ImageMetadata *im)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("INSERT INTO gallery_directories ("
                        "filename, name, path, parent_id, "
                        "dir_count, file_count, "
                        "hidden "
                        ") VALUES ("
                        ":FILENAME, :NAME, :PATH, :PARENT_ID, "
                        ":DIRCOUNT, :FILECOUNT, "
                        ":HIDDEN)");
    query.bindValue(":FILENAME",    im->m_fileName);
    query.bindValue(":NAME",        im->m_name);
    query.bindValue(":PATH",        im->m_path);
    query.bindValue(":PARENT_ID",   im->m_parentId);
    query.bindValue(":DIRCOUNT" ,   im->m_dirCount);
    query.bindValue(":FILECOUNT",   im->m_fileCount);
    query.bindValue(":HIDDEN",      im->m_isHidden);

    if (!query.exec())
        MythDB::DBError("Error inserting, query: ", query);

    return query.lastInsertId().toInt();
}



/** \fn     GalleryDatabaseHelper::InsertFile(ImageMetadata *)
 *  \brief  Saves information about a given file in the database
 *  \param  im Information of the file
 *  \return void
 */
int GalleryDatabaseHelper::InsertFile(ImageMetadata *im)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("INSERT INTO gallery_files ("
                    "filename, name, path, dir_id, "
                    "type, modtime, size, extension, "
                    "angle, date, zoom, "
                    "hidden, orientation "
                    ") VALUES ("
                    ":FILENAME, :NAME, :PATH, :DIR_ID, "
                    ":TYPE, :MODTIME, :SIZE, :EXTENSION, "
                    ":ANGLE, :DATE, :ZOOM, "
                    ":HIDDEN, :ORIENT)");
    query.bindValue(":FILENAME",    im->m_fileName);
    query.bindValue(":NAME",        im->m_name);
    query.bindValue(":PATH",        im->m_path);
    query.bindValue(":DIR_ID",      im->m_parentId);
    query.bindValue(":TYPE",        im->m_type);
    query.bindValue(":MODTIME",     im->m_modTime);
    query.bindValue(":SIZE",        im->m_size);
    query.bindValue(":EXTENSION",   im->m_extension);
    query.bindValue(":ANGLE",       im->GetAngle());
    query.bindValue(":DATE",        im->m_date);
    query.bindValue(":ZOOM",        im->GetZoom());
    query.bindValue(":HIDDEN",      im->m_isHidden);
    query.bindValue(":ORIENT",      im->GetOrientation());

    if (!query.exec())
        MythDB::DBError("Error inserting, query: ", query);

    return query.lastInsertId().toInt();
}



/** \fn     GalleryDatabaseHelper::UpdateDirectory(ImageMetadata *)
 *  \brief  Updates the information about a given directory in the database
 *  \param  im Information of the directory
 *  \return void
 */
void GalleryDatabaseHelper::UpdateDirectory(ImageMetadata *im)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE gallery_directories SET "
                    "filename =     :FILENAME, "
                    "name =         :NAME, "
                    "path =         :PATH, "
                    "parent_id =    :PARENT_ID, "
                    "dir_count =    :DIR_COUNT, "
                    "file_count =   :FILE_COUNT, "
                    "hidden =       :HIDDEN "
                    "WHERE dir_id = :ID;");
    query.bindValue(":FILENAME",    im->m_fileName);
    query.bindValue(":NAME",        im->m_name);
    query.bindValue(":PATH",        im->m_path);
    query.bindValue(":PARENT_ID",   im->m_parentId);
    query.bindValue(":DIR_COUNT",   im->m_dirCount);
    query.bindValue(":FILE_COUNT",  im->m_fileCount);
    query.bindValue(":HIDDEN",      im->m_isHidden);
    query.bindValue(":ID",          im->m_id);

    if (!query.exec())
        MythDB::DBError("Error updating, query: ", query);
}



/** \fn     GalleryDatabaseHelper::UpdateFile(ImageMetadata *)
 *  \brief  Updates the information about a given file in the database
 *  \param  im Information of the file
 *  \return void
 */
void GalleryDatabaseHelper::UpdateFile(ImageMetadata *im)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE gallery_files SET "
                    "filename       = :FILENAME, "
                    "name           = :NAME, "
                    "path           = :PATH, "
                    "dir_id         = :DIR_ID, "
                    "type           = :TYPE, "
                    "modtime        = :MODTIME, "
                    "size           = :SIZE, "
                    "extension      = :EXTENSION, "
                    "angle          = :ANGLE, "
                    "date           = :DATE, "
                    "zoom           = :ZOOM, "
                    "hidden         = :HIDDEN, "
                    "orientation    = :ORIENT "
                    "WHERE file_id  = :ID;");
    query.bindValue(":FILENAME",    im->m_fileName);
    query.bindValue(":NAME",        im->m_name);
    query.bindValue(":PATH",        im->m_path);
    query.bindValue(":DIR_ID",      im->m_parentId);
    query.bindValue(":TYPE",        im->m_type);
    query.bindValue(":MODTIME",     im->m_modTime);
    query.bindValue(":SIZE",        im->m_size);
    query.bindValue(":EXTENSION",   im->m_extension);
    query.bindValue(":ANGLE",       im->GetAngle());
    query.bindValue(":DATE",        im->m_date);
    query.bindValue(":ZOOM",        im->GetZoom());
    query.bindValue(":HIDDEN",      im->m_isHidden);
    query.bindValue(":ORIENT",      im->GetOrientation());
    query.bindValue(":ID",          im->m_id);

    if (!query.exec())
        MythDB::DBError("Error updating, query: ", query);
}



/** \fn     GalleryDatabaseHelper::RemoveDirectory(ImageMetadata *)
 *  \brief  Deletes the information about a given directory in the database
 *  \param  im Information of the directory
 *  \return void
 */
void GalleryDatabaseHelper::RemoveDirectory(ImageMetadata *im)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE from gallery_directories WHERE dir_id = :ID;");
    query.bindValue(":ID", im->m_id);

    if (!query.exec())
        MythDB::DBError("Error removing, query: ", query);
}



/** \fn     GalleryDatabaseHelper::RemoveFile(ImageMetadata *)
 *  \brief  Deletes the information about a given file in the database
 *  \param  im Information of the directory
 *  \return void
 */
void GalleryDatabaseHelper::RemoveFile(ImageMetadata *im)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE from gallery_files WHERE file_id = :ID;");
    query.bindValue(":ID", im->m_id);

    if (!query.exec())
        MythDB::DBError("Error removing, query: ", query);
}



/** \fn     GalleryDatabaseHelper::InsertData(ImageMetadata *)
 *  \brief  Inserts either a new directory or file in the database
 *  \param  im Information of the given item
 *  \return void
 */
void GalleryDatabaseHelper::InsertData(ImageMetadata *im)
{
    if (!im)
        return;

    if (im->m_type == kSubDirectory || im->m_type == kUpDirectory)
        InsertDirectory(im);

    if (im->m_type == kImageFile || im->m_type == kVideoFile)
        InsertFile(im);
}



/** \fn     GalleryDatabaseHelper::UpdateData(ImageMetadata *)
 *  \brief  Updates either a directory or a file in the database
 *  \param  im Information of the given item
 *  \return void
 */
void GalleryDatabaseHelper::UpdateData(ImageMetadata *im)
{
    if (!im)
        return;

    if (im->m_type == kSubDirectory || im->m_type == kUpDirectory)
        UpdateDirectory(im);

    if (im->m_type == kImageFile || im->m_type == kVideoFile)
        UpdateFile(im);
}



/** \fn     GalleryDatabaseHelper::RemoveData(ImageMetadata *)
 *  \brief  Deletes either a directory or file from the database
 *  \param  im Information of the given item
 *  \return void
 */
void GalleryDatabaseHelper::RemoveData(ImageMetadata *im)
{
    if (!im)
        return;

    if (im->m_type == kSubDirectory || im->m_type == kUpDirectory)
        RemoveDirectory(im);

    if (im->m_type == kImageFile || im->m_type == kVideoFile)
        RemoveFile(im);
}



/** \fn     GalleryDatabaseHelper::LoadDirectoryValues(MSqlQuery &, ImageMetadata *)
 *  \brief  Loads the directory information from the database
 *  \param  query Information from the database
 *  \param  im Holds the loaded information
 *  \return void
 */
void GalleryDatabaseHelper::LoadDirectoryValues(MSqlQuery &query, ImageMetadata *im)
{
    im->m_id            = query.value(0).toInt();
    im->m_fileName      = query.value(1).toString();
    im->m_name          = query.value(2).toString();
    im->m_path          = query.value(3).toString();
    im->m_parentId      = query.value(4).toInt();
    im->m_dirCount      = query.value(5).toInt();
    im->m_fileCount     = query.value(6).toInt();
    im->m_isHidden      = query.value(7).toInt();

    // preset all directories as subfolders
    im->m_type          = kSubDirectory;

    LoadDirectoryThumbnailValues(im);
}



/** \fn     GalleryDatabaseHelper::LoadFileValues(MSqlQuery &, ImageMetadata *)
 *  \brief  Loads the file information from the database
 *  \param  query Information from the database
 *  \param  im Holds the loaded information
 *  \return void
 */
void GalleryDatabaseHelper::LoadFileValues(MSqlQuery &query, ImageMetadata *im)
{
    im->m_id            = query.value(0).toInt();
    im->m_fileName      = query.value(1).toString();
    im->m_name          = query.value(2).toString();
    im->m_path          = query.value(3).toString();
    im->m_parentId      = query.value(4).toInt();
    im->m_type          = query.value(5).toInt();
    im->m_modTime       = query.value(6).toInt();
    im->m_size          = query.value(7).toInt();
    im->m_extension     = query.value(8).toString();
    im->SetAngle(         query.value(9).toInt());
    im->m_date          = query.value(10).toInt();
    im->SetZoom(          query.value(11).toInt());
    im->m_isHidden      = query.value(12).toInt();
    im->SetOrientation(   query.value(13).toInt(), true);

    LoadFileThumbnailValues(im);
}



/** \fn     GalleryDatabaseHelper::LoadDirectoryThumbnailValues(ImageMetadata *)
 *  \brief  Gets four images from the directory from the
 *          database which will be used as a folder thumbnail
 *  \param  im Holds the loaded information
 *  \return void
 */
void GalleryDatabaseHelper::LoadDirectoryThumbnailValues(ImageMetadata *im)
{
    // Try to get four new thumbnail filenames
    // from the available images in this folder
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT filename, path FROM gallery_files "
                          "WHERE path LIKE :PATH "
                          "AND type = '4' "
                          "AND hidden = '0' LIMIT :LIMIT");
    query.bindValue(":PATH", im->m_path);
    query.bindValue(":LIMIT", kMaxFolderThumbnails);

    if (!query.exec())
        LOG(VB_GENERAL, LOG_ERR, MythDB::DBErrorMessage(query.lastError()));

    int i = 0;
    while (query.next())
    {
        QString thumbFileName = QString("%1%2")
                .arg(GetConfDir().append("/MythImage/"))
                .arg(query.value(0).toString());

        if (i >= im->m_thumbFileNameList->size())
            break;

        im->m_thumbFileNameList->replace(i, thumbFileName);
        im->m_thumbPath = query.value(1).toString();
        ++i;
    }

    // Set the path to the thumbnail files. As a default this will be
    // the path ".mythtv/MythGallery" in the users home directory
    im->m_thumbPath.prepend(GetConfDir().append("/MythImage/"));
}



/** \fn     GalleryDatabaseHelper::LoadFileThumbnailValues(ImageMetadata *)
 *  \brief  Sets the thumbnail information for a file
 *  \param  im Holds the loaded information
 *  \return void
 */
void GalleryDatabaseHelper::LoadFileThumbnailValues(ImageMetadata *im)
{
    // Set the path to the thumbnail files. As a default this will be
    // the path ".mythtv/MythGallery" in the users home directory
    im->m_thumbPath = im->m_path;
    im->m_thumbPath.prepend(GetConfDir().append("/MythImage/"));

    // Create the full path and filename to the thumbnail image
    QString thumbFileName = QString("%1%2")
            .arg(GetConfDir().append("/MythImage/"))
            .arg(im->m_fileName);

    // If the file is a video then append a png, otherwise the preview
    // image would not be readable due to the video file extension
    if (im->m_type == kVideoFile)
        thumbFileName.append(".png");

    im->m_thumbFileNameList->replace(0, thumbFileName);
}



/** \fn     GalleryDatabaseHelper::GetSortOrder()
 *  \brief  Prepares the SQL query according to the sorting
 *          rules specified by the user in the settings.
 *  \return void
 */
QString GalleryDatabaseHelper::GetSortOrder()
{
    // prepare the sorting statement
    QString sort;
    switch (gCoreContext->GetNumSetting("GallerySortOrder"))
    {
    case kSortByNameAsc:
        sort.append("name ASC ");
        break;
    case kSortByNameDesc:
        sort.append("name DESC ");
        break;
    case kSortByModTimeAsc:
        sort.append("modtime ASC, name ASC ");
        break;
    case kSortByModTimeDesc:
        sort.append("modtime DESC, name ASC ");
        break;
    case kSortByExtAsc:
        sort.append("extension ASC, name ASC ");
        break;
    case kSortByExtDesc:
        sort.append("extension DESC, name ASC ");
        break;
    case kSortBySizeAsc:
        sort.append("size ASC, name ASC ");
        break;
    case kSortBySizeDesc:
        sort.append("size DESC, name ASC ");
        break;
    case kSortByDateAsc:
        sort.append("date ASC, name ASC ");
        break;
    case kSortByDateDesc:
        sort.append("date DESC, name ASC ");
        break;
    default:
        sort.append("name ASC ");
        break;
    }

    return sort;
}



/** \fn     GalleryDatabaseHelper::ClearDatabase()
 *  \brief  Removes all contents from the gallery_directories and gallery_files tables.
 *  \return void
 */
void GalleryDatabaseHelper::ClearDatabase()
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(QString("TRUNCATE gallery_directories;"));

    if (!query.exec())
        LOG(VB_GENERAL, LOG_ERR, MythDB::DBErrorMessage(query.lastError()));

    query.prepare(QString("TRUNCATE gallery_files;"));

    if (!query.exec())
        LOG(VB_GENERAL, LOG_ERR, MythDB::DBErrorMessage(query.lastError()));
}
