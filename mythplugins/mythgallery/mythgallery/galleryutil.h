// -*- Mode: c++ -*-
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

    static QString getCaption(const QString &filePath);

    static bool loadDirectory(ThumbList& itemList,
                              const QString& dir, bool recurse,
                              ThumbDict *itemDict, ThumbGenerator* thumbGen);

    static bool CopyMove(const QFileInfo &src, QFileInfo &dst, bool move)
        { if (move) return Move(src, dst); else return Copy(src, dst); }

    static bool Copy(const QFileInfo &src, QFileInfo &dst);
    static bool Move(const QFileInfo &src, QFileInfo &dst);
    static bool Delete(const QFileInfo &file);

  private:
    static bool CopyDirectory(const QFileInfo src, QFileInfo &dst);
    static bool MoveDirectory(const QFileInfo src, QFileInfo &dst);
    static bool DeleteDirectory(const QFileInfo &dir);
};

#endif /* EXIFUTIL_H */
