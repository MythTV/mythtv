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

// qt
#include <QDir>

// myth
#include <mythcontext.h>
#include <mythdbcon.h>
#include <util.h>
#include <mythdirs.h>
#include <mythdb.h>
#include <mythuihelper.h>

// mythgallery
#include "config.h"
#include "galleryutil.h"
#include "thumbgenerator.h"

#ifdef DCRAW_SUPPORT
#include "../dcrawplugin/dcrawformats.h"
#endif // DCRAW_SUPPORT

#ifdef EXIF_SUPPORT
#include <libexif/exif-data.h>
#include <libexif/exif-entry.h>
// include "exif.hpp"
#endif // EXIF_SUPPORT

#define LOC QString("GalleryUtil:")
#define LOC_ERR QString("GalleryUtil, Error:")

static QFileInfo MakeUnique(const QFileInfo &dest);
static QFileInfo MakeUniqueDirectory(const QFileInfo &dest);
static bool FileCopy(const QFileInfo &src, const QFileInfo &dst);
static bool FileMove(const QFileInfo &src, const QFileInfo &dst);
static bool FileDelete(const QFileInfo &file);

QStringList GalleryUtil::GetImageFilter(void)
{
    QStringList filt;
    filt.push_back("*.jpg");
    filt.push_back("*.jpeg");
    filt.push_back("*.png");
    filt.push_back("*.tif");
    filt.push_back("*.tiff");
    filt.push_back("*.bmp");
    filt.push_back("*.gif");

#ifdef DCRAW_SUPPORT
    filt << DcrawFormats::getFilters();
#endif // DCRAW_SUPPORT

    return filt;
}

QStringList GalleryUtil::GetMovieFilter(void)
{
    QStringList filt;
    filt.push_back("*.avi");
    filt.push_back("*.mpg");
    filt.push_back("*.mp4");
    filt.push_back("*.mpeg");
    filt.push_back("*.mov");
    filt.push_back("*.wmv");
    return filt;
}

QStringList GalleryUtil::GetMediaFilter(void)
{
    QStringList filt = GetImageFilter();
    filt << GetMovieFilter();
    return filt;
}

bool GalleryUtil::IsImage(const QString &filePath)
{
    QFileInfo fi(filePath);
    if (fi.isDir())
        return false;

    QStringList filt = GetImageFilter();
    QStringList::const_iterator it = filt.begin();
    for (; it != filt.end(); ++it)
    {
        if ((*it).toLower().contains(fi.suffix()))
            return true;
    }

    return false;
}

bool GalleryUtil::IsMovie(const QString &filePath)
{
    QFileInfo fi(filePath);
    if (fi.isDir())
        return false;

    QStringList filt = GetMovieFilter();
    QStringList::const_iterator it = filt.begin();
    for (; it != filt.end(); ++it)
    {
        if ((*it).toLower().contains(fi.suffix()))
            return true;
    }

    return false;
}

long GalleryUtil::GetNaturalRotation(const QString &filePathString)
{
    long rotateAngle = 0;
    QByteArray filePathBA = filePathString.toLocal8Bit();
    const char *filePath = filePathBA.constData();

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
                ExifByteOrder byteorder = exif_data_get_byte_order (data);

                if (entry)
                {
                    ExifShort v_short = exif_get_short (entry->data, byteorder);
                    VERBOSE(VB_GENERAL|VB_EXTRA, QString("Exif entry=%1").arg(v_short));
                    /* See http://sylvana.net/jpegcrop/exif_orientation.html*/
                    if (v_short == 8)
                    {
                        rotateAngle = -90;
                    }
                    else if (v_short == 6)
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
            VERBOSE(VB_FILE, LOC_ERR +
                    QString("Could not load exif data from '%1'")
                    .arg(filePath));
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
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Failed to extract EXIF headers from '%1'")
                .arg(filePathString));
    }

    return rotateAngle;
}

