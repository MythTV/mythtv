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
#include <cmath>

#include <qevent.h>
#include <qimage.h>
#include <qfileinfo.h>
#include <qdir.h>

#include <mythtv/mythcontext.h>

#include "singleview.h"
#include "constants.h"
#include "galleryutil.h"

SingleView::SingleView(ThumbList itemList, int pos, int slideShow,
                       MythMainWindow *parent, const char *name )
    : MythDialog(parent, name)
{
    m_itemList  = itemList;
    m_pos       = pos;
    m_slideShow = slideShow;

    // --------------------------------------------------------------------

    // remove all dirs from m_itemList;
    bool recurse = gContext->GetNumSetting("GalleryRecursiveSlideshow", 0);

    m_itemList.setAutoDelete(false);
    ThumbItem* item = m_itemList.first();
    while (item) {
        ThumbItem* next = m_itemList.next();
        if (item->isDir) {
            if (recurse)
                GalleryUtil::loadDirectory(m_itemList, item->path, recurse, NULL, NULL);
            m_itemList.remove(item);
        }
        item = next;
    }

    // since we remove dirs item position might have changed
    item = itemList.at(m_pos);
    if (item) 
        m_pos = m_itemList.find(item);

    if (!item || (m_pos == -1))
        m_pos = 0;
    
    // --------------------------------------------------------------------

    registerEffects();
    
    m_effectMethod = 0;
    m_effectRandom = false;
    QString transType = gContext->GetSetting("SlideshowTransition");
    if (!transType.isEmpty() && m_effectMap.contains(transType))
        m_effectMethod = m_effectMap[transType];
    
    if (!m_effectMethod || transType == "random") {
        m_effectMethod = getRandomEffect();
        m_effectRandom = true;
    }

    // ---------------------------------------------------------------

    m_delay = gContext->GetNumSetting("SlideshowDelay", 0);
    if (!m_delay)
        m_delay = 2;

    // --------------------------------------------------------------------

    setNoErase();
    QString bgtype = gContext->GetSetting("SlideshowBackground");
    if (bgtype != "theme" && !bgtype.isEmpty())
        setPalette(QPalette(QColor(bgtype)));

    // --------------------------------------------------------------------

    m_movieState  = 0;
    m_pixmap      = 0;
    m_rotateAngle = 0;
    m_zoom        = 1;
    m_sx          = 0;
    m_sy          = 0;

    m_info        = false;
    m_infoBgPix   = 0;

    // --------------------------------------------------------------------

    m_tmout = m_delay * 1000;
    m_effectRunning = false;
    m_running = false;
    m_i = 0;
    m_effectPix = 0;
    m_painter = 0;
    mIntArray = 0;

    m_timer = new QTimer(this);
    connect(m_timer, SIGNAL(timeout()), SLOT(slotTimeOut()));

    // --------------------------------------------------------------------

    if(slideShow > 1)
        randomFrame();
    loadImage();
    if (slideShow) {
        m_running = true;
        m_timer->start(m_tmout, true);
        gContext->DisableScreensaver();
    }
}

SingleView::~SingleView()
{
    if (m_painter) {
        if (m_painter->isActive())
            m_painter->end();
        delete m_painter;
    }

    if (m_pixmap)
        delete m_pixmap;

    if (m_effectPix)
        delete m_effectPix;
    
    if (m_infoBgPix)
        delete m_infoBgPix;

    if (mIntArray)
        delete [] mIntArray;
}

