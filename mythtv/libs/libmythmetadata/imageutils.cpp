// Qt headers

// MythTV headers
#include "mythcontext.h"
#include "mythdirs.h"
#include "storagegroup.h"
#include "imageutils.h"


// The maximum possible value of the utc time
#define MAX_UTCTIME 2147483646;

ImageUtils* ImageUtils::m_instance = NULL;

ImageUtils::ImageUtils()
{
    m_imageFileExt = QString("jpg,jpeg,png,tif,tiff,bmp,gif").split(",");
    m_videoFileExt = QString("avi,mpg,mp4,mpeg,mov,wmv,3gp").split(",");
}



ImageUtils::~ImageUtils()
{

}



ImageUtils* ImageUtils::getInstance()
{
    if (!m_instance)
        m_instance = new ImageUtils();

    return m_instance;
}



/** \fn     ImageUtils::LoadDirectoryFromDB(QMap<QString, ImageMetadata *>*)
 *  \brief  Loads all directory information from the database
 *  \param  dbList The list where the results are stored
 *  \return void
 */
void ImageUtils::LoadDirectoriesFromDB(QMap<QString, ImageMetadata *>* dbList)
{
    dbList->clear();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
                QString("SELECT "
                        "dir_id, filename, name, path, parent_id, "
                        "dir_count, file_count, "
                        "hidden "
                        "FROM gallery_directories"));

    if (!query.exec())
        LOG(VB_GENERAL, LOG_ERR, MythDB::DBErrorMessage(query.lastError()));

    if (query.size() > 0)
    {
        while (query.next())
        {
            ImageMetadata *im = new ImageMetadata();
            LoadDirectoryValues(query, im);
            dbList->insert(im->m_fileName, im);
        }
    }
}



/** \fn     ImageUtils::LoadFilesFromDB(QMap<QString, ImageMetadata *>*)
 *  \brief  Loads all file information from the database
 *  \param  dbList The list where the results are stored
 *  \return void
 */
void ImageUtils::LoadFilesFromDB(QMap<QString, ImageMetadata *>* dbList)
{
    dbList->clear();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
                QString("SELECT "
                        "file_id, CONCAT_WS('/', path, filename), name, path, "
                        "dir_id, type, modtime, size, extension, "
                        "angle, date, zoom, "
                        "hidden, orientation "
                        "FROM gallery_files"));

    if (!query.exec())
        LOG(VB_GENERAL, LOG_ERR, MythDB::DBErrorMessage(query.lastError()));

    if (query.size() > 0)
    {
        while (query.next())
        {
            ImageMetadata *im = new ImageMetadata();
            LoadFileValues(query, im);
            dbList->insert(im->m_fileName, im);
        }
    }
}



/** \fn     ImageUtils::LoadFileFromDB(ImageMetadata *, int)
 *  \brief  Load the file information from the database given by the id
 *  \param  im The image metadata which holds the information
 *  \return void
 */
void ImageUtils::LoadFileFromDB(ImageMetadata * im, int id)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
                QString("SELECT "
                        "file_id, CONCAT_WS('/', path, filename), name, path, dir_id, "
                        "type, modtime, size, extension, "
                        "angle, date, zoom, "
                        "hidden, orientation "
                        "FROM gallery_files "
                        "WHERE file_id = :FILE_ID;"));
    query.bindValue(":FILE_ID", id);

    if (!query.exec())
        LOG(VB_GENERAL, LOG_ERR, MythDB::DBErrorMessage(query.lastError()));

    if (query.size() > 0)
    {
        while (query.next())
        {
            LoadFileValues(query, im);
        }
    }
}



/** \fn     ImageUtils::InsertDirectoryIntoDB(ImageMetadata *)
 *  \brief  Saves information about a given directory in the database
 *  \param  dm Information of the directory
 *  \return void
 */
int ImageUtils::InsertDirectoryIntoDB(ImageMetadata *im)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
                QString("INSERT INTO gallery_directories ("
                        "filename, name, path, parent_id, "
                        "dir_count, file_count, "
                        "hidden "
                        ") VALUES ("
                        ":FILENAME, :NAME, :PATH, :PARENT_ID, "
                        ":DIRCOUNT, :FILECOUNT, "
                        ":HIDDEN);"));
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