bool GalleryUtil::LoadDirectory(ThumbList& itemList, const QString& dir,
                                int sortorder, bool recurse,
                                ThumbHash *itemHash, ThumbGenerator* thumbGen)
{
    QString blah = dir;
    QDir d(blah);
    QString currDir = d.absolutePath();

    bool isGallery;
    QFileInfoList gList = d.entryInfoList(QStringList("serial*.dat"),
                                          QDir::Files);
    isGallery = (gList.count() != 0);

    // Create .thumbcache dir if neccesary
    if (thumbGen)
        thumbGen->getThumbcacheDir(currDir);

    QFileInfoList list = d.entryInfoList(GetMediaFilter(),
                                         QDir::Files | QDir::AllDirs,
                                         (QDir::SortFlag)sortorder);

    if (list.isEmpty())
        return false;

    QFileInfoList::const_iterator it = list.begin();
    const QFileInfo *fi;

    if (thumbGen) 
    {
        thumbGen->cancel();
        thumbGen->setDirectory(currDir, isGallery);
    }

    while (it != list.end())
    {
        fi = &(*it);
        ++it;
        if (fi->fileName() == "." || fi->fileName() == "..")
            continue;

        // remove these already-resized pictures.
        if (isGallery && (
                (fi->fileName().indexOf(".thumb.") > 0) ||
                (fi->fileName().indexOf(".sized.") > 0) ||
                (fi->fileName().indexOf(".highlight.") > 0)))
            continue;

        if (fi->isDir() && recurse) 
        {
            GalleryUtil::LoadDirectory(
                itemList, QDir::cleanPath(fi->absoluteFilePath()),
                sortorder, true, itemHash, thumbGen);
        }
        else 
        {
            ThumbItem *item = new ThumbItem(
                fi->fileName(),
                QDir::cleanPath(fi->absoluteFilePath()), fi->isDir());

            itemList.append(item);

            if (itemHash)
                itemHash->insert(item->GetName(), item);

            if (thumbGen)
                thumbGen->addFile(item->GetName());
        }
    }

    return isGallery;
}

QString GalleryUtil::GetCaption(const QString &filePath)
{
    QString caption("");

    try
    {
#ifdef EXIF_SUPPORT
        char *exifvalue = new char[1024];
        ExifData *data = exif_data_new_from_file(
            filePath.toLocal8Bit().constData());
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
                    if(!caption.trimmed().isEmpty())
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
                    if(!caption.trimmed().isEmpty())
                       break;
                }
            }
            exif_data_free(data);
        }
        else
        {
           VERBOSE(VB_FILE, LOC_ERR +
                   QString("Could not load exif data from '%1'")
                   .arg(filePath));
        }

        delete [] exifvalue;
#endif // EXIF_SUPPORT
    }
    catch (...)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Failed to extract EXIF headers from '%1'")
                .arg(filePath));
    }

    return caption;
}

bool GalleryUtil::Copy(const QFileInfo &src, QFileInfo &dst)
{
    if (src.isDir())
        return CopyDirectory(src, dst);

    dst = MakeUnique(dst);

    if (!FileCopy(src, dst))
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("INSERT INTO gallerymetadata (image, angle) "
                  "SELECT :IMAGENEW , angle "
                  "FROM gallerymetadata "
                  "WHERE image = :IMAGEOLD");
    query.bindValue(":IMAGENEW", dst.absoluteFilePath());
    query.bindValue(":IMAGEOLD", src.absoluteFilePath());
    if (query.exec())
        return true;

    // try to undo copy on DB failure
    FileDelete(dst);
    return false;
}

bool GalleryUtil::Move(const QFileInfo &src, QFileInfo &dst)
{
    if (src.isDir())
        return MoveDirectory(src, dst);

    dst = MakeUnique(dst);

    if (!FileMove(src, dst))
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE gallerymetadata "
                  "SET image = :IMAGENEW "
                  "WHERE image = :IMAGEOLD");
    query.bindValue(":IMAGENEW", dst.absoluteFilePath());
    query.bindValue(":IMAGEOLD", src.absoluteFilePath());
    if (query.exec())
        return true;

    // try to undo move on DB failure
    FileMove(dst, src);
    return false;
}

bool GalleryUtil::Delete(const QFileInfo &file)
{
    if (!file.exists())
        return false;

    if (file.isDir())
        return DeleteDirectory(file);

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM gallerymetadata "
                  "WHERE image = :IMAGE ;");
    query.bindValue(":IMAGE", file.absoluteFilePath());
    if (query.exec())
        return FileDelete(file);

    return false;
}

bool GalleryUtil::Rename(const QString &currDir, const QString &oldName,
                         const QString &newName)
{
    // make sure there isn't already a file/directory with the same name
    QFileInfo fi(currDir + '/' + newName);
    if (fi.exists())
        return false;

    fi.setFile(currDir + '/' + oldName);
    if (fi.isDir())
        return RenameDirectory(currDir, oldName, newName);

    // rename the file
    QDir cdir(currDir);
    if (!cdir.rename(oldName, newName))
        return false;

    // rename the file's thumbnail if it exists
    if (QFile::exists(currDir + "/.thumbcache/" + oldName))
    {
        QDir d(currDir + "/.thumbcache/");
        d.rename(oldName, newName);
    }

    int prefixLen = gCoreContext->GetSetting("GalleryDir").length();
    QString path = GetConfDir() + "/MythGallery";
    path += currDir.right(currDir.length() - prefixLen);
    path += QString("/.thumbcache/");
    if (QFile::exists(path + oldName))
    {
        QDir d(path);
        d.rename(oldName, newName);
    }

    // fix up the metadata in the database
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("UPDATE gallerymetadata "
                  "SET image = :IMAGENEW "
                  "WHERE image = :IMAGEOLD");
    query.bindValue(":IMAGENEW", QString(currDir + '/' + newName));
    query.bindValue(":IMAGEOLD", QString(currDir + '/' + oldName));
    if (query.exec())
        return true;

    // try to undo rename on DB failure
    cdir.rename(newName, oldName);
    return false;
}

