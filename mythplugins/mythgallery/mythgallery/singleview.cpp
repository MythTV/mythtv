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
#include <qpointarray.h>

extern "C"
{
#include <math.h>
}

#include "thumbgenerator.h"
#include "singleview.h"

SingleView::SingleView(QSqlDatabase *db, const QPtrList<ThumbItem>& images,
                       int pos, ThumbGenerator *thumbGen, bool slideShow,
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

    setNoErase();
    QString bgtype = gContext->GetSetting("SlideshowBackground");
    if (bgtype != "theme" && !bgtype.isEmpty())
        setPalette(QPalette(QColor(bgtype)));

    registerTrans();

    m_transMethod = 0;
    m_transRandom = false;
    QString transType = gContext->GetSetting("SlideshowTransition");
    if (!transType.isEmpty() && m_transMap.contains(transType))
        m_transMethod = m_transMap[transType];

    if (!m_transMethod || transType == "random") {
        m_transMethod = getRandomTrans();
        m_transRandom = true;
    }
    
    m_timer = new QTimer(this);
    connect(m_timer, SIGNAL(timeout()), SLOT(slotTimeOut()));
    m_timerSecs = gContext->GetNumSetting("SlideshowDelay");
    if (!m_timerSecs)
        m_timerSecs = 2;

    m_transRunning = false;
    m_transPix     = 0;
    m_transPainter = 0;
    mIntArray      = 0;


    if (slideShow) {
        loadPrev();
        startShow();
    }
    else
        loadImage();

}