/** \fn     ImageUtils::InsertFileIntoDB(ImageMetadata *)
 *  \brief  Saves information about a given file in the database
 *  \param  dm Information of the file
 *  \return void
 */
int ImageUtils::InsertFileIntoDB(ImageMetadata *im)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
                QString("INSERT INTO gallery_files ("
                        "filename, name, path, dir_id, "
                        "type, modtime, size, extension, "
                        "angle, date, zoom, "
                        "hidden, orientation "
                        ") VALUES ("
                        ":FILENAME, :NAME, :PATH, :DIR_ID, "
                        ":TYPE, :MODTIME, :SIZE, :EXTENSION, "
                        ":ANGLE, :DATE, :ZOOM, "
                        ":HIDDEN, :ORIENT)"));
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



/** \fn     ImageUtils::UpdateDirectoryInDB(ImageMetadata *)
 *  \brief  Updates the information about a given directory in the database
 *  \param  dm Information of the directory
 *  \return void
 */
bool ImageUtils::UpdateDirectoryInDB(ImageMetadata *im)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
                QString("UPDATE gallery_directories SET "
                        "filename =     :FILENAME, "
                        "name =         :NAME, "
                        "path =         :PATH, "
                        "parent_id =    :PARENT_ID, "
                        "dir_count =    :DIR_COUNT, "
                        "file_count =   :FILE_COUNT, "
                        "hidden =       :HIDDEN "
                        "WHERE dir_id = :ID;"));
    query.bindValue(":FILENAME",    im->m_fileName);
    query.bindValue(":NAME",        im->m_name);
    query.bindValue(":PATH",        im->m_path);
    query.bindValue(":PARENT_ID",   im->m_parentId);
    query.bindValue(":DIR_COUNT",   im->m_dirCount);
    query.bindValue(":FILE_COUNT",  im->m_fileCount);
    query.bindValue(":HIDDEN",      im->m_isHidden);
    query.bindValue(":ID",          im->m_id);

    return query.exec();
}



/** \fn     ImageUtils::UpdateFileInDB(ImageMetadata *)
 *  \brief  Updates the information about a given file in the database
 *  \param  dm Information of the file
 *  \return void
 */
bool ImageUtils::UpdateFileInDB(ImageMetadata *im)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
                QString("UPDATE gallery_files SET "
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
                        "WHERE file_id  = :ID;"));
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

    return query.exec();
}



/** \fn     ImageUtils::RemoveFromDB(ImageMetadata *im)
 *  \brief  Deletes either a directory or file from the database
 *  \param  im Information of the given item
 *  \return void
 */
bool ImageUtils::RemoveFromDB(ImageMetadata *im)
{
    if (!im)
        return false;

    if (im->m_type == kSubDirectory || im->m_type == kUpDirectory)
        return RemoveDirectoryFromDB(im);

    if (im->m_type == kImageFile || im->m_type == kVideoFile)
        return RemoveFileFromDB(im);

    return false;
}



/** \fn     ImageUtils::RemoveDirectoryFromDB(ImageMetadata *)
 *  \brief  Deletes the information about a given directory in the database
 *  \param  im Information of the directory
 *  \return void
 */
bool ImageUtils::RemoveDirectoryFromDB(ImageMetadata *im)
{
    if (!im)
        return false;

    return RemoveDirectoryFromDB(im->m_id);
}



bool ImageUtils::RemoveDirectoryFromDB(int id)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
                QString("DELETE from gallery_directories "
                        "WHERE dir_id = :ID;"));
    query.bindValue(":ID", id);

    return query.exec();
}



/** \fn     ImageUtils::RemoveFileFromDB(ImageMetadata *)
 *  \brief  Deletes the information about a given file in the database
 *  \param  im Information of the directory
 *  \return void
 */
bool ImageUtils::RemoveFileFromDB(ImageMetadata *im)
{
    if (!im)
        return false;

    return RemoveFileFromDB(im->m_id);
}



bool ImageUtils::RemoveFileFromDB(int id)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
                QString("DELETE from gallery_files "
                        "WHERE file_id = :ID;"));
    query.bindValue(":ID", id);

    return query.exec();
}