QSize GalleryUtil::ScaleToDest(const QSize &src, const QSize &dest, ScaleMax scaleMax)
{
    QSize sz = src;

    // calculate screen pixel aspect ratio
    double pixelAspect = GetMythUI()->GetPixelAspectRatio();

    // calculate image aspect ratio
    double imageAspect = 1.0;
    if ((sz.width() > 0) && (sz.height() > 0))
        imageAspect = (double)sz.width() / (double)sz.height();

    int scaleWidth = sz.width();
    int scaleHeight = sz.height();

    switch (scaleMax)
    {
    case kScaleToFill:
        // scale-max to dest width for most images
        scaleWidth = dest.width();
        scaleHeight = (int)((float)dest.width() * pixelAspect / imageAspect);
        if (scaleHeight < dest.height())
        {
            // scale-max to dest height for extra wide images
            scaleWidth = (int)((float)dest.height() * imageAspect / pixelAspect);
            scaleHeight = dest.height();
        }
        break;

    case kReduceToFit:
        // Reduce to fit (but never enlarge)
        if (scaleWidth <= dest.width() && scaleHeight <= dest.height())
            break;
        // Fall through

    case kScaleToFit:
        // scale-min to dest height for most images
        scaleWidth = (int)((float)dest.height() * imageAspect / pixelAspect);
        scaleHeight = dest.height();
        if (scaleWidth > dest.width())
        {
            // scale-min to dest width for extra wide images
            scaleWidth = dest.width();
            scaleHeight = (int)((float)dest.width() * pixelAspect / imageAspect);
        }
        break;

    default:
        break;
    }

    if (scaleWidth != sz.width() || scaleHeight != sz.height())
        sz.scale(scaleWidth, scaleHeight, Qt::KeepAspectRatio);
    return sz;
}

bool GalleryUtil::CopyDirectory(const QFileInfo src, QFileInfo &dst)
{
    QDir srcDir(src.absoluteFilePath());

    dst = MakeUniqueDirectory(dst);
    if (!dst.exists())
    {
        srcDir.mkdir(dst.absoluteFilePath());
        dst.refresh();
    }

    if (!dst.exists() || !dst.isDir())
        return false;

    bool ok = true;
    QDir dstDir(dst.absoluteFilePath());
    QFileInfoList list = srcDir.entryInfoList();
    QFileInfoList::const_iterator it = list.begin();
    for (; it != list.end(); ++it)
    {
        const QString fn = it->fileName();
        if (fn != "." && fn != "..")
        {
            QFileInfo dfi(dstDir, fn);
            ok &= Copy(*it, dfi);
        }
    }

    return ok;
}

bool GalleryUtil::MoveDirectory(const QFileInfo src, QFileInfo &dst)
{
    QDir srcDir(src.absoluteFilePath());

    dst = MakeUniqueDirectory(dst);
    if (!dst.exists())
    {
        srcDir.mkdir(dst.absoluteFilePath());
        dst.refresh();
    }

    if (!dst.exists() || !dst.isDir())
        return false;

    bool ok = true;
    QDir dstDir(dst.absoluteFilePath());
    QFileInfoList list = srcDir.entryInfoList();
    QFileInfoList::const_iterator it = list.begin();
    for (; it != list.end(); ++it)
    {
        const QString fn = it->fileName();
        if (fn != "." && fn != "..")
        {
            QFileInfo dfi(dstDir, fn);
            ok &= Move(*it, dfi);
        }
    }

    return ok && FileDelete(src);
}

bool GalleryUtil::DeleteDirectory(const QFileInfo &dir)
{
    if (!dir.exists())
        return false;

    QDir srcDir(dir.absoluteFilePath());
    QFileInfoList list = srcDir.entryInfoList();
    QFileInfoList::const_iterator it = list.begin();
    for (; it != list.end(); ++it)
    {
        const QString fn = it->fileName();
        if (fn != "." && fn != "..")
            Delete(*it);
    }

    return FileDelete(dir);
}

