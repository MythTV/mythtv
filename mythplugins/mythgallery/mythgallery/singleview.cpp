/* ============================================================
 * File  : singleview.cpp
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

#include <iostream>

#include <qsqldatabase.h>
#include <qevent.h>
#include <qimage.h>
#include <qtimer.h>
#include <qfileinfo.h>
#include <qdir.h>

#include "thumbgenerator.h"
#include "singleview.h"

SingleView::SingleView(QSqlDatabase *db, const QPtrList<ThumbItem>& images,
                     int pos, ThumbGenerator *thumbGen,
                     MythMainWindow *parent, const char *name )
    : MythDialog(parent, name)
{
    m_db        = db;
    m_imageList = images;
    m_imageList.setAutoDelete(false);
    m_pos       = pos;
    m_thumbGen  = thumbGen;
    
    m_pixmap      = 0;
    m_rotateAngle = 0;
    m_zoom        = 1;
    m_sx          = 0;
    m_sy          = 0;

    m_info        = false;
    m_infoBgPix   = 0;

    m_timer = new QTimer(this);
    connect(m_timer, SIGNAL(timeout()),
            SLOT(slotTimeOut()));
    m_timerSecs = gContext->GetNumSetting("SlideshowDelay");
    if (!m_timerSecs)
        m_timerSecs = 2;
    
    setNoErase();
    QString bgtype = gContext->GetSetting("SlideshowBackground");
    if (bgtype != "theme" && !bgtype.isEmpty())
        setPalette(QPalette(QColor(bgtype)));

    loadImage();
}

SingleView::~SingleView()
{
    m_timer->stop();
    delete m_timer;
    
    if (m_pixmap)
        delete m_pixmap;

    if (m_infoBgPix)
        delete m_infoBgPix;
}

void SingleView::paintEvent(QPaintEvent *)
{
    QPixmap pix(screenwidth, screenheight);
    pix.fill(this, 0, 0);
    if (m_pixmap) {
        if (m_pixmap->width() <= screenwidth &&
            m_pixmap->height() <= screenheight)
            bitBlt(&pix, screenwidth/2-m_pixmap->width()/2,
                   screenheight/2-m_pixmap->height()/2,
                   m_pixmap,0,0,-1,-1,Qt::CopyROP);
        else
            bitBlt(&pix, 0, 0, m_pixmap, m_sx, m_sy,
                   pix.width(), pix.height());
        if (m_zoom != 1) {
            QPainter p(&pix, this);
            p.drawText(screenwidth / 10, screenheight / 10,
                       QString::number(m_zoom) + "x Zoom");
            p.end();
        }

        if (m_info) {

            if (!m_infoBgPix)
                createInfoBg();

            bitBlt(&pix, screenwidth/10, screenheight/10,
                   m_infoBgPix,0,0,-1,-1,Qt::CopyROP);

            QPainter p(&pix, this);
            ThumbItem* item = m_imageList.at(m_pos);
            QFileInfo fi(item->path);
            QString info(item->name);
            info += "\n\n" + tr("Folder: ") + fi.dir().dirName();
            info += "\n" + tr("Created: ") + fi.created().toString();
            info += "\n" + tr("Modified: ") + fi.lastModified().toString();
            info += "\n" + QString(tr("Bytes") + ": %1").arg(fi.size());
            info += "\n" + QString(tr("Width") + ": %1 " + tr("pixels"))
                    .arg(m_image.width());
            info += "\n" + QString(tr("Height") + ": %1 " + tr("pixels"))
                    .arg(m_image.height());
            info += "\n" + QString(tr("Pixel Count") + ": %1 " + 
                                   tr("megapixels"))
                    .arg((float)m_image.width() * m_image.height() / 1000000,
                         0, 'f', 2);
            info += "\n" + QString(tr("Rotation Angle") + ": %1 " +
                                   tr("degrees")).arg(m_rotateAngle);
            p.drawText(screenwidth/10 + (int)(10*wmult),
                       screenheight/10 + (int)(10*hmult),
                       m_infoBgPix->width()-2*(int)(10*wmult),
                       m_infoBgPix->height()-2*(int)(10*hmult),
                       Qt::AlignLeft, info);
            p.end();
        }

    }
        
    bitBlt(this,0,0,&pix,0,0,-1,-1,Qt::CopyROP);
}

void SingleView::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    bool timerWasRunning = m_timerRunning;
    m_timerRunning = false;
    m_timer->stop();
    
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Gallery", e, actions);

    int scrollX = (int)(10*wmult);
    int scrollY = (int)(10*hmult);
    
    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "LEFT" || action == "UP")
        {
            m_rotateAngle = 0;
            m_zoom = 1;
            m_sx   = 0;
            m_sy   = 0;
            loadPrev();
            loadImage();
        }
        else if (action == "RIGHT" || action == "DOWN")
        {
            m_rotateAngle = 0;
            m_zoom = 1;
            m_sx   = 0;
            m_sy   = 0;
            loadNext();
            loadImage();
        }
        else if (action == "ZOOMOUT")
        {
            m_sx   = 0;
            m_sy   = 0;
            if (m_zoom > 0.5) {
                m_zoom = m_zoom /2;
                zoom();
            }
            else
                handled = false;
        }
        else if (action == "ZOOMIN")
        {
            m_sx   = 0;
            m_sy   = 0;
            if (m_zoom < 4.0) {
                m_zoom = m_zoom * 2;
                zoom();
            }
            else
                handled = true;
        }
        else if (action == "FULLSIZE")
        {
            m_sx = 0;
            m_sy = 0;
            if (m_zoom != 1) {
                m_zoom = 1.0;
                zoom();
            }
            else
                handled = true;
        }
        else if (action == "SCROLLLEFT")
        {
            if (m_zoom > 1.0) {
                m_sx -= scrollX;
                m_sx  = (m_sx < 0) ? 0 : m_sx;
            }
            else
                handled = false;
        }
        else if (action == "SCROLLRIGHT")
        {
            if (m_zoom > 1.0 && m_pixmap) {
                m_sx += scrollX;
                m_sx  = QMIN(m_sx, m_pixmap->width()-
                             scrollX-screenwidth);
            }
            else
                handled = false;
        }
        else if (action == "SCROLLUP")
        {
            if (m_zoom > 1.0 && m_pixmap) {
                m_sy += scrollY;
                m_sy  = QMIN(m_sy, m_pixmap->height()-
                             scrollY-screenheight);
            }
            else
                handled = false;
        }
        else if (action == "SCROLLDOWN")
        {
            if (m_zoom > 1.0) {
                m_sy -= scrollY;
                m_sy  = (m_sy < 0) ? 0 : m_sy;
            }
            else
                handled = false;
        }
        else if (action == "RECENTER")
        {
            if (m_zoom > 1.0 && m_pixmap) {
                m_sx = (m_pixmap->width()-screenwidth)/2;
                m_sy = (m_pixmap->height()-screenheight)/2;
            }
            else
                handled = false;
        }
        else if (action == "UPLEFT")
        {
            if (m_zoom > 1.0) {
                m_sx = m_sy = 0;
            }
            else
                handled = false;
        }
        else if (action == "LOWRIGHT")
        {
            if (m_zoom > 1.0 && m_pixmap) {
                m_sx  = m_pixmap->width()-scrollX-screenwidth;
                m_sy  = m_pixmap->height()-scrollY-screenheight;
            }
            else
                handled = false;
        }
        else if (action == "ROTRIGHT")
        {
            m_sx = 0;
            m_sy = 0;
            rotate(90);
        }
        else if (action == "ROTLEFT")
        {
            m_sx = 0;
            m_sy = 0;
            rotate(-90);
        }
        else if (action == "PLAY")
        {
            m_timerRunning = !timerWasRunning;
            m_rotateAngle = 0;
            m_zoom        = 1.0;
            m_sx          = 0;
            m_sy          = 0;
        }
        else if (action == "INFO")
        {
            m_info = !m_info;
        }
        else 
            handled = false;
    }

    if (handled) {
        update();
        if (m_timerRunning)
            startShow();
    }
    else {
        MythDialog::keyPressEvent(e);
    }
}

void SingleView::loadNext()
{
    m_pos++;
    if (m_pos >= (int)m_imageList.count())
        m_pos = 0;

    ThumbItem *item = m_imageList.at(m_pos);
    if (item->isDir) 
        loadNext();
}

void SingleView::loadPrev()
{
    m_pos--;
    if (m_pos < 0)
        m_pos = m_imageList.count()-1;

    ThumbItem *item = m_imageList.at(m_pos);
    if (item->isDir) 
        loadPrev();
}

void SingleView::loadImage()
{
    if (m_pixmap) {
        delete m_pixmap;
        m_pixmap = 0;
    }
    
    ThumbItem *item = m_imageList.at(m_pos);
    if (item) {

        m_image.load(item->path);

        if (!m_image.isNull()) {

            QString queryStr = "SELECT angle FROM gallerymetadata WHERE "
                               "image=\"" + item->path + "\";";
            QSqlQuery query = m_db->exec(queryStr);
			
            if (query.isActive()  && query.numRowsAffected() > 0) 
            {
                query.next();
                m_rotateAngle = query.value(0).toInt();
                if (m_rotateAngle != 0) {
                    QWMatrix matrix;
                    matrix.rotate(m_rotateAngle);
                    m_image = m_image.xForm(matrix);
                }
            }

            m_pixmap = new QPixmap(m_image.smoothScale(screenwidth, screenheight,
                                                       QImage::ScaleMin));
        }
    
    }
}

void SingleView::rotate(int angle)
{
    m_rotateAngle += angle;
    if (m_rotateAngle >= 360)
        m_rotateAngle -= 360;
    if (m_rotateAngle < 0)
        m_rotateAngle += 360;

    ThumbItem *item = m_imageList.at(m_pos);
    if (item) {
        QString queryStr = "REPLACE INTO gallerymetadata SET image=\"" +
                           item->path + "\", angle=" + 
                           QString::number(m_rotateAngle) + ";";
        m_db->exec(queryStr);

        // Regenerate thumbnail for this
        m_thumbGen->addFile(item->name);
    }
    
    if (!m_image.isNull()) {
        QWMatrix matrix;
        matrix.rotate(angle);
        m_image = m_image.xForm(matrix);
        if (m_pixmap) {
            delete m_pixmap;
            m_pixmap = 0;
        }
        m_pixmap = new QPixmap(m_image.smoothScale((int)(m_zoom*screenwidth),
                                                   (int)(m_zoom*screenheight),
                                                   QImage::ScaleMin));
    }
}

void SingleView::zoom()
{
    if (!m_image.isNull()) {
        if (m_pixmap) {
            delete m_pixmap;
            m_pixmap = 0;
        }
        m_pixmap =
            new QPixmap(m_image.smoothScale((int)(screenwidth*m_zoom),
                                            (int)(screenheight*m_zoom),
                                            QImage::ScaleMin));
    }
}

void SingleView::startShow()
{
    m_timerRunning = true;
    m_timer->start(m_timerSecs * 1000);
}

void SingleView::slotTimeOut()
{
    loadNext();
    loadImage();
    update();
}

void SingleView::createInfoBg()
{
    QImage img(screenwidth-2*screenwidth/10,
               screenheight-2*screenheight/10,32);
    img.setAlphaBuffer(true);

    for (int y = 0; y < img.height(); y++) 
    {
        for (int x = 0; x < img.width(); x++) 
        {
            uint *p = (uint *)img.scanLine(y) + x;
            *p = qRgba(0, 0, 0, 120);
        }
    }

    m_infoBgPix = new QPixmap(img);
}
