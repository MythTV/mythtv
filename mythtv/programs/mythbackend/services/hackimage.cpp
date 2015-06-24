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

//#include "mythcorecontext.h"
//#include "storagegroup.h"

//#include <imagemetadata.h>
//#include "imagescanner.h"
//#include "imagethumbs.h"
//#include "imageutils.h"
#include "image.h"
#include <imagehandlers.h>

//QString Image::GetImage(int id, ImageItem* im, const QString &function)
//{
//    QString imageFileName = QString();

//    ImageUtils *iu = ImageUtils::getInstance();
//    iu->LoadFileFromDB(*im, id);

//    if (im->m_fileName.isEmpty())

//        LOG(VB_GENERAL, LOG_ERR, QString("%1 - Image %2 not found in DB.")
//            .arg(function)
//            .arg(id));
//    else
//    {
//        QString sgName = IMAGE_STORAGE_GROUP;
//        StorageGroup sg = StorageGroup(sgName, gCoreContext->GetHostName());
//        imageFileName = sg.FindFile(im->m_fileName);

//        if (imageFileName.isEmpty())

//            LOG(VB_GENERAL, LOG_ERR,
//                QString("%1 - Storage Group file %2 not found for image %3")
//                .arg(function)
//                .arg(im->m_fileName)
//                .arg(id));
//    }
//    return imageFileName;
//}


///** \fn     Image::SetImageInfo( int id,
//                                 const QString &tag,
//                                 const QString &value )
// *  \brief  Saves the given value into the exif tag of the filename.
// *  \param  id The database id of the file
// *  \param  tag The tag that shall be overwritten
// *  \param  value The new value
// *  \return bool True when saving was successful, otherwise false
// */
//bool Image::SetImageInfo( int id, const QString &tag, const QString &value )
//{
//    ImageItem *im = new ImageItem();
//    QString fileName = GetImage(id, im, QString("SetImageInfo"));
//    delete im;

//    if (fileName.isEmpty())

//        return false;

//    // We got the file name from the ID, so use this method
//    // which does the same but just on a filename basis.
//    return SetImageInfoByFileName(fileName, tag, value);
// }



///** \fn     Image::SetImageInfoByFileName( const QString &fileName,
//                                           const QString &tag,
//                                           const QString &value )
// *  \brief  Saves the given value into the exif tag of the filename.
// *  \param  fileName The full filename
// *  \param  tag The tag that shall be overwritten
// *  \param  value The new value
// *  \return bool True when saving was successful, otherwise false
// */
//bool Image::SetImageInfoByFileName( const QString &fileName,
//                                    const QString &tag,
//                                    const QString &value )
//{
//    if (!QFile::exists( fileName ))
//    {
//        LOG(VB_GENERAL, LOG_ERR, "SetImageInfoByFileName - File does not exist.");
//        return false;
//    }

//    if (tag.isEmpty() || value.isEmpty())
//    {
//        LOG(VB_GENERAL, LOG_ERR, "SetImageInfoByFileName - Exif tag name or value is missing.");
//        return false;
//    }

//    bool ok;
//    ImageUtils *iu = ImageUtils::getInstance();
//    iu->SetExifValue(fileName, tag, value, &ok);

//    return false;
//}



/**
 *  \brief  Returns the value of the specified metadata tag from the image
            file. If the filename or metadata do not exist, an empty value is returned.
 *  \param  id The database id of the file
 *  \param  tag The tag name, as per Exif 2.2 standard tag names,
 *          see http://www.exiv2.org/tags.html. For videos the Exif orientation and
 *          datetime tag names are recognised.
 *  \return QString The tag value if successful, otherwise empty
 */
QString Image::GetImageInfo( int id, const QString &tag )
{
    ImageList items;
    ImageDbWriter db;
    db.ReadDbFilesById(items, QString::number(id));

    if (items.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("GetImageInfo: Image %1 not found.")
            .arg(id));
        return QString();
    }

    ImageMetaData::TagMap tags;
    tags.insert(tag, qMakePair(QString(), QString()));

    if (!ImageMetaData::GetMetaData(items[0], tags))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("GetImageInfo: Tag %1 not found for image %2.")
            .arg(tag).arg(id));
        qDeleteAll(items);
        return QString();
    }

    qDeleteAll(items);
    return tags[tag].first;
}



///** \fn     Image::GetImageInfoByFileName( const QString &fileName,
//                                           const QString &tag )
// *  \brief  Returns the value of the specified exif tag from the image
//            file. If the filename or exif tag do not
//            exist or the tag has no contents, an empty value is returned.
// *  \param  fileName The full filename
// *  \param  tag The exif tag
// *  \return QString The exif tag value if successful, otherwise empty
// */
//QString Image::GetImageInfoByFileName( const QString &fileName, const QString &tag )
//{
//    if (!QFile::exists( fileName ))
//    {
//        LOG(VB_GENERAL, LOG_ERR, "GetImageInfoByFileName - File does not exist.");
//        return QString();
//    }

