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

#include "config.h"
#include "constants.h"
#include "galleryutil.h"

#ifdef EXIF_SUPPORT
#include <libexif/exif-data.h>
#include <libexif/exif-entry.h>
// include "exif.hpp"
#endif // EXIF_SUPPORT

bool GalleryUtil::isImage(const char* filePath)
{
    QFileInfo fi(filePath);
    return IMAGE_FILENAMES.find(fi.extension()) != -1;
}

bool GalleryUtil::isMovie(const char* filePath)
{
    QFileInfo fi(filePath);
    return MOVIE_FILENAMES.find(fi.extension()) != -1;
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