bool GalleryUtil::RenameDirectory(const QString &currDir, const QString &oldName, 
                                const QString &newName)
{
    // rename the directory
    QDir cdir(currDir);
    if (!cdir.rename(oldName, newName))
        return false;

    // rename the directory's thumbnail if it exists in the parent directory
    if (QFile::exists(currDir + "/.thumbcache/" + oldName))
    {
        QDir d(currDir + "/.thumbcache/");
        d.rename(oldName, newName);
    }

    // also look in HOME directory for any thumbnails
    int prefixLen = gCoreContext->GetSetting("GalleryDir").length();
    QString path = GetConfDir() + "/MythGallery";
    path += currDir.right(currDir.length() - prefixLen) + '/';
    if (QFile::exists(path + oldName))
    {
        QDir d(path);
        d.rename(oldName, newName);

        // rename this directory's thumbnail
        path += QString(".thumbcache/");
        if (QFile::exists(path + oldName))
        {
            QDir d(path);
            d.rename(oldName, newName);
        }
    }

    // fix up the metadata in the database
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT image, angle FROM gallerymetadata "
                  "WHERE image LIKE :IMAGEOLD");
    query.bindValue(":IMAGEOLD", QString(currDir + '/' + oldName + '%'));
    if (query.exec())
    {
        while (query.next())
        {
            QString oldImage = query.value(0).toString();
            QString newImage = oldImage;
            newImage = newImage.replace(currDir + '/' + oldName,
                                        currDir + '/' + newName);

            MSqlQuery subquery(MSqlQuery::InitCon());
            subquery.prepare("UPDATE gallerymetadata "
                        "SET image = :IMAGENEW "
                        "WHERE image = :IMAGEOLD");
            subquery.bindValue(":IMAGENEW", newImage);
            subquery.bindValue(":IMAGEOLD", oldImage);
            if (!subquery.exec())
                MythDB::DBError("GalleryUtil::RenameDirectory - update image",
                                subquery);
        }
    }

    return true;
}

static QFileInfo MakeUnique(const QFileInfo &dest)
{
    QFileInfo newDest = dest;

    for (uint i = 0; newDest.exists(); i++)
    {
        QString basename = QString("%1_%2.%3")
            .arg(dest.baseName()).arg(i).arg(dest.completeSuffix());

        newDest.setFile(dest.dir(), basename);

        VERBOSE(VB_GENERAL, LOC_ERR +
                QString("Need to find a new name for '%1' trying '%2'")
                .arg(dest.absoluteFilePath()).arg(newDest.absoluteFilePath()));
    }

    return newDest;
}

static QFileInfo MakeUniqueDirectory(const QFileInfo &dest)
{
    QFileInfo newDest = dest;

    for (uint i = 0; newDest.exists() && !newDest.isDir(); i++)
    {
        QString fullname = QString("%1_%2").arg(dest.absoluteFilePath()).arg(i);
        newDest.setFile(fullname);

        VERBOSE(VB_GENERAL, LOC_ERR +
                QString("Need to find a new name for '%1' trying '%2'")
                .arg(dest.absoluteFilePath()).arg(newDest.absoluteFilePath()));
    }

    return newDest;
}

static bool FileCopy(const QFileInfo &src, const QFileInfo &dst)
{
    const int bufferSize = 16*1024;

    QFile s(src.absoluteFilePath());
    QFile d(dst.absoluteFilePath());
    char buffer[bufferSize];
    int len;

    if (!s.open(QIODevice::ReadOnly))
        return false;

    if (!d.open(QIODevice::WriteOnly))
    {
        s.close();
        return false;
    }

    len = s.read(buffer, bufferSize);
    do
    {
        d.write(buffer, len);
        len = s.read(buffer, bufferSize);
    } while (len > 0);

    s.close();
    d.close();

    return true;
}

static bool FileMove(const QFileInfo &src, const QFileInfo &dst)
{
    // attempt to rename the file,
    // this will fail if files are on different partitions
    QByteArray source = src.absoluteFilePath().toLocal8Bit();
    QByteArray dest   = dst.absoluteFilePath().toLocal8Bit();
    if (rename(source.constData(), dest.constData()) == 0)
    {
        return true;
    }

    // src and dst are on different mount points, move manually.
    if (errno == EXDEV)
    {
        if (FileCopy(src, dst))
            return FileDelete(src);
    }

    return false;
}

static bool FileDelete(const QFileInfo &file)
{
    if (!file.isDir())
        return QFile::remove(file.absoluteFilePath());

    // delete .thumbcache
    QDir srcDir(file.absoluteFilePath());
    QFileInfo tc(srcDir, ".thumbcache");
    GalleryUtil::Delete(tc);

    srcDir.rmdir(srcDir.absolutePath());

    return true;
}
