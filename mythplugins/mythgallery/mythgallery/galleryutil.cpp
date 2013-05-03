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
#include <QApplication>
#include <QImageReader>

// myth
#include <mythcontext.h>
#include <mythdbcon.h>
#include <mythdate.h>
#include <mythdirs.h>
#include <mythdb.h>
#include <mythuihelper.h>
#include <mythmainwindow.h>

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
#endif // EXIF_SUPPORT

#define LOC QString("GalleryUtil:")

static QFileInfo MakeUnique(const QFileInfo &dest);
static QFileInfo MakeUniqueDirectory(const QFileInfo &dest);
static bool FileCopy(const QFileInfo &src, const QFileInfo &dst);
static bool FileMove(const QFileInfo &src, const QFileInfo &dst);
static bool FileDelete(const QFileInfo &file);

QStringList GalleryUtil::GetImageFilter(void)
{
    QStringList filt;

    Q_FOREACH(QByteArray format, QImageReader::supportedImageFormats())
        filt.push_back("*." + format);

    filt.push_back("*.tif");

#ifdef DCRAW_SUPPORT
    filt << DcrawFormats::getFilters();
#endif // DCRAW_SUPPORT

    return filt;
}

QStringList GalleryUtil::GetMovieFilter(void)
{
    QStringList filt;
    filt.push_back("*.avi");
    filt.push_back("*.bin");
    filt.push_back("*.iso");
    filt.push_back("*.img");
    filt.push_back("*.mpg");
    filt.push_back("*.mp4");
    filt.push_back("*.mpeg");
    filt.push_back("*.mov");
    filt.push_back("*.wmv");
    filt.push_back("*.3gp");
    filt.push_back("*.wmv");
    filt.push_back("*.flv");
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
        if ((*it).contains(fi.suffix().toLower()))
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
        if ((*it).contains(fi.suffix().toLower()))
            return true;
    }

    return false;
}

long GalleryUtil::GetNaturalRotation(const unsigned char *buffer, int size)
{
    long rotateAngle = 0;

#ifdef EXIF_SUPPORT
    try
    {
        ExifData *data = exif_data_new_from_data(buffer, size);
        if (data)
        {
            rotateAngle = GetNaturalRotation(data);
            exif_data_free(data);
        }
        else
        {
            LOG(VB_FILE, LOG_ERR, LOC + "Could not load exif data from buffer");
        }
    }
    catch (...)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Failed to extract EXIF headers from buffer");
    }
#else
    // Shut the compiler up about the unused argument
    (void)buffer;
    (void)size;
#endif

    return rotateAngle;
}

long GalleryUtil::GetNaturalRotation(const QString &filePathString)
{
    long rotateAngle = 0;

#ifdef EXIF_SUPPORT
    QByteArray filePathBA = filePathString.toLocal8Bit();
    const char *filePath = filePathBA.constData();

    try
    {
        ExifData *data = exif_data_new_from_file(filePath);
        if (data)
        {
            rotateAngle = GetNaturalRotation(data);
            exif_data_free(data);
        }
        else
        {
            LOG(VB_FILE, LOG_ERR, LOC +
                QString("Could not load exif data from '%1'") .arg(filePath));
        }
    }
    catch (...)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Failed to extract EXIF headers from '%1'") .arg(filePath));
    }
#else
    // Shut the compiler up about the unused argument
    (void)filePathString;
#endif

    return rotateAngle;
}

