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

//#include "mythcorecontext.h"

#include "image.h"

#define LOC QString("ImageService: ")

/**
 *  \brief  Returns the value of the specified exif tag from the image
            file. If the filename or exif tag do not
            exist or the tag has no contents, an empty value is returned.
 *  \param  id The database id of the file
 *  \param  tag The exif tag
 *  \return QString The exif tag value if successful, otherwise empty
 */
QString Image::GetImageInfo( int /*id*/, const QString &/*tag*/ )
{
//    ImageManagerBe *mgr = ImageManagerBe::getInstance();

//    // Find image in DB
//    ImageList images;
//    if (mgr->GetFiles(images, QString::number(id)) != 1)
//    {
//        qDeleteAll(images);
//        LOG(VB_FILE, LOG_NOTICE, LOC + QString("Image %1 not found").arg(id));
//        return QString();
//    }

//    // Read all metadata tags
//    ImageAdapterBase::TagMap tags;
//    tags.insert(tag, qMakePair(QString(), QString()));

//    ImageItem *im = images[0];
//    bool found = mgr->HandleGetMetadata(id, tags);
//    delete im;

//    if (found)
//        return tags[tag].first;

//    LOG(VB_FILE, LOG_DEBUG, LOC + QString("Tag %1 not found for %2").arg(tag).arg(id));
    return QString();
}



/**
 *  \brief  Returns all values from all available exif tags
 *  \param  id The database id of the file
 *  \return DTC::ImageMetadataInfoList The list with all exif values
 */
DTC::ImageMetadataInfoList* Image::GetImageInfoList(int id)
{
    // This holds the xml data structure from
    // the returned stringlist with the exif data
    auto *imInfoList = new DTC::ImageMetadataInfoList();

    // Read all metadata tags
//    ImageManagerBe *mgr = ImageManagerBe::getInstance();
    QStringList tags; // = mgr->HandleGetMetadata(QString::number(id));

    if (tags.size() < 2 || tags[0] != "OK")
    {
        LOG(VB_FILE, LOG_NOTICE, LOC +
            QString("Image %1 - %2").arg(id).arg(tags.join(",")));
        return imInfoList;
    }
    tags.removeFirst();

    // Set the general information of the image
    imInfoList->setCount(tags.size());
//    imInfoList->setFile(im->m_filePath);
//    imInfoList->setPath(im->m_path);
//    imInfoList->setSize(im->m_size);
//    imInfoList->setExtension(im->m_extension);

    // Each string contains a Name<seperator>Label<seperator>Value.
    QString seperator = tags.takeFirst();
    int index = 0;
    foreach (const QString &token, tags)
    {
        QStringList parts = token.split(seperator);
        if (parts.size() != 3)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Bad Metadata received: '%1' (%2)").arg(token, seperator));
            continue;
        }
        DTC::ImageMetadataInfo *imInfo = imInfoList->AddNewImageMetadataInfo();

        imInfo->setNumber(index++);
        imInfo->setTag(parts[0]);
        imInfo->setLabel(parts[1]);
        imInfo->setValue(parts[2]);

#if DUMP_METADATA_TAGS
        LOG(VB_FILE, LOG_DEBUG, LOC +
            QString("Metadata %1 : %2 : '%3'").arg(parts[0], parts[1], parts[2]));
#endif
    }
    return imInfoList;
}



/*!
 \brief Deletes an image file or dir subtree from filesystem and database
 \param id Image
 \return bool True if deleted, false otherwise
*/
bool Image::RemoveImage( int id )
{
    QStringList result = ImageManagerBe::getInstance()->
            HandleDelete(QString::number(id));
    return result[0] == "OK";
}


/**
 *  \brief  Renames the file to the new name.
 *  \param  id The database id of the file
 *  \param  newName  The new name of the file (only the name, no path)
 *  \return bool True if renaming was successful, otherwise false
 */
bool Image::RenameImage( int id, const QString &newName)
{
    QStringList result = ImageManagerBe::getInstance()->
            HandleRename(QString::number(id), newName);
    return result[0] == "OK";
}


/**
 *  \brief  Starts the synchronization of the images with the database
 *  \return bool True if the sync has started, otherwise false
 */
bool Image::StartSync( void )
{
    QStringList result = ImageManagerBe::getInstance()->HandleScanRequest("START");
    return result.size() >= 2 && !result[1].isEmpty();
}



/**
 *  \brief  Stops the image synchronization if its running
 *  \return bool True if the sync has stopped, otherwise false
 */
bool Image::StopSync( void )
{
    QStringList result = ImageManagerBe::getInstance()->HandleScanRequest("STOP");
    return result.size() >= 2 && !result[1].isEmpty();
}



/**
 *  \brief  Returns a list with information if the synchronization is
            currently running, the already synchronized images and
            the total amount of images that shall be synchronized.
 *  \return DTC::ImageSyncInfo The status information
 */
DTC::ImageSyncInfo* Image::GetSyncStatus( void )
{
    // Default to No Scan
    bool running = false;
    int  current = 0;
    int  total   = 0;

    // Expects OK, scanner id, current#, total#
    QStringList result = ImageManagerBe::getInstance()->HandleScanRequest("QUERY");
    if (result.size() == 4 && result[0] == "OK")
    {
        current = result[2].toInt();
        total   = result[3].toInt();
        running = (current != total);
    }

    LOG(VB_GENERAL, LOG_DEBUG,
        QString("Image: Sync status is running: %1, current: %2, total: %3")
        .arg(running).arg(current).arg(total));

    auto *syncInfo = new DTC::ImageSyncInfo();
    syncInfo->setRunning(running);
    syncInfo->setCurrent(current);
    syncInfo->setTotal(total);

    return syncInfo;
}

/*!
 \brief Request creation of a thumbnail
 \param id Image
 \return bool True if image is valid
*/
bool Image::CreateThumbnail(int id)
{
    QStringList mesg("0");
    mesg << QString::number(id);
    ImageManagerBe::getInstance()->HandleCreateThumbnails(mesg);
    return true;
}
