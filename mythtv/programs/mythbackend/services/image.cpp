//////////////////////////////////////////////////////////////////////////////
// Program Name: image.cpp
// Created     : Jul. 27, 2012
//
// Copyright (c) 2012 Robert Siebert  <trebor_s@web.de>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#include <QFile>

#include "mythcorecontext.h"
#include "storagegroup.h"

#include "imagescan.h"
#include "imagethumbgenthread.h"
#include "imageutils.h"
#include "image.h"



/** \fn     Image::SetImageInfo( int id,
                                 const QString &tag,
                                 const QString &value )
 *  \brief  Saves the given value into the exif tag of the filename.
 *  \param  id The database id of the file
 *  \param  tag The tag that shall be overwritten
 *  \param  value The new value
 *  \return bool True when saving was successful, otherwise false
 */
bool Image::SetImageInfo( int id, const QString &tag,
                          const QString &value )
{
    ImageMetadata *im = new ImageMetadata();
    ImageUtils *iu = ImageUtils::getInstance();
    iu->LoadFileFromDB(im, id);

    if (im->m_fileName.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "SetImageInfo - File not found in DB.");
        delete im;
        return false;
    }

    // We got the file name from the ID, so use this method
    // which does the same but just on a filename basis.
    bool ok;
    ok = SetImageInfoByFileName( im->m_fileName, tag, value );

    delete im;
    return ok;
 }



/** \fn     Image::SetImageInfoByFileName( const QString &fileName,
                                           const QString &tag,
                                           const QString &value )
 *  \brief  Saves the given value into the exif tag of the filename.
 *  \param  fileName The full filename
 *  \param  tag The tag that shall be overwritten
 *  \param  value The new value
 *  \return bool True when saving was successful, otherwise false
 */
bool Image::SetImageInfoByFileName( const QString &fileName,
                                    const QString &tag,
                                    const QString &value )
{
    if (!QFile::exists( fileName ))
    {
        LOG(VB_GENERAL, LOG_ERR, "SetImageInfoByFileName - File does not exist.");
        return false;
    }

    if (tag.isEmpty() || value.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "SetImageInfoByFileName - Exif tag name or value is missing.");
        return false;
    }

    bool ok;
    ImageUtils *iu = ImageUtils::getInstance();
    iu->SetExifValue(fileName, tag, value, &ok);

    return ok;
}



/** \fn     Image::GetImageInfo( int id, const QString &tag )
 *  \brief  Returns the value of the specified exif tag from the image
            file. If the filename or exif tag do not
            exist or the tag has no contents, an empty value is returned.
 *  \param  id The database id of the file
 *  \param  tag The exif tag
 *  \return QString The exif tag value if successful, otherwise empty
 */
QString Image::GetImageInfo( int id, const QString &tag )
{
    ImageMetadata *im = new ImageMetadata();
    ImageUtils *iu = ImageUtils::getInstance();
    iu->LoadFileFromDB(im, id);

    if (im->m_fileName.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "GetImageInfo - File not found in DB.");
        delete im;
        return QString();
    }

    // We got the file name from the ID, so use this method
    // which does the same but just on a filename basis.
    QString value = GetImageInfoByFileName( im->m_fileName, tag );

    delete im;
    return value;
}



/** \fn     Image::GetImageInfoByFileName( const QString &fileName,
                                           const QString &tag )
 *  \brief  Returns the value of the specified exif tag from the image
            file. If the filename or exif tag do not
            exist or the tag has no contents, an empty value is returned.
 *  \param  fileName The full filename
 *  \param  tag The exif tag
 *  \return QString The exif tag value if successful, otherwise empty
 */
QString Image::GetImageInfoByFileName( const QString &fileName, const QString &tag )
{
    if (!QFile::exists( fileName ))
    {
        LOG(VB_GENERAL, LOG_ERR, "GetImageInfoByFileName - File does not exist.");
        return QString();
    }

    if (tag.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "GetImageInfoByFileName - Exif tag name is missing.");
        return QString();
    }

    bool ok;
    ImageUtils *iu = ImageUtils::getInstance();
    QString value = iu->GetExifValue(fileName, tag, &ok);

    if (!ok)
    {
        LOG(VB_GENERAL, LOG_ERR, "GetImageInfoByFileName - Could not read exif tag");
        return QString();
    }

    return value;
}