/** \fn     ImageUtils::LoadDirectoryValues(MSqlQuery &, ImageMetadata *)
 *  \brief  Loads the directory information from the database into the dataMap
 *  \param  query Information from the database
 *  \param  dm Holds the loaded information
 *  \return void
 */
void ImageUtils::LoadDirectoryValues(MSqlQuery &query, ImageMetadata *dm)
{
    dm->m_id            = query.value(0).toInt();
    dm->m_fileName      = query.value(1).toString();
    dm->m_name          = query.value(2).toString();
    dm->m_path          = query.value(3).toString();
    dm->m_parentId      = query.value(4).toInt();
    dm->m_dirCount      = query.value(5).toInt();
    dm->m_fileCount     = query.value(6).toInt();
    dm->m_isHidden      = query.value(7).toInt();

    // preset all directories as subfolders
    dm->m_type          = kSubDirectory;

    LoadDirectoryThumbnailValues(dm);
}



/** \fn     ImageUtils::LoadFileValues(MSqlQuery &, ImageMetadata *)
 *  \brief  Loads the file information from the database into the dataMap
 *  \param  query Information from the database
 *  \param  dm Holds the loaded information
 *  \return void
 */
void ImageUtils::LoadFileValues(MSqlQuery &query, ImageMetadata *dm)
{
    dm->m_id            = query.value(0).toInt();
    dm->m_fileName      = query.value(1).toString();
    dm->m_name          = query.value(2).toString();
    dm->m_path          = query.value(3).toString();
    dm->m_parentId      = query.value(4).toInt();
    dm->m_type          = query.value(5).toInt();
    dm->m_modTime       = query.value(6).toInt();
    dm->m_size          = query.value(7).toInt();
    dm->m_extension     = query.value(8).toString();
    dm->SetAngle(         query.value(9).toInt());
    dm->m_date          = query.value(10).toInt();
    dm->SetZoom(          query.value(11).toInt());
    dm->m_isHidden      = query.value(12).toInt();
    dm->SetOrientation(   query.value(13).toInt(), true);

    LoadFileThumbnailValues(dm);
}



/** \fn     ImageUtils::GetStorageDirs()
 *  \brief  Gets the available storage groups
 *  \return List of all available storage groups
 */
QStringList ImageUtils::GetStorageDirs()
{
    QStringList sgDirList;

    QString sgName = IMAGE_STORAGE_GROUP;

    if (!sgName.isEmpty())
    {
        QString host = gCoreContext->GetHostName();

        // Search for the specified dirs in the defined storage group.
        // If there is no such storage group then don't use the fallback
        // and don't get the default storage group name of "/mnt/store".
        // The list will be empty. The user has to check the settings.
        StorageGroup sg;
        sg.Init(sgName, host, false);
        sgDirList = sg.GetDirList();
    }

    return sgDirList;
}



/** \fn     ImageUtils::LoadDirectoryData(QFileInfo &, DataMap *, int)
 *  \brief  Loads the information from the fileInfo into the dataMap object
 *  \param  fileInfo Holds the information about the file
 *  \param  data Holds the loaded information about a file
 *  \param  parentId The parent directory
 *  \return void
 */
void ImageUtils::LoadDirectoryData(QFileInfo &fileInfo,
                                   ImageMetadata *data,
                                   int parentId,
                                   const QString &baseDirectory)
{
    QDir baseDir(baseDirectory);
    data->m_parentId    = parentId;
    data->m_fileName    = baseDir.relativeFilePath(fileInfo.absoluteFilePath());
    data->m_name        = fileInfo.fileName();
    data->m_path        = baseDir.relativeFilePath(fileInfo.absoluteFilePath());
    data->m_isHidden    = fileInfo.isHidden();

    QDir dir(data->m_fileName);
    data->m_dirCount = dir.entryList(QDir::Dirs |
                                     QDir::NoSymLinks |
                                     QDir::NoDotAndDotDot |
                                     QDir::Readable).count();

    data->m_fileCount = dir.entryList(QDir::Files |
                                      QDir::NoSymLinks |
                                      QDir::NoDotAndDotDot |
                                      QDir::Readable).count();
}