void SingleView::paintEvent(QPaintEvent *)
{
    if (m_movieState > 0)
    {
        if (m_movieState == 1)
        {
            QString path = QString("\"") + item->path + "\"";
            m_movieState = 2;
            ThumbItem* item = m_itemList.at(m_pos);
            QString cmd = gContext->GetSetting("GalleryMoviePlayerCmd");
            cmd.replace("%s", path);
            myth_system(cmd);
            if (!m_running)
            {
                reject();
            }
        }
    }
    else if (!m_effectRunning) {
        
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
                ThumbItem* item = m_itemList.at(m_pos);
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
    else {
        if (m_effectMethod)
            (this->*m_effectMethod)();
    }
}

void SingleView::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;

    bool wasRunning = m_running;
    m_timer->stop();
    m_running = false;
    gContext->RestoreScreensaver();
    m_effectRunning = false;
    m_tmout = m_delay * 1000;
    if (m_painter && m_painter->isActive())
        m_painter->end();

    bool wasInfo = m_info;
    m_info = false;
    
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
            retreatFrame();
            loadImage();
        }
        else if (action == "RIGHT" || action == "DOWN")
        {
            m_rotateAngle = 0;
            m_zoom = 1;
            m_sx   = 0;
            m_sy   = 0;
            advanceFrame();
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
                handled = false;
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
                handled = false;
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
            m_sx   = 0;
            m_sy   = 0;
            m_zoom = 1.0;
            m_rotateAngle = 0;
            m_running = !wasRunning;
        }
        else if (action == "INFO")
        {
            m_info = !wasInfo;
        }
        else 
            handled = false;
    }

    if (m_running) {
        m_timer->start(m_tmout, true);
        gContext->DisableScreensaver();
    }

    if (handled) {
        update();
    }
    else {
        MythDialog::keyPressEvent(e);
    }
}

void SingleView::advanceFrame()
{
    m_pos++;
    if (m_pos >= (int)m_itemList.count())
        m_pos = 0;
}

void SingleView::randomFrame()
{
    int newframe;
    if(m_itemList.count() > 1){
        while((newframe = (int)(rand()/(RAND_MAX+1.0) * m_itemList.count())) == m_pos);
        m_pos = newframe;
    }
}

void SingleView::retreatFrame()
{
    m_pos--;
    if (m_pos < 0)
        m_pos = m_itemList.count()-1;
}

void SingleView::loadImage()
{
    m_movieState = 0;
    if (m_pixmap) {
        delete m_pixmap;
        m_pixmap = 0;
    }
    
    ThumbItem *item = m_itemList.at(m_pos);
    if (item) {
      if (GalleryUtil::isMovie(item->path)) {
        m_movieState = 1;
      }
      else {
        m_image.load(item->path);
        
        if (!m_image.isNull()) {

          m_rotateAngle = item->GetRotationAngle();
          
          if (m_rotateAngle != 0) {
            QWMatrix matrix;
            matrix.rotate(m_rotateAngle);
            m_image = m_image.xForm(matrix);
          }
        
          m_pixmap = new QPixmap(m_image.smoothScale(screenwidth, screenheight,
                                 QImage::ScaleMin));
          
        }
      }
    }
    else {
      std::cerr << "SingleView: Failed to load image "
        << item->path << std::endl;
    }
}