/** \fn     Image::GetImageInfoList(int id)
 *  \brief  Returns all values from all available exif tags
 *  \param  id The database id of the file
 *  \return DTC::ImageMetadataInfoList The list with all exif values
 */
DTC::ImageMetadataInfoList* Image::GetImageInfoList( int id )
{
    ImageMetadata *im = new ImageMetadata();
    ImageUtils *iu = ImageUtils::getInstance();
    iu->LoadFileFromDB(im, id);

    if (im->m_fileName.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "GetImageInfoList - File not found in DB");
        delete im;
        return NULL;
    }

    // We got the file name from the ID, so use this method
    // which does the same but just on a filename basis.
    DTC::ImageMetadataInfoList *imInfoList;
    imInfoList = GetImageInfoListByFileName(im->m_fileName);

    delete im;
    return imInfoList;
}



/** \fn     Image::GetImageInfoListByFileName(QString &fileName)
 *  \brief  Returns all values from all available exif tags
 *  \param  fileName The name of the file
 *  \return DTC::ImageMetadataInfoList The list with all exif values
 */
DTC::ImageMetadataInfoList* Image::GetImageInfoListByFileName( const QString &fileName )
{
    if (!QFile::exists(fileName))
    {
        LOG(VB_GENERAL, LOG_ERR, "GetImageInfoListByFileName - File does not exist.");
        return NULL;
    }

    // Read all available exif tag
    // values from the given image file
    ImageUtils *iu = ImageUtils::getInstance();
    QList<QStringList> valueList = iu->GetAllExifValues(fileName);

    if (valueList.size() == 0)
    {
        LOG(VB_GENERAL, LOG_ERR, "GetImageInfoListByFileName - Could not read exif tags");
        return NULL;
    }

    // This holds the xml data structure from
    // the returned stringlist with the exif data
    DTC::ImageMetadataInfoList *imInfoList = new DTC::ImageMetadataInfoList();

    // Set the general information of the image
    QFileInfo fi(fileName);
    imInfoList->setCount(valueList.size());
    imInfoList->setFile(fi.fileName());
    imInfoList->setPath(fi.path());
    imInfoList->setSize(fi.size());
    imInfoList->setExtension(fi.suffix());

    // The returned stringlist contents are
    // <familyname>, <groupname>, <tagname>, <taglabel>, <value>
    // Go through all list items and build the response. Create
    // a new tag and add the tagnames below it. Each tagname
    // has these children: family, group, name, label, value.
    for (int i = 0; i < valueList.size(); ++i)
    {
        QStringList values = valueList.at(i);

        DTC::ImageMetadataInfo *imInfo = imInfoList->AddNewImageMetadataInfo();

        imInfo->setNumber(  i);
        imInfo->setFamily(  values.at(0));
        imInfo->setGroup(   values.at(1));
        imInfo->setTag(     values.at(2));
        imInfo->setKey(     values.at(3));
        imInfo->setLabel(   values.at(4));
        imInfo->setValue(   values.at(5));
    }

    return imInfoList;
}



/** \fn     Image::RemoveImageFromDB(int id)
 *  \brief  Returns all values from all available exif tags
 *  \param  id The database id of the file
 *  \return bool True if successful, otherwise false
 */
bool Image::RemoveImageFromDB( int id )
{
    ImageUtils *iu = ImageUtils::getInstance();
    return iu->RemoveFileFromDB(id);
}



/** \fn     Image::RemoveImage(int id)
 *  \brief  Returns all values from all available exif tags
 *  \param  id The database id of the file
 *  \return bool True if successful, otherwise false
 */
bool Image::RemoveImage( int id )
{
    ImageMetadata *im = new ImageMetadata();
    ImageUtils *iu = ImageUtils::getInstance();
    iu->LoadFileFromDB(im, id);

    if (im->m_fileName.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "RemoveImage - File not found");
        delete im;
        return false;
    }

    if (!QFile::exists( im->m_fileName ))
    {
        LOG(VB_GENERAL, LOG_ERR, "RemoveImage - File does not exist.");
        delete im;
        return false;
    }

    if (!QFile::remove( im->m_fileName ))
    {
        LOG(VB_GENERAL, LOG_ERR, "RemoveImage - Could not delete file.");
        delete im;
        return false;
    }

    delete im;

    // Remove the database entry if the file has been deleted.
    return RemoveImageFromDB(id);
}



