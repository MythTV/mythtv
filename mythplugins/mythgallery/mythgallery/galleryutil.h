/* ============================================================
 * File  : exifutil.h
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

#ifndef EXIFUTIL_H
#define EXIFUTIL_H

#include "iconview.h"

class GalleryUtil
{

 public:
	static bool isImage(const char* filePath);
	static bool isMovie(const char* filePath);
	static long getNaturalRotation(const char* filePath);

    static bool loadDirectory(ThumbList& itemList,
                              const QString& dir, bool recurse,
                              ThumbDict *itemDict, ThumbGenerator* thumbGen);
};

#endif /* EXIFUTIL_H */