/** \fn     ImageUtils::LoadFileData(QFileInfo &, DataMap *)
 *  \brief  Loads the information from the fileInfo into the dataMap object
 *  \param  fileInfo Holds the information about the file
 *  \param  data Holds the loaded information about a file
 *  \return void
 */
void ImageUtils::LoadFileData(QFileInfo &fileInfo,
                              ImageMetadata *data,
                              const QString &baseDirectory)
{
    QDir baseDir(baseDirectory);
    data->m_fileName    = fileInfo.fileName();
    data->m_name        = fileInfo.fileName();
    data->m_path        = baseDir.relativeFilePath(fileInfo.absolutePath());
    if (data->m_path.isNull()) // Hack because relativeFilePath will return a null instead of empty string
        data->m_path = "";
    data->m_modTime     = fileInfo.lastModified().toTime_t();
    data->m_size        = fileInfo.size();
    data->m_isHidden    = fileInfo.isHidden();
    data->m_extension   = fileInfo.completeSuffix().toLower();

    // Set defaults, the data will be loaded later
    data->SetAngle(0);
    data->m_date = MAX_UTCTIME;

    if (m_imageFileExt.contains(data->m_extension))
    {
        data->m_type = kImageFile;
    }
    else if (m_videoFileExt.contains(data->m_extension))
    {
        data->m_type = kVideoFile;
    }
    else
    {
        data->m_type = kUnknown;
    }
}



/** \fn     ImageUtils::GetExifOrientation(const QString &fileName, bool *ok)
 *  \brief  Reads and returns the orientation value
 *  \param  fileName The filename that holds the exif data
 *  \param  ok Will be set to true if the reading was ok, otherwise false
 *  \return The orientation value
 */
int ImageUtils::GetExifOrientation(const QString &fileName, bool *ok)
{
    QString tag = "Exif.Image.Orientation";
    QString value = GetExifValue(fileName, tag, ok);

    // The orientation of the image. Only return the value if its valid
    // See http://jpegclub.org/exif_orientation.html for details
    bool valid;
    int orientation = QString(value).toInt(&valid);
    return (valid) ? orientation : 1;
}



/** \fn     ImageUtils::SetExifOrientation(const QString &, const long , bool *)
 *  \brief  Saves the given value in the orientation exif tag
 *  \param  fileName The filename that holds the exif data
 *  \param  orientation The value that shall be saved in the exif data
 *  \param  ok Will be set to true if the update was ok, otherwise false
 *  \return void
 */
void ImageUtils::SetExifOrientation(const QString &fileName,
                                    const int orientation, bool *ok)
{
    // the orientation of the image.
    // See http://jpegclub.org/exif_orientation.html for details
    if (orientation >= 1 && orientation <= 8)
    {
        QString tag = "Exif.Image.Orientation";
        SetExifValue(fileName, tag, QString::number(orientation), ok);
    }
}



/** \fn     ImageUtils::GetExifDate(const QString &, bool *)
 *  \brief  Reads and returns the exif date
 *  \param  fileName The filename that holds the exif data
 *  \param  ok Will be set to true if the reading was ok, otherwise false
 *  \return The date in utc time
 */
long ImageUtils::GetExifDate(const QString &fileName, bool *ok)
{
    long utcTime = MAX_UTCTIME;

    QString tag = "Exif.Image.DateTime";
    QString value = GetExifValue(fileName, tag, ok);

    // convert the string into the UTC time. We need to split
    // the exif time format, which is this: "2006:07:21 18:54:58"
    if (!value.isEmpty())
    {
        bool ok;
        QDateTime dateTime =
                QDateTime(QDate(value.mid(0,4).toInt(&ok, 10),
                                value.mid(5,2).toInt(&ok, 10),
                                value.mid(8,2).toInt(&ok, 10)),
                          QTime(value.mid(11,2).toInt(&ok, 10),
                                value.mid(14,2).toInt(&ok, 10),
                                value.mid(17,2).toInt(&ok, 10), 0));

        // convert it to the utc time so
        // we can easily compare it later.
        utcTime = dateTime.toTime_t();
    }

    return utcTime;
}



/** \fn     ImageUtils::SetExifDate(const QString &, const long , bool *)
 *  \brief  Saves the given date in the date exif tag
 *  \param  fileName The filename that holds the exif data
 *  \param  date The date that shall be saved in the exif data
 *  \param  ok Will be set to true if the update was ok, otherwise false
 *  \return void
 */
