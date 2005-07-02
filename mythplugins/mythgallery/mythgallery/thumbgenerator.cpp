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

#include "mythtv/mythcontext.h"
#include "mythtv/util.h"

#include "thumbgenerator.h"
#include "constants.h"
#include "galleryutil.h"

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
        
            QString cachePath = dir + QString("/.thumbcache/") + file;
            QFileInfo cacheInfo(cachePath);

            if (cacheInfo.exists() &&
                cacheInfo.lastModified() >= fileInfo.lastModified()) {
                continue;
            }
            else {
                
                // cached thumbnail not there or out of date
                QImage image;

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
        image.load(f->absFilePath());
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
      if (! tmpDir.exists())
      {
        if (! tmpDir.mkdir(tmpDir.absPath()))
        {
          std::cerr << "Unable to create temp dir for movie thumbnail creation: "
                     << tmpDir.absPath() << endl;
        }
      }
      if (tmpDir.exists())
      {
          QString cmd = "cd " + tmpDir.absPath()
            + "; mplayer -nosound -frames 1 -vo png " + fi.absFilePath();
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
      if (! thumbnailCreated)
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
      image.load(fi.absFilePath());
  }
}
