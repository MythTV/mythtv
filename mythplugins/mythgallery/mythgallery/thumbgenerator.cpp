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

#include "config.h"
#include "mythtv/mythcontext.h"
#include "mythtv/util.h"

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
        
            QString cachePath = getThumbcacheDir(dir) + file;
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
                
                image = image.smoothScale(m_width,m_width,QImage::ScaleMax);
                image.save(cachePath, "JPEG");

                // deep copies all over
                ThumbData *td = new ThumbData;
                td->directory = dir;
                td->fileName  = file;
                td->thumb     = image.copy();

                // inform parent we have thumbnail ready for it
                QApplication::postEvent(m_parent,
                                        new QCustomEvent(QEvent::User, td));
                
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
        QString path(subdir.entryInfoList()->getFirst()->absFilePath());
        return (QImageIO::imageFormat(path) != 0);
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
            return (QImageIO::imageFormat(galThumb.absFilePath()) != 0);
        else
            return false;
    }
    return false;
}

void ThumbGenerator::loadDir(QImage& image, const QFileInfo& fi)
{
    QDir dir(fi.absFilePath());
    dir.setFilter(QDir::Files);
    const QFileInfoList *list = dir.entryInfoList();
    if (!list)
        return;

    QFileInfoListIterator it( *list );
    QFileInfo *f;

    bool found = false;
    while ( (f = it.current()) != 0 ) {
        if (QImage::imageFormat(f->absFilePath()) != 0) {
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
        const QFileInfoList *dirList = dir.entryInfoList();
        if (!dirList)
            return;

        QFileInfoListIterator it( *dirList );
        QFileInfo *f;

        while ( ((f = it.current()) != 0) && image.isNull() ) {

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
                std::cerr << "Unable to create temp dir for movie thumbnail creation: "
                          << tmpDir.absPath() << endl;
            }
        }

        if (tmpDir.exists())
        {
            QString cmd = "cd \"" + tmpDir.absPath() + 
                          "\"; mplayer -nosound -frames 1 -vo png \"" + 
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
            QImage *img = gContext->LoadScaleImage("gallery-moviethumb.png");
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

QString ThumbGenerator::getThumbcacheDir(const QString& inDir)
{
    QString galleryDir = gContext->GetSetting("GalleryDir");

    // For directory "/my/images/january", this function either returns 
    // "/my/images/january/.thumbcache" or "~/.mythtv/mythgallery/january/.thumbcache"
    QString aPath = inDir + QString("/.thumbcache/");
    if (gContext->GetNumSetting("GalleryThumbnailLocation") &&
        !QDir(aPath).exists() &&
        inDir.startsWith(galleryDir))
    {
        mkpath(aPath);
    }
    if (!gContext->GetNumSetting("GalleryThumbnailLocation") ||
        !QDir(aPath).exists() ||
        !inDir.startsWith(galleryDir))
    {
        // Arrive here if storing thumbs in home dir, 
        // OR failed to create thumb dir in gallery pics location
        int prefixLen = gContext->GetSetting("GalleryDir").length();
        aPath = gContext->GetConfDir() + "/MythGallery";
        aPath += inDir.right(inDir.length() - prefixLen);
        aPath += QString("/.thumbcache/");
        mkpath(aPath);
    }
    
    return aPath;
}

bool ThumbGenerator::mkpath(const QString& inPath)
{
    // The function will create all parent directories necessary to create the directory.
    // We can replace this function with QDir::mkpath() when uprading to Qt 4.0
    int i = 0;
    QString absPath = QDir(inPath).absPath() + "/";
    QDir parent("/");
    do 
    {
        i = absPath.find('/', i + 1);
        if (i == -1) 
            return true;

        QString subPath(absPath.left(i));
        if (!QDir(subPath).exists()) 
        {
            if (!parent.mkdir(subPath.right(subPath.length() - 
                                            parent.absPath().length() - 1))) 
            {
                return false;
            }
        }
        parent = QDir(subPath);
    } while(1);
}