/** \fn     Image::StartSync(void)
 *  \brief  Starts the synchronization of the images with the database
 *  \return bool True if the sync has started, otherwise false
 */
bool Image::StartSync( void )
{
    // Check that the required image tables exist to avoid
    // syncing against non existent tables in the database.
    if (gCoreContext->GetNumSetting("DBSchemaVer") < 1318)
    {
        LOG(VB_GENERAL, LOG_INFO,
            "Sync cannot start, the required database tables are missing."
            "Please upgrade your database schema to at least 1318.");
        return false;
    }

    ImageScan *is = ImageScan::getInstance();
    if (!is->SyncIsRunning())
        is->StartSync();

    return is->SyncIsRunning();
}



/** \fn     Image::StopSync(void)
 *  \brief  Stops the image synchronization if its running
 *  \return bool True if the sync has stopped, otherwise false
 */
bool Image::StopSync( void )
{
    ImageScan *is = ImageScan::getInstance();
    if (is->SyncIsRunning())
        is->StopSync();

    return is->SyncIsRunning();
}



/** \fn     Image::GetSyncStatus(void)
 *  \brief  Returns a list with information if the synchronization is
            currently running, the already synchronized images and
            the total amount of images that shall be synchronized.
 *  \return DTC::ImageSyncInfo The status information
 */
DTC::ImageSyncInfo* Image::GetSyncStatus( void )
{
    DTC::ImageSyncInfo *syncInfo = new DTC::ImageSyncInfo();

    ImageScan *is = ImageScan::getInstance();

    LOG(VB_GENERAL, LOG_DEBUG,
        QString("Image: Sync status is running: %1, current: %2, total: %3")
        .arg(is->SyncIsRunning())
        .arg(is->GetCurrent())
        .arg(is->GetTotal()));

    syncInfo->setRunning(is->SyncIsRunning());
    syncInfo->setCurrent(is->GetCurrent());
    syncInfo->setTotal(is->GetTotal());

    return syncInfo;
}

/**
 *  \brief  Starts thumbnail generation thread for images
 *  \return bool True if the generation thread has started, otherwise false
 */
bool Image::StartThumbnailGeneration(void )
{
    // Check that the required image tables exist to avoid
    // syncing against non existent tables in the database.
    if (gCoreContext->GetNumSetting("DBSchemaVer") < 1318)
    {
        LOG(VB_GENERAL, LOG_INFO,
            "Thumbnail generation cannot start, the required database tables "
            "are missing. "
            "Please upgrade your database schema to at least 1318.");
        return false;
    }

    ImageThumbGen *thumbGen = ImageThumbGen::getInstance();
    if (!thumbGen->ThumbGenIsRunning())
        thumbGen->StartThumbGen();

    return thumbGen->ThumbGenIsRunning();
}


/**
 *  \brief  Stops the thumbnail generation if it's running
 *  \return bool True if the generation has stopped, otherwise false
 */
bool Image::StopThumbnailGeneration(void )
{
    ImageThumbGen *thumbGen = ImageThumbGen::getInstance();
    if (thumbGen->ThumbGenIsRunning())
        thumbGen->StopThumbGen();

    return !thumbGen->ThumbGenIsRunning();
}

bool Image::CreateThumbnail(int id)
{
    ImageMetadata *im = new ImageMetadata();
    ImageUtils *iu = ImageUtils::getInstance();
    iu->LoadFileFromDB(im, id);

    if (im->m_fileName.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "QueueCreateThumbnail - File not found");
        delete im;
        return false;
    }

    ImageThumbGen *thumbGen = ImageThumbGen::getInstance();
    return thumbGen->AddToThumbnailList(im);
}

bool Image::RecreateThumbnail(int id)
{
    ImageMetadata *im = new ImageMetadata();
    ImageUtils *iu = ImageUtils::getInstance();
    iu->LoadFileFromDB(im, id);

    if (im->m_fileName.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "QueueCreateThumbnail - File not found");
        delete im;
        return false;
    }

    ImageThumbGen *thumbGen = ImageThumbGen::getInstance();
    return thumbGen->RecreateThumbnail(im);
}

bool Image::SetThumbnailSize(int Width, int Height)
{
    ImageThumbGen *thumbGen = ImageThumbGen::getInstance();
    return thumbGen->SetThumbnailSize(Width, Height);
}