//    if (tag.isEmpty())
//    {
//        LOG(VB_GENERAL, LOG_ERR, "GetImageInfoByFileName - Exif tag name is missing.");
//        return QString();
//    }

//    bool ok;
//    ImageUtils *iu = ImageUtils::getInstance();
//    QString value = iu->GetExifValue(fileName, tag, &ok);

//    if (!ok)
//    {
//        LOG(VB_GENERAL, LOG_ERR, "GetImageInfoByFileName - Could not read exif tag");
//        return QString();
//    }

//    return value;
//    return QString();
//}



/**
 *  \brief  Returns all metadata tags
 *  \param  id The database id of the file
 *  \return DTC::ImageMetadataInfoList The list of metadata tags
 */
DTC::ImageMetadataInfoList* Image::GetImageInfoList( int id )
{
    ImageList items;
    ImageDbWriter db;
    db.ReadDbFilesById(items, QString::number(id));

    if (items.size() != 1)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("GetImageInfo: Image %1 not found.")
            .arg(id));
        return NULL;
    }

    ImageItem *im = items[0];
    ImageMetaData::TagMap tags;
    if (!ImageMetaData::GetMetaData(im, tags))
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("GetImageInfo - Could not read metadata for %1")
            .arg(im->m_fileName));
        return NULL;
    }

    // This holds the xml data structure from
    // the returned stringlist with the exif data
    DTC::ImageMetadataInfoList *imInfoList = new DTC::ImageMetadataInfoList();

    // Set the general information of the image
    imInfoList->setCount(tags.size());
    imInfoList->setFile(im->m_fileName);
    imInfoList->setPath(im->m_path);
    imInfoList->setSize(im->m_size);
    imInfoList->setExtension(im->m_extension);

    // Each property is described by a pair of <tagvalue> : <taglabel>
    int index = 0;
    foreach (const QString &key, tags.keys())
    {
        DTC::ImageMetadataInfo *imInfo = imInfoList->AddNewImageMetadataInfo();

        imInfo->setNumber( index++);
        imInfo->setTag(    key);
        imInfo->setLabel(  tags[key].second);
        imInfo->setValue(  tags[key].first);
    }

    return imInfoList;
}


bool Image::RemoveImage( int id )
{
    QStringList result = ImageHandler::HandleDelete(QString::number(id));
    return result[0] == "OK";
}


bool Image::RenameImage( int id, const QString &newName)
{
    QStringList result = ImageHandler::HandleRename(QString::number(id),
                                                         newName);
    return result[0] == "OK";
}


bool Image::MoveImage(int id, int destinationDir)
{
    QStringList result = ImageHandler::HandleMove(QString::number(destinationDir),
                                                  QString::number(id));
    return result[0] == "OK";
}


/** \fn     Image::StartSync(void)
 *  \brief  Starts the synchronization of the images with the database
 *  \return bool True if the sync has started, otherwise false
 */
bool Image::StartSync( void )
{
    // TODO
    // Check that the required image tables exist to avoid
    // syncing against non existent tables in the database.
//    if (gCoreContext->GetNumSetting("DBSchemaVer") < 1318)
//    {
//        LOG(VB_GENERAL, LOG_INFO,
//            "Sync cannot start, the required database tables are missing."
//            "Please upgrade your database schema to at least 1318.");
//        return false;
//    }

//    ImageScan *is = ImageScan::getInstance();
//    if (!is->SyncIsRunning())
//        is->StartSync();

//    return is->SyncIsRunning();
    return false;
}



/** \fn     Image::StopSync(void)
 *  \brief  Stops the image synchronization if its running
 *  \return bool True if the sync has stopped, otherwise false
 */
bool Image::StopSync( void )
{
    // TODO
//    ImageScan *is = ImageScan::getInstance();
//    if (is->SyncIsRunning())
//        is->StopSync();

//    return is->SyncIsRunning();
    return false;
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

    // TODO
//    ImageScan *is = ImageScan::getInstance();

//    LOG(VB_GENERAL, LOG_DEBUG,
//        QString("Image: Sync status is running: %1, current: %2, total: %3")
//        .arg(is->SyncIsRunning())
//        .arg(is->GetCurrent())
//        .arg(is->GetTotal()));

//    syncInfo->setRunning(is->SyncIsRunning());
//    syncInfo->setCurrent(is->GetCurrent());
//    syncInfo->setTotal(is->GetTotal());

    return syncInfo;
}

bool Image::CreateThumbnail(int id, bool recreate)
{
    // TODO
//    ImageItem *im = new ImageItem();
//    ImageUtils *iu = ImageUtils::getInstance();
//    iu->LoadFileFromDB(*im, id);

//    if (im->m_fileName.isEmpty())
//    {
//        LOG(VB_GENERAL, LOG_ERR,
//            QString("CreateThumbnail - Image %1 not found in DB").arg(id));
//        delete im;
//        return false;
//    }

//    ImageThumbGen *thumbGen = ImageThumbGen::getInstance();
//    return thumbGen->AddToThumbnailList(im, recreate);
    return false;
}