void ImageUtils::SetExifDate(const QString &fileName,
                             const long date, bool *ok)
{
    QString value;

    // Convert the date number into the UTC time and then
    // into the string with the format "2006:07:21 18:54:58".
    QDateTime dateTime;
    dateTime.setTime_t(date);
    value = dateTime.toString("yyyy:MM:dd hh:mm:ss");

    if (!value.isEmpty())
    {
        QString tag = "Exif.Image.DateTime";
        SetExifValue(fileName, tag, value, ok);
    }
}



/** \fn     ImageUtils::GetAllExifValues(const QString &)
 *  \brief  Reads and returns all non empty tags from the given file
 *  \param  fileName The filename that holds the exif data
 *  \return The list of exif tag names and values
 */
QList<QStringList> ImageUtils::GetAllExifValues(const QString &fileName)
{
    // default value, an empty list means no
    // exif tags were found or an error occured
    QList<QStringList> valueList;

    try
    {
        Exiv2::Image::AutoPtr image =
                Exiv2::ImageFactory::open(fileName.toLocal8Bit().constData());

        if (image.get())
        {
            image->readMetadata();
            Exiv2::ExifData &exifData = image->exifData();
            if (!exifData.empty())
            {
                LOG(VB_FILE, LOG_DEBUG,
                    QString("Found %1 tag(s) for file %2")
                    .arg(exifData.count())
                    .arg(fileName));

                Exiv2::ExifData::const_iterator end = exifData.end();
                Exiv2::ExifData::const_iterator i = exifData.begin();
                for (; i != end; ++i)
                {
                    QString value = QString::fromStdString(i->value().toString());

                    // Do not add empty tags to the list
                    if (!value.isEmpty())
                    {
                        QStringList values;

                        // These three are the same as i->key()
                        values.append(QString::fromStdString(i->familyName()));
                        values.append(QString::fromStdString(i->groupName()));
                        values.append(QString::fromStdString(i->tagName()));
                        values.append(QString::fromStdString(i->key()));
                        values.append(QString::fromStdString(i->tagLabel()));
                        values.append(QString::fromStdString(i->value().toString()));

                        // Add the exif information to the list
                        valueList.append(values);
                    }
                }
            }
            else
            {
                LOG(VB_FILE, LOG_DEBUG,
                    QString("Exiv2 error: No header, file %1")
                    .arg(fileName));
            }
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Exiv2 error: Could not open file, file %1")
                .arg(fileName));
        }
    }
    catch (Exiv2::Error& e)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Exiv2 exception %1, file %2")
            .arg(e.what()).arg(fileName));
    }

    return valueList;
}



/** \fn     ImageUtils::GetExifValue(const QString &, const QString &, bool *)
 *  \brief  Reads and returns the value of an exif tag in a file
 *  \param  fileName The filename that holds the exif data
 *  \param  exifTag The key that shall be updated
 *  \param  ok Will be set to true if the reading was ok, otherwise false
 *  \return The value of the exif tag or an empty string
 */
QString ImageUtils::GetExifValue(const QString &fileName,
                                 const QString &exifTag,
                                 bool *ok)
{
    // Assume the exif reading fails
    *ok = false;

    // default value
    QString value("");

    try
    {
        Exiv2::Image::AutoPtr image =
                Exiv2::ImageFactory::open(fileName.toLocal8Bit().constData());

        if (image.get())
        {
            image->readMetadata();
            Exiv2::ExifData &exifData = image->exifData();
            if (!exifData.empty())
            {
                Exiv2::Exifdatum &datum =
                        exifData[exifTag.toLocal8Bit().constData()];

                value = QString::fromStdString(datum.toString());
                if (!value.isEmpty())
                {
                    *ok = true;
                }
                else
                {
                    LOG(VB_FILE, LOG_DEBUG,
                        QString("Exiv2 error: No tag found, file %1, tag %2")
                        .arg(fileName).arg(exifTag));
                }
            }
            else
            {
                LOG(VB_FILE, LOG_DEBUG,
                    QString("Exiv2 error: No header, file %1, tag %2")
                    .arg(fileName).arg(exifTag));
            }
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Exiv2 error: Could not open file, file %1, tag %2")
                .arg(fileName).arg(exifTag));
        }
    }
    catch (Exiv2::Error& e)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Exiv2 exception %1, file %2, tag %3")
            .arg(e.what()).arg(fileName).arg(exifTag));
    }

    return value;
}



