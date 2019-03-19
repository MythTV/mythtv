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

#include <QStringList>
#include <QImage>

#include <mthread.h>

class QObject;
class QImage;

typedef struct {
    QImage  thumb;
    QString fileName;
    QString directory;
} ThumbData;

class ThumbGenEvent : public QEvent
{
  public:
    explicit ThumbGenEvent(ThumbData *td) : QEvent(kEventType), m_thumbData(td) {}
    ~ThumbGenEvent() = default;

    ThumbData *m_thumbData {nullptr};

    static Type kEventType;
};

class ThumbGenerator : public MThread
{
public:

    ThumbGenerator(QObject *parent, int w, int h)
        : MThread("ThumbGenerator"), m_parent(parent),
          m_width(w), m_height(h) {}
    ~ThumbGenerator();

    void setSize(int w, int h);
    void setDirectory(const QString& directory, bool isGallery=false);
    void addFile(const QString& filePath);
    void cancel();

    static QString getThumbcacheDir(const QString& inDir);

protected:

    void run() override; // MThread

private:

    bool moreWork();
    bool checkGalleryDir(const QFileInfo& fi);
    bool checkGalleryFile(const QFileInfo& fi);
    void loadDir(QImage& image, const QFileInfo& fi);
    void loadFile(QImage& image, const QFileInfo& fi);

    QObject     *m_parent    {nullptr};
    QString      m_directory;
    bool         m_isGallery {false};
    QStringList  m_fileList;
    QMutex       m_mutex;
    int          m_width;
    int          m_height;
    bool         m_cancel    {false};
};

#endif /* THUMBGENERATOR_H */
