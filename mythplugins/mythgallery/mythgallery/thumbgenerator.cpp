/* ============================================================
 * File  : thumbgenerator.cpp
 * Author: Renchi Raju <renchi@pooh.tam.uiuc.edu>
 * Date  : 2004-01-02
 * Description :
 *
 * Copyright 2004 by Renchi Raju

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

#include <qapplication.h>
#include <qimage.h>
#include <qobject.h>
#include <qfileinfo.h>
#include <qdir.h>
#include <QEvent>
#include <qimagereader.h>

#include "config.h"
#include "mythtv/mythcontext.h"
#include "mythtv/util.h"
#include <mythtv/libmythui/mythuihelper.h>
#include <mythtv/mythdirs.h>

#include "thumbgenerator.h"
#include "constants.h"
#include "galleryutil.h"

#ifdef EXIF_SUPPORT
#include <libexif/exif-data.h>
#include <libexif/exif-entry.h>
#endif

ThumbGenerator::ThumbGenerator(QObject *parent, int w, int h)
{
    m_parent = parent;
    m_width  = w;
    m_height = h;
    m_isGallery = false;
}

ThumbGenerator::~ThumbGenerator()
{
    cancel();
    wait();
}

void ThumbGenerator::setSize(int w, int h)
{
    m_width = w;
    m_height = h;
}

void ThumbGenerator::setDirectory(const QString& directory, bool isGallery)
{
    m_mutex.lock();
    m_directory = directory;
    m_isGallery = isGallery;
    m_mutex.unlock();
}

void ThumbGenerator::addFile(const QString& filePath)
{
    // Add a file to the list of thumbs.
    // Must remember to call start after adding all the files!
    m_mutex.lock();
    m_fileList.append(filePath);
    m_mutex.unlock();
}

void ThumbGenerator::cancel()
{
    m_mutex.lock();
    m_fileList.clear();
    m_mutex.unlock();
}

void ThumbGenerator::run()
{

    while (moreWork()) {

        QString file, dir;
        bool    isGallery;

        m_mutex.lock();
        dir       = m_directory;
        isGallery = m_isGallery;
        file = m_fileList.first();
        if (!m_fileList.isEmpty())
            m_fileList.pop_front();
        m_mutex.unlock();

        if (file.isEmpty())
            continue;

        QString   filePath = dir + QString("/") + file;
        QFileInfo fileInfo(filePath);
        if (!fileInfo.exists())
            continue;

        if (isGallery) {

            if (fileInfo.isDir())
                isGallery = checkGalleryDir(fileInfo);
            else
                isGallery = checkGalleryFile(fileInfo);
        }

        if (!isGallery) {

            QString cachePath = QString("%1%2.jpg").arg(getThumbcacheDir(dir))
                                                   .arg(file);
            QFileInfo cacheInfo(cachePath);

            if (cacheInfo.exists() &&
                cacheInfo.lastModified() >= fileInfo.lastModified()) {
                continue;
            }
            else {

                // cached thumbnail not there or out of date
                QImage image;

                if (cacheInfo.exists()) {
                    // Remove the old one if it exists
                    QFile::remove(cachePath);
                }

                if (fileInfo.isDir())
                    loadDir(image, fileInfo);
                else
                    loadFile(image, fileInfo);

                if (image.isNull())
                    continue; // give up;

                // if the file is a movie save the image to use as a screenshot
                if (GalleryUtil::isMovie(fileInfo.filePath()))
                {
                    QString screenshotPath = QString("%1%2-screenshot.jpg")
                            .arg(getThumbcacheDir(dir))
                            .arg(file);
                    image.save(screenshotPath, "JPEG", 96);
                }

                image = image.scaled(m_width,m_height,
                                Qt::KeepAspectRatio, Qt::SmoothTransformation);
                image.save(cachePath, "JPEG", 96);

                // deep copies all over
                ThumbData *td = new ThumbData;
                td->directory = dir;
                td->fileName  = file;
                td->thumb     = image.copy();

                // inform parent we have thumbnail ready for it
                QApplication::postEvent(m_parent, new ThumbGenEvent(td));

            }
        }
    }
}

bool ThumbGenerator::moreWork()
{
    bool result;
    m_mutex.lock();
    result = !m_fileList.isEmpty();
    m_mutex.unlock();
    return result;
}

bool ThumbGenerator::checkGalleryDir(const QFileInfo& fi)
{
    // try to find a highlight
    QDir subdir(fi.absFilePath(), "*.highlight.*", QDir::Name,
                QDir::Files);


    if (subdir.count() > 0) {
        // check if the image format is understood
        QString path(subdir.entryInfoList().begin()->absFilePath());
        QImageReader testread(path);
        return testread.canRead();
    }
    else
        return false;
}

bool ThumbGenerator::checkGalleryFile(const QFileInfo& fi)
{
    // if the image name is xyz.jpg, then look
    // for a file named xyz.thumb.jpg.
    QString fn = fi.fileName();
    int firstDot = fn.find('.');
    if (firstDot > 0)
    {
        fn.insert(firstDot, ".thumb");
        QFileInfo galThumb(fi.dirPath(true) + "/" + fn);
        if (galThumb.exists())
        {
            QImageReader testread(galThumb.absFilePath());
            return testread.canRead();
        }
        else
            return false;
    }
    return false;
}

void ThumbGenerator::loadDir(QImage& image, const QFileInfo& fi)
{
    QDir dir(fi.absFilePath());
    dir.setFilter(QDir::Files);
    QFileInfoList list = dir.entryInfoList();
    if (list.isEmpty())
        return;

    QFileInfoList::const_iterator it = list.begin();
    const QFileInfo *f;

    bool found = false;
    while (it != list.end()) {
        f = &(*it);
        QImageReader testread(f->absFilePath());
        if (testread.canRead()) {
            found = true;
            break;
        }
        ++it;
    }

    if (found) {
        loadFile(image, *f);
        return;
    }
    else {
        // if we didn't find the image yet
        // go into subdirs and keep looking

        dir.setFilter(QDir::Dirs);
        QFileInfoList dirlist = dir.entryInfoList();
        if (dirlist.isEmpty())
            return;

        QFileInfoList::const_iterator it = dirlist.begin();
        const QFileInfo *f;

        while (it != dirlist.end() && image.isNull() ) {

            f = &(*it);
            ++it;

            if (f->fileName() == "." || f->fileName() == "..")
                continue;

            loadDir(image, *f);
        }
    }
}

void ThumbGenerator::loadFile(QImage& image, const QFileInfo& fi)
{
    if (GalleryUtil::isMovie(fi.filePath()))
    {
        bool thumbnailCreated = false;
        QDir tmpDir("/tmp/mythgallery");
        if (!tmpDir.exists())
        {
            if (!tmpDir.mkdir(tmpDir.absPath()))
            {
                VERBOSE(VB_IMPORTANT, "Unable to create temp dir for movie "
                        "thumbnail creation: " + tmpDir.absPath());
            }
        }

        if (tmpDir.exists())
        {
            QString cmd = "cd \"" + tmpDir.absPath() +
                          "\"; mplayer -nosound -frames 1 -vo png:z=6 \"" +
                          fi.absFilePath() + "\"";
            if (myth_system(cmd) == 0)
            {
                QFileInfo thumb(tmpDir.filePath("00000001.png"));
                if (thumb.exists())
                {
                    QImage img(thumb.absFilePath());
                    image = img;
                    thumbnailCreated = true;
                }
            }
        }

        if (!thumbnailCreated)
        {
            QImage *img = GetMythUI()->LoadScaleImage("gallery-moviethumb.png");
            if (img)
            {
                image = *img;
            }
        }
    }
    else
    {
#ifdef EXIF_SUPPORT
        // Try to get thumbnail from exif data
        ExifData *ed = exif_data_new_from_file(fi.absFilePath());
        if (ed && ed->data)
        {
            image.loadFromData(ed->data, ed->size);
        }

        if (ed)
            exif_data_free(ed);

        if (image.width() > 0 && image.height() > 0)
            return;
#endif

        image.load(fi.absFilePath());
    }
}

// static function
QString ThumbGenerator::getThumbcacheDir(const QString& inDir)
{
    QString galleryDir = gContext->GetSetting("GalleryDir");

    // For directory "/my/images/january", this function either returns
    // "/my/images/january/.thumbcache" or "~/.mythtv/mythgallery/january/.thumbcache"
    QString aPath = inDir + QString("/.thumbcache/");
    QDir dir(aPath);
    if (gContext->GetNumSetting("GalleryThumbnailLocation") &&
        !dir.exists() &&
        inDir.startsWith(galleryDir))
    {
        dir.mkpath(aPath);
    }

    if (!gContext->GetNumSetting("GalleryThumbnailLocation") ||
        !dir.exists() ||
        !inDir.startsWith(galleryDir))
    {
        // Arrive here if storing thumbs in home dir,
        // OR failed to create thumb dir in gallery pics location
        int prefixLen = gContext->GetSetting("GalleryDir").length();
        aPath = GetConfDir() + "/MythGallery";
        aPath += inDir.right(inDir.length() - prefixLen);
        aPath += QString("/.thumbcache/");
        dir.setPath(aPath);
        dir.mkpath(aPath);
    }

    return aPath;
}