void SingleView::rotate(int angle)
{
    m_rotateAngle += angle;
    if (m_rotateAngle >= 360)
        m_rotateAngle -= 360;
    if (m_rotateAngle < 0)
        m_rotateAngle += 360;

    ThumbItem *item = m_itemList.at(m_pos);
    if (item) {
        item->SetRotationAngle(m_rotateAngle);

        // Delete thumbnail for this
        if (item->pixmap)
            delete item->pixmap;
        item->pixmap = 0;
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

void SingleView::registerEffects()
{
    m_effectMap.insert("none", &SingleView::effectNone);
    m_effectMap.insert("chess board", &SingleView::effectChessboard);
    m_effectMap.insert("melt down", &SingleView::effectMeltdown);
    m_effectMap.insert("sweep", &SingleView::effectSweep);
    m_effectMap.insert("noise", &SingleView::effectNoise);
    m_effectMap.insert("growing", &SingleView::effectGrowing);
    m_effectMap.insert("incoming edges", &SingleView::effectIncomingEdges);
    m_effectMap.insert("horizontal lines", &SingleView::effectHorizLines);
    m_effectMap.insert("vertical lines", &SingleView::effectVertLines);
    m_effectMap.insert("circle out", &SingleView::effectCircleOut);
    m_effectMap.insert("multicircle out", &SingleView::effectMultiCircleOut);
    m_effectMap.insert("spiral in", &SingleView::effectSpiralIn);
    m_effectMap.insert("blobs", &SingleView::effectBlobs);
}

SingleView::EffectMethod SingleView::getRandomEffect()
{
    QMap<QString,EffectMethod> tmpMap(m_effectMap);
    tmpMap.remove("none");
    
    QStringList t = tmpMap.keys();

    int count = t.count();
    int i = (int)((float)(count)*rand()/(RAND_MAX+1.0));
    QString key = t[i];

    return tmpMap[key];
}

void SingleView::startPainter()
{
    if (!m_painter)
        m_painter = new QPainter();
    if (m_painter->isActive())
        m_painter->end();

    QBrush brush;
    if (m_effectPix)
        brush.setPixmap(*m_effectPix);

    m_painter->begin(this);
    m_painter->setBrush(brush);
    m_painter->setPen(Qt::NoPen);
}

void SingleView::createEffectPix()
{
    if (!m_effectPix) 
        m_effectPix = new QPixmap(screenwidth, screenheight);

    m_effectPix->fill(this, 0, 0);

    if (m_pixmap)
        bitBlt(m_effectPix,
               (m_effectPix->width()-m_pixmap->width())/2,
               (m_effectPix->height()-m_pixmap->height())/2,
               m_pixmap, 0, 0, -1, -1, Qt::CopyROP);
}

void SingleView::effectNone()
{
    m_effectRunning = false;
    m_tmout = -1;
    update();
    return;
}

void SingleView::effectChessboard()
{
    if (m_i == 0)
    {
        mw  = width();
        mh  = height();
        mdx = 8;              // width of one tile
        mdy = 8;              // height of one tile
        mj  = (mw+mdx-1)/mdx; // number of tiles
        mx  = mj*mdx;         // shrinking x-offset from screen border
        mix = 0;              // growing x-offset from screen border
        miy = 0;              // 0 or mdy for growing tiling effect
        my  = mj&1 ? 0 : mdy; // 0 or mdy for shrinking tiling effect
        mwait = 800 / mj;     // timeout between effects
    }

    if (mix >= mw)
    {
        m_effectRunning = false;
        m_tmout = -1;
        update();
        return;
    }

    mix += mdx;
    mx  -= mdx;
    miy = miy ? 0 : mdy;
    my  = my ? 0 : mdy;

    for (int y=0; y<mw; y+=(mdy<<1))
    {
         bitBlt(this, mix, y+miy, m_effectPix, mix, y+miy,
                mdx, mdy, Qt::CopyROP, true);
         bitBlt(this, mx, y+my, m_effectPix, mx, y+my,
                mdx, mdy, Qt::CopyROP, true);
    }

    m_tmout = mwait;

    m_i = 1;
}

void SingleView::effectSweep()
{
    int w, h, x, y, i;

    if (m_i == 0)
    {
        // subtype: 0=sweep right to left, 1=sweep left to right
        //          2=sweep bottom to top, 3=sweep top to bottom
        mSubType = rand() % 4;
        mw  = width();
        mh  = height();
        mdx = (mSubType==1 ? 16 : -16);
        mdy = (mSubType==3 ? 16 : -16);
        mx  = (mSubType==1 ? 0 : mw);
        my  = (mSubType==3 ? 0 : mh);
    }

    if (mSubType==0 || mSubType==1)
    {
        // horizontal sweep
        if ((mSubType==0 && mx < -64) ||
            (mSubType==1 && mx > mw+64))
        {
            m_tmout = -1;
            m_effectRunning = false;
            update();
            return;
        }
        for (w=2,i=4,x=mx; i>0; i--, w<<=1, x-=mdx)
        {
            bitBlt(this, x, 0, m_effectPix,
                   x, 0, w, mh, Qt::CopyROP, true);
        }
        mx += mdx;
    }
    else
    {
        // vertical sweep
        if ((mSubType==2 && my < -64) ||
            (mSubType==3 && my > mh+64))
        {
            m_tmout = -1;
            m_effectRunning = false;
            update();
            return;
        }
        for (h=2,i=4,y=my; i>0; i--, h<<=1, y-=mdy)
        {
            bitBlt(this, 0, y, m_effectPix,
                   0, y, mw, h, Qt::CopyROP, true);
        }
        my += mdy;
    }

    m_tmout = 20;
    m_i = 1;
}

void SingleView::effectGrowing()
{
    if (m_i == 0)
    {
        mw = width();
        mh = height();
        mx = mw >> 1;
        my = mh >> 1;
        mi = 0;
        mfx = mx / 100.0;
        mfy = my / 100.0;
    }

    mx = (mw>>1) - (int)(mi * mfx);
    my = (mh>>1) - (int)(mi * mfy);
    mi++;

    if (mx<0 || my<0)
    {
        m_tmout = -1;
        m_effectRunning = false;
        update();
        return;
    }

    bitBlt(this, mx, my, m_effectPix, mx, my,
           mw - (mx<<1), mh - (my<<1), Qt::CopyROP, true);

    m_tmout = 20;
    m_i     = 1;
}

void SingleView::effectHorizLines()
{
    static int iyPos[] = { 0, 4, 2, 6, 1, 5, 3, 7, -1 };
    int y;

    if (m_i == 0)
    {
        mw = width();
        mh = height();
        mi = 0;
    }

    if (iyPos[mi] < 0)
    {
        m_tmout = -1;
        m_effectRunning = false;
        update();
        return;
    }

    for (y=iyPos[mi]; y<mh; y+=8)
    {
        bitBlt(this, 0, y, m_effectPix,
               0, y, mw, 1, Qt::CopyROP, true);
    }

    mi++;
    
    if (iyPos[mi] >= 0) {
        m_tmout = 160;
        m_i     = 1;
    }
    else {
        m_tmout = -1;
        m_effectRunning = false;
        update();
        return;
    }        
}

void SingleView::effectVertLines()
{
    static int ixPos[] = { 0, 4, 2, 6, 1, 5, 3, 7, -1 };
    int x;

    if (m_i == 0)
    {
        mw = width();
        mh = height();
        mi = 0;
    }

    if (ixPos[mi] < 0)
    {
        m_tmout = -1;
        m_effectRunning = false;
        update();
        return;
    }

    for (x=ixPos[mi]; x<mw; x+=8)
    {
        bitBlt(this, x, 0, m_effectPix,
               x, 0, 1, mh, Qt::CopyROP, true);
    }

    mi++;
    
    if (ixPos[mi] >= 0) {
        m_tmout = 160;
        m_i     = 1;
    }
    else {
        m_tmout = -1;
        m_effectRunning = false;
        update();
        return;
    }        
}

void SingleView::effectMeltdown()
{
    int i, x, y;
    bool done;

    if (m_i == 0)
    {
        if (mIntArray)
            delete [] mIntArray;
        mw = width();
        mh = height();
        mdx = 4;
        mdy = 16;
        mix = mw / mdx;
        mIntArray = new int[mix];
        for (i=mix-1; i>=0; i--)
            mIntArray[i] = 0;
    }

    done = true;
    for (i=0,x=0; i<mix; i++,x+=mdx)
    {
        y = mIntArray[i];
        if (y >= mh)
            continue;
        done = false;
        if ((rand()&15) < 6)
            continue;
        bitBlt(this, x, y, m_effectPix, x, y, mdx, mdy,
               Qt::CopyROP, true);
        mIntArray[i] += mdy;
    }

    if (done)
    {
        delete [] mIntArray;
        mIntArray = 0;

        m_tmout = -1;
        m_effectRunning = false;
        update();
        return;
    }

    m_tmout = 15;
    m_i     = 1;
}

void SingleView::effectIncomingEdges()
{
    int x1, y1;

    if (m_i == 0)
    {
        mw = width();
        mh = height();
        mix = mw >> 1;
        miy = mh >> 1;
        mfx = mix / 100.0;
        mfy = miy / 100.0;
        mi = 0;
        mSubType = rand() & 1;
    }

    mx = (int)(mfx * mi);
    my = (int)(mfy * mi);

    if (mx>mix || my>miy)
    {
        m_tmout = -1;
        m_effectRunning = false;
        update();
        return;
    }

    x1 = mw - mx;
    y1 = mh - my;
    mi++;

    if (mSubType)
    {
        // moving image edges
        bitBlt(this,  0,  0, m_effectPix, mix-mx, miy-my, mx, my,
               Qt::CopyROP, true);
        bitBlt(this, x1,  0, m_effectPix, mix, miy-my, mx, my,
               Qt::CopyROP, true);
        bitBlt(this,  0, y1, m_effectPix, mix-mx, miy, mx, my,
               Qt::CopyROP, true);
        bitBlt(this, x1, y1, m_effectPix, mix, miy, mx, my,
               Qt::CopyROP, true);
    }
    else
    {
        // fixed image edges
        bitBlt(this,  0,  0, m_effectPix,  0,  0, mx, my,
               Qt::CopyROP, true);
        bitBlt(this, x1,  0, m_effectPix, x1,  0, mx, my,
               Qt::CopyROP, true);
        bitBlt(this,  0, y1, m_effectPix,  0, y1, mx, my,
               Qt::CopyROP, true);
        bitBlt(this, x1, y1, m_effectPix, x1, y1, mx, my,
               Qt::CopyROP, true);
    }

    m_tmout = 20;
    m_i     = 1;
}

void SingleView::effectMultiCircleOut()
{
    int x, y, i;
    double alpha;
    static QPointArray pa(4);

    if (m_i == 0)
    {
        startPainter();
        mw = width();
        mh = height();
        mx = mw;
        my = mh>>1;
        pa.setPoint(0, mw>>1, mh>>1);
        pa.setPoint(3, mw>>1, mh>>1);
        mfy = sqrt((double)mw*mw + mh*mh) / 2;
        mi  = rand()%15 + 2;
        mfd = M_PI*2/mi;
        mAlpha = mfd;
        mwait = 10 * mi;
        mfx = M_PI/32;  // divisor must be powers of 8
    }

    if (mAlpha < 0)
    {
        m_painter->end();

        m_tmout = -1;
        m_effectRunning = false;
        update();
        return;
    }

    for (alpha=mAlpha, i=mi; i>=0; i--, alpha+=mfd)
    {
        x = (mw>>1) + (int)(mfy * cos(-alpha));
        y = (mh>>1) + (int)(mfy * sin(-alpha));

        mx = (mw>>1) + (int)(mfy * cos(-alpha + mfx));
        my = (mh>>1) + (int)(mfy * sin(-alpha + mfx));

        pa.setPoint(1, x, y);
        pa.setPoint(2, mx, my);

        m_painter->drawPolygon(pa);
    }

    mAlpha -= mfx;

    m_tmout = mwait;
    m_i     = 1;
}

void SingleView::effectSpiralIn()
{
    if (m_i == 0)
    {
        startPainter();
        mw = width();
        mh = height();
        mix = mw / 8;
        miy = mh / 8;
        mx0 = 0;
        mx1 = mw - mix;
        my0 = miy;
        my1 = mh - miy;
        mdx = mix;
        mdy = 0;
        mi = 0;
        mj = 16 * 16;
        mx = 0;
        my = 0;
    }

    if (mi==0 && mx0>=mx1)
    {
        m_painter->end();

        m_tmout = -1;
        m_effectRunning = false;
        update();
        return;
    }

    if (mi==0 && mx>=mx1) // switch to: down on right side
    {
        mi = 1;
        mdx = 0;
        mdy = miy;
        mx1 -= mix;
    }
    else if (mi==1 && my>=my1) // switch to: right to left on bottom
                               // side
    {
        mi = 2;
        mdx = -mix;
        mdy = 0;
        my1 -= miy;
    }
    else if (mi==2 && mx<=mx0) // switch to: up on left side
    {
        mi = 3;
        mdx = 0;
        mdy = -miy;
        mx0 += mix;
    }
    else if (mi==3 && my<=my0) // switch to: left to right on top side
    {
        mi = 0;
        mdx = mix;
        mdy = 0;
        my0 += miy;
    }

    bitBlt(this, mx, my, m_effectPix, mx, my, mix, miy,
           Qt::CopyROP, true);

    mx += mdx;
    my += mdy;
    mj--;

    m_tmout = 8;
    m_i     = 1;
}

void SingleView::effectCircleOut()
{
    int x, y;
    static QPointArray pa(4);

    if (m_i == 0)
    {
        startPainter();
        mw = width();
        mh = height();
        mx = mw;
        my = mh>>1;
        mAlpha = 2*M_PI;
        pa.setPoint(0, mw>>1, mh>>1);
        pa.setPoint(3, mw>>1, mh>>1);
        mfx = M_PI/16;  // divisor must be powers of 8
        mfy = sqrt((double)mw*mw + mh*mh) / 2;
    }

    if (mAlpha < 0)
    {
        m_painter->end();
        
        m_tmout = -1;
        m_effectRunning = false;
        update();
        return;
    }

    x = mx;
    y = my;
    mx = (mw>>1) + (int)(mfy * cos(mAlpha));
    my = (mh>>1) + (int)(mfy * sin(mAlpha));
    mAlpha -= mfx;

    pa.setPoint(1, x, y);
    pa.setPoint(2, mx, my);

    m_painter->drawPolygon(pa);

    m_tmout = 20;
    m_i     = 1;
}

void SingleView::effectBlobs()
{
    int r;

    if (m_i == 0)
    {
        startPainter();
        mAlpha = M_PI * 2;
        mw = width();
        mh = height();
        mi = 150;
    }

    if (mi <= 0)
    {
        m_painter->end();

        m_tmout = -1;
        m_effectRunning = false;
        update();
        return;
    }

    mx = rand() % mw;
    my = rand() % mh;
    r = (rand() % 200) + 50;

    m_painter->drawEllipse(mx-r, my-r, r, r);
    mi--;

    m_tmout = 10;
    m_i     = 1;
}

void SingleView::effectNoise()
{
    int x, y, i, w, h, fact, sz;

    fact = (rand() % 3) + 1;

    w = width() >> fact;
    h = height() >> fact;
    sz = 1 << fact;

    for (i = (w*h)<<1; i > 0; i--)
    {
        x = (rand() % w) << fact;
        y = (rand() % h) << fact;
        bitBlt(this, x, y, m_effectPix,
               x, y, sz, sz, Qt::CopyROP, true);
    }

    m_tmout = -1;
    m_effectRunning = false;
    update();
    return;
}



void SingleView::slotTimeOut()
{
    if (!m_effectMethod) {
        std::cerr << "SingleView: No transition method"
                  << std::endl;
        return;
    }

    if (!m_effectRunning) {

        if (m_tmout == -1) {
            // effect was running and is complete now
            // run timer while showing current image
            m_tmout = m_delay * 1000;
            m_i = 0;
        }
        else {

            // timed out after showing current image
            // load next image and start effect

            if (m_effectRandom)
                m_effectMethod = getRandomEffect();

             if (m_slideShow > 1)
                 randomFrame();
             else
                 advanceFrame();

            bool wasMovie = m_movieState > 0;
            loadImage();
            bool isMovie = m_movieState > 0;
            // If transitioning to/from a movie, don't do an effect,
            // and shorten timeout
            if (wasMovie || isMovie)
            {
                m_tmout = 1;
            }
            else {
                createEffectPix();
                m_effectRunning = true;
                m_tmout = 10;
                m_i = 0;
            }
       }   
    }

    update();
    m_timer->start(m_tmout, true);
}
