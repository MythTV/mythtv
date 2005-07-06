/* ============================================================
 * File  : thumbgenerator.h
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

#ifndef THUMBGENERATOR_H
#define THUMBGENERATOR_H

#include <qthread.h>
#include <qstring.h>
#include <qstringlist.h>

class QObject;
class QImage;

typedef struct {
    QImage  thumb;
    QString fileName;
    QString directory;
} ThumbData;

class ThumbGenerator : public QThread
{
public:

    ThumbGenerator(QObject *parent, int w, int h);
    ~ThumbGenerator();

    void setDirectory(const QString& directory, bool isGallery=false);
    void addFile(const QString& fileName);
    void cancel();
    
    QString getThumbcacheDir(const QString& inDir);

protected:

    void run();
    
private:

    bool moreWork();
    bool checkGalleryDir(const QFileInfo& fi);
    bool checkGalleryFile(const QFileInfo& fi);
    void loadDir(QImage& image, const QFileInfo& fi);
    void loadFile(QImage& image, const QFileInfo& fi);
    
    bool mkpath(const QString& inPath);

    QObject     *m_parent;
    QString      m_directory;
    bool         m_isGallery;
    QStringList  m_fileList;
    QMutex       m_mutex;
    int          m_width, m_height;

};

#endif /* THUMBGENERATOR_H */