SingleView::~SingleView()
{
    stopShow();
    
    if (m_pixmap)
        delete m_pixmap;

    if (m_infoBgPix)
        delete m_infoBgPix;

    if (m_transPix)
        delete m_transPix;

    if (mIntArray)
        delete [] mIntArray;

    if (m_transPainter)
        delete m_transPainter;
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
    stopShow();
    
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
    update();
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

void SingleView::startShow()
{
    m_timerRunning = true;
    m_transRunning = false;
    //m_timer->start(10);
    slotTimeOut();
}

void SingleView::stopShow()
{
    if (m_transPainter && m_transPainter->isActive())
        m_transPainter->end();
    m_timerRunning = false;
    m_transRunning = false;
    m_timer->stop();
}

void SingleView::slotTimeOut()
{
    if (!m_transMethod) {
        std::cerr << "SingleView: No transition method"
                  << std::endl;
        stopShow();
        return;
    }

    int tmout = -1;
    // Transition under progress ?
    if (m_transRunning)
    {
        tmout = (this->*m_transMethod)(false);
    }
    else {
        loadTransPixmapNext();

        if (m_transRandom) 
            m_transMethod = getRandomTrans();
        
        m_transRunning = true;
        tmout = (this->*m_transMethod)(true);
    }

    if (tmout <= 0)  // Transition finished-> delay.
    {
        tmout = m_timerSecs * 1000;
        m_transRunning = false;
    }

    m_timer->start(tmout, true);
}

void SingleView::loadTransPixmapNext()
{
    if (!m_transPix) 
        m_transPix = new QPixmap(screenwidth, screenheight);
    
    m_transPix->fill(this, 0, 0);

    loadNext();
    loadImage();

    if (m_pixmap) 
        bitBlt(m_transPix,
               (m_transPix->width()-m_pixmap->width())/2,
               (m_transPix->height()-m_pixmap->height())/2,
               m_pixmap, 0, 0, -1, -1, Qt::CopyROP);
}

void SingleView::registerTrans()
{
    m_transMap.insert("none", &SingleView::transNone);
    m_transMap.insert("chess board", &SingleView::transChessboard);
    m_transMap.insert("melt down", &SingleView::transMeltdown);
    m_transMap.insert("sweep", &SingleView::transSweep);
    m_transMap.insert("noise", &SingleView::transNoise);
    m_transMap.insert("growing", &SingleView::transGrowing);
    m_transMap.insert("incoming edges", &SingleView::transIncomingEdges);
    m_transMap.insert("horizontal lines", &SingleView::transHorizLines);
    m_transMap.insert("vertical lines", &SingleView::transVertLines);
    m_transMap.insert("circle out", &SingleView::transCircleOut);
    m_transMap.insert("multicircle out", &SingleView::transMultiCircleOut);
    m_transMap.insert("spiral in", &SingleView::transSpiralIn);
    m_transMap.insert("blobs", &SingleView::transBlobs);
}

SingleView::transMethod SingleView::getRandomTrans()
{
    QStringList t = m_transMap.keys();
    t.remove("none");

    int count = t.count();

    int i = rand() % count;
    QString key = t[i];

    return m_transMap[key];
}

void SingleView::startTransPainter()
{
    if (!m_transPainter)
        m_transPainter = new QPainter();
    if (m_transPainter->isActive())
        m_transPainter->end();

    QBrush brush;
    if (m_transPix)
        brush.setPixmap(*m_transPix);
    m_transPainter->begin(this);
    m_transPainter->setBrush(brush);
    m_transPainter->setPen(Qt::NoPen);
}

int SingleView::transNone(bool)
{
    update();
    return -1;
}


int SingleView::transChessboard(bool aInit)
{
    if (aInit)
    {
        mw  = width();
        mh  = height();
        mdx = 8;         // width of one tile
        mdy = 8;         // height of one tile
        mj  = (mw+mdx-1)/mdx; // number of tiles
        mx  = mj*mdx;    // shrinking x-offset from screen border
        mix = 0;         // growing x-offset from screen border
        miy = 0;         // 0 or mdy for growing tiling effect
        my  = mj&1 ? 0 : mdy; // 0 or mdy for shrinking tiling effect
        mwait = 800 / mj; // timeout between effects
    }

    if (mix >= mw)
    {
        update();
        return -1;
    }

    mix += mdx;
    mx  -= mdx;
    miy = miy ? 0 : mdy;
    my  = my ? 0 : mdy;

    for (int y=0; y<mw; y+=(mdy<<1))
    {
         bitBlt(this, mix, y+miy, m_transPix, mix, y+miy,
                mdx, mdy, Qt::CopyROP, true);
         bitBlt(this, mx, y+my, m_transPix, mx, y+my,
                mdx, mdy, Qt::CopyROP, true);
    }

    return mwait;
}

int SingleView::transSweep(bool aInit)
{
    int w, h, x, y, i;

    if (aInit)
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
            return -1;
        }
        for (w=2,i=4,x=mx; i>0; i--, w<<=1, x-=mdx)
        {
            bitBlt(this, x, 0, m_transPix,
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
            return -1;
        }
        for (h=2,i=4,y=my; i>0; i--, h<<=1, y-=mdy)
        {
            bitBlt(this, 0, y, m_transPix,
                   0, y, mw, h, Qt::CopyROP, true);
        }
        my += mdy;
    }

    return 20;
}


int SingleView::transGrowing(bool aInit)
{
    if (aInit)
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
        update();
        return -1;
    }

    bitBlt(this, mx, my, m_transPix, mx, my,
           mw - (mx<<1), mh - (my<<1), Qt::CopyROP, true);

    return 20;
}


int SingleView::transHorizLines(bool aInit)
{
    static int iyPos[] = { 0, 4, 2, 6, 1, 5, 3, 7, -1 };
    int y;

    if (aInit)
    {
        mw = width();
        mh = height();
        mi = 0;
    }

    if (iyPos[mi] < 0)
        return -1;

    for (y=iyPos[mi]; y<mh; y+=8)
    {
        bitBlt(this, 0, y, m_transPix,
               0, y, mw, 1, Qt::CopyROP, true);
    }

    mi++;
    
    if (iyPos[mi] >= 0)
        return 160;

    return -1;
}

int SingleView::transVertLines(bool aInit)
{
    static int ixPos[] = { 0, 4, 2, 6, 1, 5, 3, 7, -1 };
    int x;

    if (aInit)
    {
        mw = width();
        mh = height();
        mi = 0;
    }

    if (ixPos[mi] < 0)
        return -1;

    for (x=ixPos[mi]; x<mw; x+=8)
    {
        bitBlt(this, x, 0, m_transPix,
               x, 0, 1, mh, Qt::CopyROP, true);
    }

    mi++;
    if (ixPos[mi] >= 0)
        return 160;
    return -1;
}