/** \fn     ImageUtils::SetExifValue(const QString &, const QString &, const QString &, bool *)
 *  \brief  Updates the exif tag in a file with the given value
 *  \param  fileName The filename that holds the exif data
 *  \param  exifTag The key that shall be updated
 *  \param  value The new value of the exif tag
 *  \param  ok Will be set to true if the update was ok, otherwise false
 *  \return True if the exif key exists, otherwise false
 */
void ImageUtils::SetExifValue(const QString &fileName,
                              const QString &exifTag,
                              const QString &value,
                              bool *ok)
{
    // Assume the exif writing fails
    *ok = false;

    try
    {
        Exiv2::Image::AutoPtr image =
                Exiv2::ImageFactory::open(fileName.toLocal8Bit().constData());

        if (image.get())
        {
            Exiv2::ExifData exifData;
            Exiv2::Exifdatum &datum = exifData[exifTag.toLocal8Bit().constData()];
            datum.setValue(value.toLocal8Bit().constData());

            image->setExifData(exifData);
            image->writeMetadata();

            *ok = true;
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Exiv2 error: Could not open file, file %1, tag %2")
                .arg(fileName).arg(exifTag));
        }
    }
    catch (Exiv2::Error& e)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Exiv2 exception %1, file %2, tag %3, value %4")
            .arg(e.what()).arg(fileName).arg(exifTag).arg(value));
    }
}



/** \fn     ImageUtils::HasExifKey(Exiv2::ExifData, const QString &)
 *  \brief  Checks if the exif data of the file contains the given key
 *  \param  exifData The entire exif data
 *  \param  exifTag The key that shall be checked
 *  \return True if the exif key exists, otherwise false
 */
bool ImageUtils::HasExifKey(Exiv2::ExifData exifData,
                            const QString &exifTag)
{
    Exiv2::ExifKey key(exifTag.toLocal8Bit().constData());
    Exiv2::ExifData::iterator it = exifData.findKey(key);

    // If the iterator has is the end of the
    // list then the key has not been found
    return !(it == exifData.end());
}

/**
 *  \brief  Gets four images from the directory from the
 *          database which will be used as a folder thumbnail
 *  \param  im Holds the loaded information
 *  \return void
 */
void ImageUtils::LoadDirectoryThumbnailValues(ImageMetadata *im)
{
    // Try to get four new thumbnail filenames
    // from the available images in this folder
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT CONCAT_WS('/', path, filename), path FROM gallery_files "
                          "WHERE path = :PATH "
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
                .arg(GetConfDir().append("/tmp/MythImage/"))
                .arg(query.value(0).toString());

        if (i >= im->m_thumbFileNameList->size())
            break;

        im->m_thumbFileNameList->replace(i, thumbFileName);
        im->m_thumbPath = query.value(1).toString();
        ++i;
    }

    // Set the path to the thumbnail files. As a default this will be
    // the path ".mythtv/MythGallery" in the users home directory
    im->m_thumbPath.prepend(GetConfDir().append("/tmp/MythImage/"));
}



/**
 *  \brief  Sets the thumbnail information for a file
 *  \param  im Holds the loaded information
 *  \return void
 */
void ImageUtils::LoadFileThumbnailValues(ImageMetadata *im)
{
    // Set the path to the thumbnail files. As a default this will be
    // the path ".mythtv/MythGallery" in the users home directory
    im->m_thumbPath = im->m_path;
    im->m_thumbPath.prepend(GetConfDir().append("/tmp/MythImage/"));

    // Create the full path and filename to the thumbnail image
    QString thumbFileName = QString("%1%2")
                                .arg(GetConfDir().append("/tmp/MythImage/"))
                                .arg(im->m_fileName);

    // If the file is a video then append a png, otherwise the preview
    // image would not be readable due to the video file extension
    if (im->m_type == kVideoFile)
        thumbFileName.append(".png");

    im->m_thumbFileNameList->replace(0, thumbFileName);
}

