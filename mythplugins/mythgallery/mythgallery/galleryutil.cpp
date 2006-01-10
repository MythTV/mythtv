/* ============================================================
 * File  : exifutil.cpp
 * Description : 
 * 

 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published bythe Free Software Foundation;
 * either version 2, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * ============================================================ */

#include <iostream>
#include <qfileinfo.h>
#include <qdir.h>

#include "config.h"
#include "constants.h"
#include "galleryutil.h"
#include "thumbgenerator.h"

#ifdef EXIF_SUPPORT
#include <libexif/exif-data.h>
#include <libexif/exif-entry.h>
// include "exif.hpp"
#endif // EXIF_SUPPORT

bool GalleryUtil::isImage(const char* filePath)
{
    QFileInfo fi(filePath);
    return !fi.isDir() && IMAGE_FILENAMES.find(fi.extension()) != -1;
}

bool GalleryUtil::isMovie(const char* filePath)
{
    QFileInfo fi(filePath);
    return !fi.isDir() && MOVIE_FILENAMES.find(fi.extension()) != -1;
}

long GalleryUtil::getNaturalRotation(const char* filePath)
{
    long rotateAngle = 0;

    try
    {
#ifdef EXIF_SUPPORT
        char *exifvalue = new char[1024];
        ExifData *data = exif_data_new_from_file (filePath);
        if (data)
        {
            for (int i = 0; i < EXIF_IFD_COUNT; i++)
            {
                    ExifEntry *entry = exif_content_get_entry (data->ifd[i],
                                                        EXIF_TAG_ORIENTATION);
                    if (entry)
                    {
#if NEW_LIB_EXIF
                        exif_entry_get_value(entry, exifvalue, 1023);
                        QString value = exifvalue;
#else
                        QString value = exif_entry_get_value(entry);
#endif
                        if (value == "left - bottom")
                        {
                          rotateAngle = -90;
                        }
                        else if (value == "right - top")
                        {
                          rotateAngle = 90;
                        }
                        break;
                    }
            }
            exif_data_free(data);
        }
        else
        {
                std::cerr << "Could not load exif data from " << filePath << std::endl;
        }
        
        delete [] exifvalue;
        
        /*
        Exiv2::ExifData exifData;
         int rc = exifData.read(filePath);
        if (!rc)
        {
            Exiv2::ExifKey key = Exiv2::ExifKey("Exif.Image.Orientation");
            Exiv2::ExifData::iterator pos = exifData.findKey(key);
            if (pos != exifData.end())
            {
                long orientation = pos->toLong();
                switch (orientation)
                {
                    case 6:
                        rotateAngle = 90;
                        break;
                    case 8:
                        rotateAngle = -90;
                        break;
                    default:
                        rotateAngle = 0;
                        break;
                }
            }
        }
        */
#endif // EXIF_SUPPORT
    }
    catch (...)
    {
        std::cerr << "GalleryUtil: Failed to extract EXIF headers from "
        << filePath << std::endl;
    }

    return rotateAngle;
}

bool GalleryUtil::loadDirectory(ThumbList& itemList,
                                const QString& dir, bool recurse,
                                ThumbDict *itemDict, ThumbGenerator* thumbGen)
{
    QString blah = dir;
    QDir d(blah);
    QString currDir = d.absPath();

    bool isGallery;
    const QFileInfoList* gList = d.entryInfoList("serial*.dat", QDir::Files);
    if (gList)
        isGallery = (gList->count() != 0);
    else
        isGallery = false;

    // Create .thumbcache dir if neccesary
    if (thumbGen)
        thumbGen->getThumbcacheDir(currDir);

    d.setNameFilter(MEDIA_FILENAMES);
    d.setSorting(QDir::Name | QDir::DirsFirst | QDir::IgnoreCase);

    d.setMatchAllDirs(true);
    const QFileInfoList *list = d.entryInfoList();
    if (!list)
        return false;

    QFileInfoListIterator it(*list);
    QFileInfo *fi;

    if (thumbGen) 
    {
        thumbGen->cancel();
        thumbGen->setDirectory(currDir, isGallery);
    }

    while ((fi = it.current()) != 0)
    {
        ++it;
        if (fi->fileName() == "." || fi->fileName() == "..")
            continue;

        // remove these already-resized pictures.
        if (isGallery && (
                (fi->fileName().find(".thumb.") > 0) ||
                (fi->fileName().find(".sized.") > 0) ||
                (fi->fileName().find(".highlight.") > 0)))
            continue;

        if (fi->isDir() && recurse) 
        {
            GalleryUtil::loadDirectory(itemList,
                                       QDir::cleanDirPath(fi->absFilePath()), true,
                                       itemDict, thumbGen);
        }
        else 
        {
            ThumbItem* item = new ThumbItem;
            item->name      = fi->fileName();
            item->path      = QDir::cleanDirPath(fi->absFilePath());
            item->isDir     = fi->isDir();

            itemList.append(item);

            if (itemDict)
                itemDict->insert(item->name, item);

            if (thumbGen)
                thumbGen->addFile(item->name);
        }
    }

    if (thumbGen && !thumbGen->running())
    {
        thumbGen->start();
    }

    return isGallery;
}

QString GalleryUtil::getCaption(const QString& filePath)
{
    QString caption("");

    try
    {
#ifdef EXIF_SUPPORT
        char *exifvalue = new char[1024];
        ExifData *data = exif_data_new_from_file (filePath);
        if (data)
        {
            for (int i = 0; i < EXIF_IFD_COUNT; i++)
            {
                ExifEntry *entry = exif_content_get_entry (data->ifd[i],
                                                    EXIF_TAG_USER_COMMENT);
                if (entry)
                {
#if NEW_LIB_EXIF
                    exif_entry_get_value(entry, exifvalue, 1023);
                    caption = exifvalue;
#else           
                    caption = exif_entry_get_value(entry);
#endif          
                    // Found one, done
                    if(!caption.isEmpty())
                       break;
                }

                entry = exif_content_get_entry (data->ifd[i],
                                                EXIF_TAG_IMAGE_DESCRIPTION);
                if (entry)
                {
#if NEW_LIB_EXIF
                    exif_entry_get_value(entry, exifvalue, 1023);
                    caption = exifvalue;
#else           
                    caption = exif_entry_get_value(entry);
#endif          
                    // Found one, done
                    if(!caption.isEmpty())
                       break;
                }
            }
            exif_data_free(data);
        }
        else
        {
           std::cerr << "Could not load exif data from " << filePath << std::endl;
        }
        
        delete [] exifvalue;
#endif // EXIF_SUPPORT
    }
    catch (...)
    {
        std::cerr << "GalleryUtil: Failed to extract EXIF headers from "
        << filePath << std::endl;
    }

    return caption;
}