int SingleView::transMeltdown(bool aInit)
{
    int i, x, y;
    bool done;

    if (aInit)
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
        bitBlt(this, x, y, m_transPix, x, y, mdx, mdy, Qt::CopyROP, true);
        mIntArray[i] += mdy;
    }

    if (done)
    {
        delete [] mIntArray;
        mIntArray = 0;
        return -1;
    }

    return 15;
}

int SingleView::transIncomingEdges(bool aInit)
{
    int x1, y1;

    if (aInit)
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
        update();
        return -1;
    }

    x1 = mw - mx;
    y1 = mh - my;
    mi++;

    if (mSubType)
    {
        // moving image edges
        bitBlt(this,  0,  0, m_transPix, mix-mx, miy-my, mx, my, Qt::CopyROP, true);
        bitBlt(this, x1,  0, m_transPix, mix, miy-my, mx, my, Qt::CopyROP, true);
        bitBlt(this,  0, y1, m_transPix, mix-mx, miy, mx, my, Qt::CopyROP, true);
        bitBlt(this, x1, y1, m_transPix, mix, miy, mx, my, Qt::CopyROP, true);
    }
    else
    {
        // fixed image edges
        bitBlt(this,  0,  0, m_transPix,  0,  0, mx, my, Qt::CopyROP, true);
        bitBlt(this, x1,  0, m_transPix, x1,  0, mx, my, Qt::CopyROP, true);
        bitBlt(this,  0, y1, m_transPix,  0, y1, mx, my, Qt::CopyROP, true);
        bitBlt(this, x1, y1, m_transPix, x1, y1, mx, my, Qt::CopyROP, true);
    }
    return 20;
}

int SingleView::transMultiCircleOut(bool aInit)
{
    int x, y, i;
    double alpha;
    static QPointArray pa(4);

    if (aInit)
    {
        startTransPainter();
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
        m_transPainter->end();
        update();
        return -1;
    }

    for (alpha=mAlpha, i=mi; i>=0; i--, alpha+=mfd)
    {
        x = (mw>>1) + (int)(mfy * cos(-alpha));
        y = (mh>>1) + (int)(mfy * sin(-alpha));

        mx = (mw>>1) + (int)(mfy * cos(-alpha + mfx));
        my = (mh>>1) + (int)(mfy * sin(-alpha + mfx));

        pa.setPoint(1, x, y);
        pa.setPoint(2, mx, my);

        m_transPainter->drawPolygon(pa);
    }

    mAlpha -= mfx;

    return mwait;
}

int SingleView::transSpiralIn(bool aInit)
{
    if (aInit)
    {
        startTransPainter();
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
        m_transPainter->end();
        update();
        return -1;
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

    bitBlt(this, mx, my, m_transPix, mx, my, mix, miy,
           Qt::CopyROP, true);

    mx += mdx;
    my += mdy;
    mj--;

    return 8;
}

int SingleView::transCircleOut(bool aInit)
{
    int x, y;
    static QPointArray pa(4);

    if (aInit)
    {
        startTransPainter();
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
        m_transPainter->end();
        update();
        return -1;
    }

    x = mx;
    y = my;
    mx = (mw>>1) + (int)(mfy * cos(mAlpha));
    my = (mh>>1) + (int)(mfy * sin(mAlpha));
    mAlpha -= mfx;

    pa.setPoint(1, x, y);
    pa.setPoint(2, mx, my);

    m_transPainter->drawPolygon(pa);

    return 20;
}

int SingleView::transBlobs(bool aInit)
{
    int r;

    if (aInit)
    {
        startTransPainter();
        mAlpha = M_PI * 2;
        mw = width();
        mh = height();
        mi = 150;
    }

    if (mi <= 0)
    {
        m_transPainter->end();
        update();
        return -1;
    }

    mx = rand() % mw;
    my = rand() % mh;
    r = (rand() % 200) + 50;

    m_transPainter->drawEllipse(mx-r, my-r, r, r);
    mi--;

    return 10;
}

int SingleView::transNoise(bool aInit)
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
        bitBlt(this, x, y, m_transPix,
               x, y, sz, sz, Qt::CopyROP, true);
    }

    update();

    return -1;
}

