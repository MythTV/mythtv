/* ============================================================
 * File  : regslideshow.cpp
 * Author: Renchi Raju <renchi@pooh.tam.uiuc.edu>
 * Date  : 2004-01-12
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

#include <iostream>
#include <math.h>

#include <qsqldatabase.h>
#include <qimage.h>
#include <qpointarray.h>
#include <qfontmetrics.h>

#include "regslideshow.h"

RegSlideShow::RegSlideShow(QSqlDatabase *db, const ThumbList& itemList,
                           int pos, MythMainWindow *parent, const char *name )
    : MythDialog(parent, name)
{
    m_db       = db;
    m_pos      = pos;
    m_itemList = itemList;
    
    // ---------------------------------------------------------------
    
    registerEffects();

    // ---------------------------------------------------------------
    
    m_itemList.setAutoDelete(false);
    // remove all dirs from m_itemList;
    ThumbItem* item = m_itemList.first();
    while (item) {
        ThumbItem* next = m_itemList.next();
        if (item->isDir)
            m_itemList.remove();
        item = next;
    }

    // ---------------------------------------------------------------

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

    // ---------------------------------------------------------------

    m_curr       = 0;
    m_tmout      = m_delay * 1000;
    m_load1First = true;
    m_pixmaps[0] = 0;
    m_pixmaps[1] = 0;
    m_effectRunning = false;
    m_i = 0;
    m_painter  = 0;
    mIntArray  = 0;

    m_paused   = false;
    m_pausePix = 0;
    
    loadNext();

    // ---------------------------------------------------------------

    m_timer = new QTimer(this);
    connect(m_timer, SIGNAL(timeout()), SLOT(slotTimeOut()));
    m_timer->start(m_tmout, true);

    // ---------------------------------------------------------------

    setNoErase();
    QString bgtype = gContext->GetSetting("SlideshowBackground");
    if (bgtype != "theme" && !bgtype.isEmpty())
        setPalette(QPalette(QColor(bgtype)));

}

RegSlideShow::~RegSlideShow()
{
    m_timer->stop();
    delete m_timer;

    if (m_painter) {
        if (m_painter->isActive())
            m_painter->end();
        delete m_painter;
    }

    if (m_pixmaps[0])
        delete m_pixmaps[0];
    if (m_pixmaps[1])
        delete m_pixmaps[1];

    if (m_pausePix)
        delete m_pausePix;
    
    if (mIntArray)
        delete [] mIntArray;
}

void RegSlideShow::paintEvent(QPaintEvent *)
{
    if (!m_pixmaps[m_curr])
        return;

    if (!m_effectRunning) {
        bitBlt(this, 0, 0, m_pixmaps[m_curr], 0, 0, -1, -1,
               Qt::CopyROP, true);
        if (m_paused) {
            if (!m_pausePix)
                createPausePix();
            bitBlt(this, screenwidth - m_pausePix->width() - (int)(20*wmult),
                   (int)(20*hmult), m_pausePix,
                   0, 0, -1, -1, Qt::CopyROP, false);
        }
    }
    else {
        (this->*m_effectMethod)();
    }
}

void RegSlideShow::keyPressEvent(QKeyEvent *e)
{
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("Gallery", e, actions);

    bool handled = false;
    
    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        handled = true;

        QString action = actions[i];
        if (action == "PLAY")
        {
            m_paused = !m_paused;

            if (m_paused) {
                m_timer->stop();
                if (m_painter && m_painter->isActive())
                    m_painter->end();
                m_tmout = -1;
                m_effectRunning = false;
            }
            else {
                m_tmout = m_delay * 1000;
                m_timer->start(m_tmout, true);
            }

        }
        else 
            handled = false;
    }

    if (handled) {
        update();
    }
    else {
        MythDialog::keyPressEvent(e);
    }
}

void RegSlideShow::slotTimeOut()
{
    if (!m_effectMethod) {
        std::cerr << "RegSlideShow: No transition method"
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

            m_load1First = !m_load1First;
            m_curr = m_load1First ? 0 : 1;
            loadNext();

            m_tmout = 10;
            m_effectRunning = true;
            m_i = 0;
        }
    }

    update();
    m_timer->start(m_tmout, true);
}

void RegSlideShow::loadNext()
{
    ThumbItem *item = m_itemList.at(m_pos);
    if (!item) {
        std::cerr << "The impossible happened: "
                  << "No item in list at " << m_pos
                  << std::endl;
        return;
    }

    QImage image(item->path);
    if (!image.isNull()) {

        QString queryStr = "SELECT angle FROM gallerymetadata WHERE "
                           "image=\"" + item->path + "\";";
        QSqlQuery query = m_db->exec(queryStr);
        
        if (query.isActive()  && query.numRowsAffected() > 0) 
        {
            query.next();
            int rotateAngle = query.value(0).toInt();
            if (rotateAngle != 0) {
                    QWMatrix matrix;
                    matrix.rotate(rotateAngle);
                    image = image.xForm(matrix);
            }
        }

        image = image.smoothScale(screenwidth, screenheight,
                                  QImage::ScaleMin);
        QPixmap pix(image);

        int a = 0;
        if (!m_load1First)
            a = 1;

        if (!m_pixmaps[a])
            m_pixmaps[a] = new QPixmap(screenwidth, screenheight);

        m_pixmaps[a]->fill(this, 0, 0);

        bitBlt( m_pixmaps[a],
               (screenwidth-pix.width())/2,
               (screenheight-pix.height())/2,
                &pix, 0, 0, -1, -1,
                Qt::CopyROP, true);
    }
    
    m_pos++;
    if (m_pos >= (int)m_itemList.count())
        m_pos = 0;
   
}

void RegSlideShow::registerEffects()
{
    m_effectMap.insert("none", &RegSlideShow::effectNone);
    m_effectMap.insert("chess board", &RegSlideShow::effectChessboard);
    m_effectMap.insert("melt down", &RegSlideShow::effectMeltdown);
    m_effectMap.insert("sweep", &RegSlideShow::effectSweep);
    m_effectMap.insert("noise", &RegSlideShow::effectNoise);
    m_effectMap.insert("growing", &RegSlideShow::effectGrowing);
    m_effectMap.insert("incoming edges", &RegSlideShow::effectIncomingEdges);
    m_effectMap.insert("horizontal lines", &RegSlideShow::effectHorizLines);
    m_effectMap.insert("vertical lines", &RegSlideShow::effectVertLines);
    m_effectMap.insert("circle out", &RegSlideShow::effectCircleOut);
    m_effectMap.insert("multicircle out", &RegSlideShow::effectMultiCircleOut);
    m_effectMap.insert("spiral in", &RegSlideShow::effectSpiralIn);
    m_effectMap.insert("blobs", &RegSlideShow::effectBlobs);
}

RegSlideShow::EffectMethod RegSlideShow::getRandomEffect()
{
    QMap<QString,EffectMethod> tmpMap(m_effectMap);
    tmpMap.remove("none");
    
    QStringList t = tmpMap.keys();

    int count = t.count();
    int i = (int)((float)(count)*rand()/(RAND_MAX+1.0));
    QString key = t[i];

    return tmpMap[key];
}

void RegSlideShow::effectNone()
{
    m_effectRunning = false;
    m_tmout = -1;
    update();
    return;
}

void RegSlideShow::effectChessboard()
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
         bitBlt(this, mix, y+miy, m_pixmaps[m_curr], mix, y+miy,
                mdx, mdy, Qt::CopyROP, true);
         bitBlt(this, mx, y+my, m_pixmaps[m_curr], mx, y+my,
                mdx, mdy, Qt::CopyROP, true);
    }

    m_tmout = mwait;

    m_i = 1;
}

void RegSlideShow::effectSweep()
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
            bitBlt(this, x, 0, m_pixmaps[m_curr],
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
            bitBlt(this, 0, y, m_pixmaps[m_curr],
                   0, y, mw, h, Qt::CopyROP, true);
        }
        my += mdy;
    }

    m_tmout = 20;
    m_i = 1;
}

void RegSlideShow::effectGrowing()
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

    bitBlt(this, mx, my, m_pixmaps[m_curr], mx, my,
           mw - (mx<<1), mh - (my<<1), Qt::CopyROP, true);

    m_tmout = 20;
    m_i     = 1;
}

void RegSlideShow::effectHorizLines()
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
        bitBlt(this, 0, y, m_pixmaps[m_curr],
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

void RegSlideShow::effectVertLines()
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
        bitBlt(this, x, 0, m_pixmaps[m_curr],
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

void RegSlideShow::effectMeltdown()
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
        bitBlt(this, x, y, m_pixmaps[m_curr], x, y, mdx, mdy,
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

void RegSlideShow::effectIncomingEdges()
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
        bitBlt(this,  0,  0, m_pixmaps[m_curr], mix-mx, miy-my, mx, my,
               Qt::CopyROP, true);
        bitBlt(this, x1,  0, m_pixmaps[m_curr], mix, miy-my, mx, my,
               Qt::CopyROP, true);
        bitBlt(this,  0, y1, m_pixmaps[m_curr], mix-mx, miy, mx, my,
               Qt::CopyROP, true);
        bitBlt(this, x1, y1, m_pixmaps[m_curr], mix, miy, mx, my,
               Qt::CopyROP, true);
    }
    else
    {
        // fixed image edges
        bitBlt(this,  0,  0, m_pixmaps[m_curr],  0,  0, mx, my,
               Qt::CopyROP, true);
        bitBlt(this, x1,  0, m_pixmaps[m_curr], x1,  0, mx, my,
               Qt::CopyROP, true);
        bitBlt(this,  0, y1, m_pixmaps[m_curr],  0, y1, mx, my,
               Qt::CopyROP, true);
        bitBlt(this, x1, y1, m_pixmaps[m_curr], x1, y1, mx, my,
               Qt::CopyROP, true);
    }

    m_tmout = 20;
    m_i     = 1;
}

void RegSlideShow::effectMultiCircleOut()
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

void RegSlideShow::effectSpiralIn()
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

    bitBlt(this, mx, my, m_pixmaps[m_curr], mx, my, mix, miy,
           Qt::CopyROP, true);

    mx += mdx;
    my += mdy;
    mj--;

    m_tmout = 8;
    m_i     = 1;
}

void RegSlideShow::effectCircleOut()
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

void RegSlideShow::effectBlobs()
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

void RegSlideShow::effectNoise()
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
        bitBlt(this, x, y, m_pixmaps[m_curr],
               x, y, sz, sz, Qt::CopyROP, true);
    }

    m_tmout = -1;
    m_effectRunning = false;
    update();
    return;
}

void RegSlideShow::startPainter()
{
    if (!m_painter)
        m_painter = new QPainter();
    if (m_painter->isActive())
        m_painter->end();

    QBrush brush;
    if (m_pixmaps[m_curr])
        brush.setPixmap(*m_pixmaps[m_curr]);

    m_painter->begin(this);
    m_painter->setBrush(brush);
    m_painter->setPen(Qt::NoPen);
}

void RegSlideShow::createPausePix()
{
    setFont(gContext->GetBigFont());
    
    QFontMetrics fm(font());
    QSize sz = fm.size(Qt::SingleLine, "Paused");
    QImage img(sz.width() + (int)(20.0*wmult),
               sz.height() + (int)(20.0*hmult), 32);
    img.setAlphaBuffer(true);

        for (int y = 0; y < img.height(); y++) 
    {
        for (int x = 0; x < img.width(); x++) 
        {
            uint *p = (uint *)img.scanLine(y) + x;
            *p = qRgba(0, 0, 0, 120);
        }
    }

    m_pausePix = new QPixmap(img);

    QPainter p(m_pausePix, this);
    p.setPen(Qt::white);
    p.drawText((int)(10.0*wmult), (int)(10.0*hmult),
               sz.width(), sz.height(), Qt::AlignCenter,
               "Paused");
    p.end();
}