long GalleryUtil::GetNaturalRotation(void *exifData)
{
    long rotateAngle = 0;

#ifdef EXIF_SUPPORT
    ExifData *data = (ExifData *)exifData;

    if (!data)
        return 0;

    for (int i = 0; i < EXIF_IFD_COUNT; i++)
    {
        ExifEntry *entry = exif_content_get_entry(data->ifd[i],
                                                  EXIF_TAG_ORIENTATION);
        ExifByteOrder byteorder = exif_data_get_byte_order(data);

        if (entry)
        {
            ExifShort v_short = exif_get_short(entry->data, byteorder);
            LOG(VB_GENERAL, LOG_DEBUG,
                QString("Exif entry=%1").arg(v_short));

            /* See http://sylvana.net/jpegcrop/exif_orientation.html*/
            switch (v_short)
            {
                case 3:
                    rotateAngle = 180;
                    break;
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
            break;
        }
    }
#else
    // Shut the compiler up about the unused argument
    (void)exifData;
#endif // EXIF_SUPPORT

    return rotateAngle;
}

bool GalleryUtil::LoadDirectory(ThumbList& itemList, const QString& dir,
                                const GalleryFilter& flt, bool recurse,
                                ThumbHash *itemHash, ThumbGenerator* thumbGen)
{
    QString blah = dir;
    QDir d(blah);
    QString currDir = d.absolutePath();
    QStringList splitFD;

    bool isGallery;
    QFileInfoList gList = d.entryInfoList(QStringList("serial*.dat"),
                                          QDir::Files);
    isGallery = (gList.count() != 0);

    // Create .thumbcache dir if neccesary
    if (thumbGen)
        thumbGen->getThumbcacheDir(currDir);
    
    QFileInfoList list = d.entryInfoList(GetMediaFilter(),
                                         QDir::Files | QDir::AllDirs |
                                         QDir::NoDotAndDotDot,
                                         (QDir::SortFlag)flt.getSort());

    if (list.isEmpty())
        return false;

    QFileInfoList::const_iterator it = list.begin();
    const QFileInfo *fi;

    if (thumbGen)
    {
        thumbGen->cancel();
        thumbGen->setDirectory(currDir, isGallery);
    }

    if (!flt.getDirFilter().isEmpty())
    {
        splitFD = flt.getDirFilter().split(":");
    }

    while (it != list.end())
    {
        fi = &(*it);
        ++it;

        // remove these already-resized pictures.
        if (isGallery && (
                (fi->fileName().indexOf(".thumb.") > 0) ||
                (fi->fileName().indexOf(".sized.") > 0) ||
                (fi->fileName().indexOf(".highlight.") > 0)))
            continue;

        // skip filtered directory
        if (fi->isDir() &&
             !splitFD.filter(fi->fileName(), Qt::CaseInsensitive).isEmpty())
            continue;

        if (fi->isDir() && recurse)
        {
            LoadDirectory(itemList, QDir::cleanPath(fi->absoluteFilePath()),
                          flt, true, itemHash, thumbGen);
        }
        else
        {
            if ((GalleryUtil::IsImage(fi->absoluteFilePath()) &&
                 flt.getTypeFilter() == kTypeFilterMoviesOnly) ||
                (GalleryUtil::IsMovie(fi->absoluteFilePath()) &&
                 flt.getTypeFilter() == kTypeFilterImagesOnly))
                continue;

            ThumbItem *item = new ThumbItem(fi->fileName(),
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
#if NEW_LIB_EXIF
        char *exifvalue = new char[1024];
#endif
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
           LOG(VB_FILE, LOG_ERR, LOC +
               QString("Could not load exif data from '%1'") .arg(filePath));
        }
#if NEW_LIB_EXIF
        delete [] exifvalue;
#endif
#endif // EXIF_SUPPORT
    }
    catch (...)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Failed to extract EXIF headers from '%1'") .arg(filePath));
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
    srcDir.setFilter(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    QFileInfoList list = srcDir.entryInfoList();
    QFileInfoList::const_iterator it = list.begin();
    for (; it != list.end(); ++it)
    {
        const QString fn = it->fileName();
        QFileInfo dfi(dstDir, fn);
        ok &= Copy(*it, dfi);
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
    srcDir.setFilter(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    QFileInfoList list = srcDir.entryInfoList();
    QFileInfoList::const_iterator it = list.begin();
    for (; it != list.end(); ++it)
    {
        const QString fn = it->fileName();
        QFileInfo dfi(dstDir, fn);
        ok &= Move(*it, dfi);
    }

    return ok && FileDelete(src);
}

bool GalleryUtil::DeleteDirectory(const QFileInfo &dir)
{
    if (!dir.exists())
        return false;

    QDir srcDir(dir.absoluteFilePath());
    srcDir.setFilter(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    QFileInfoList list = srcDir.entryInfoList();
    QFileInfoList::const_iterator it = list.begin();
    for (; it != list.end(); ++it)
    {
        const QString fn = it->fileName();
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


void GalleryUtil::PlayVideo(const QString &filename)
{
    // HACK begin - remove when everything is using mythui
    vector<QWidget *> widgetList;
    if (GetMythMainWindow()->currentWidget())
    {
        QWidget *widget = GetMythMainWindow()->currentWidget();

        while (widget)
        {
            widgetList.push_back(widget);
            GetMythMainWindow()->detach(widget);
            widget->hide();
            widget = GetMythMainWindow()->currentWidget();
        }

        GetMythMainWindow()->GetPaintWindow()->raise();
        GetMythMainWindow()->GetPaintWindow()->setFocus();
        //GetMythMainWindow()->ShowPainterWindow();
    }
    // HACK end

    GetMythMainWindow()->HandleMedia("Internal", filename);


    // HACK begin - remove when everything is using mythui
    vector<QWidget*>::reverse_iterator it;
    for (it = widgetList.rbegin(); it != widgetList.rend(); ++it)
    {
        GetMythMainWindow()->attach(*it);
        (*it)->show();
    }
    //GetMythMainWindow()->HidePainterWindow();
    // HACK end
}

static QFileInfo MakeUnique(const QFileInfo &dest)
{
    QFileInfo newDest = dest;

    for (uint i = 0; newDest.exists(); i++)
    {
        QString basename = QString("%1_%2.%3")
            .arg(dest.baseName()).arg(i).arg(dest.completeSuffix());

        newDest.setFile(dest.dir(), basename);

        LOG(VB_GENERAL, LOG_ERR, LOC +
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

        LOG(VB_GENERAL, LOG_ERR, LOC +
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

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
