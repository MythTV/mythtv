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

#include "thumbgenerator.h"

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
    m_directory = QString(directory.latin1());
    m_isGallery = isGallery;
    m_mutex.unlock();
}

void ThumbGenerator::addFile(const QString& filePath)
{
    m_mutex.lock();
    m_fileList.append(QString(filePath.latin1())); // deep copy
    m_mutex.unlock();
    if (!running())
        start();
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

        QImage image;

        if (isGallery) {
            if (fileInfo.isDir()) 
                loadGalleryDir(image, fileInfo);
            else
                loadGalleryFile(image, fileInfo);
        }

        if (!isGallery || image.isNull()) {
        
            QString cachePath = dir + QString("/.thumbcache/") + file;
            QFileInfo cacheInfo(cachePath);

            if (cacheInfo.exists() &&
                (cacheInfo.lastModified() >= fileInfo.lastModified())) {
                image.load(cachePath);
                image = image.smoothScale(m_width,m_width,QImage::ScaleMax);
            }
            else {
                if (fileInfo.isDir()) {
                    loadDir(image, fileInfo);
                }
                else {
                    loadFile(image, fileInfo);
                }

                if (image.isNull())
                    continue;
                image = image.smoothScale(m_width,m_width,QImage::ScaleMax);
                image.save(cachePath, "JPEG");
            }

        }
        
        if (image.isNull())
            continue;

        // deep copies all over
        ThumbData *td = new ThumbData;
        td->directory = QString(dir.latin1());
        td->fileName  = QString(file.latin1());
        td->thumb     = QImage(image.copy());
        
        QApplication::postEvent(m_parent,
                                new QCustomEvent(QEvent::User, td));
        
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

void ThumbGenerator::loadGalleryDir(QImage& image, const QFileInfo& fi)
{
    // try to find a highlight
    QDir subdir(fi.absFilePath(), "*.highlight.*", QDir::Name, 
                QDir::Files);
    if (subdir.count()>0) 
        image.load(subdir.entryInfoList()->getFirst()->absFilePath());
}

void ThumbGenerator::loadGalleryFile(QImage& image, const QFileInfo& fi)
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
            image.load(galThumb.absFilePath());
        }
    }
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

    if (!found)
        return;

    image.load(f->absFilePath());
}

void ThumbGenerator::loadFile(QImage& image, const QFileInfo& fi)
{
    image.load(fi.absFilePath());
}
