/* ============================================================
 * File  : regslideshow.h
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

#ifndef REGSLIDESHOW_H
#define REGSLIDESHOW_H

#include <qmap.h>

#include "iconview.h"

class QTimer;
class QPixmap;

class RegSlideShow : public MythDialog
{
    Q_OBJECT

public:

    RegSlideShow(QSqlDatabase *db, const ThumbList& itemList,
                 int pos, MythMainWindow *parent, const char *name=0);
    ~RegSlideShow();

protected:

    void paintEvent(QPaintEvent *e);
    void keyPressEvent(QKeyEvent *e);
    
private slots:

    void slotTimeOut();
    
private:

    void  loadNext();
    void  startPainter();
    void  createPausePix();

    void  effectNone();
    void  effectChessboard();
    void  effectSweep();
    void  effectGrowing();
    void  effectHorizLines();
    void  effectVertLines();
    void  effectMeltdown();  
    void  effectIncomingEdges();
    void  effectMultiCircleOut();
    void  effectSpiralIn();
    void  effectCircleOut();
    void  effectBlobs();
    void  effectNoise();

    typedef void               (RegSlideShow::*EffectMethod)();
    void                       registerEffects();
    EffectMethod               getRandomEffect();
    EffectMethod               m_effectMethod;
    QMap<QString,EffectMethod> m_effectMap;
    bool                       m_effectRandom;

    QSqlDatabase *m_db;
    ThumbList     m_itemList;
    QTimer       *m_timer;
    QPixmap      *m_pixmaps[2];
    QPainter     *m_painter;
    
    int           m_pos;
    int           m_curr;
    int           m_load1First;
    int           m_tmout;
    int           m_delay;
    bool          m_effectRunning;
    bool          m_paused;
    QPixmap      *m_pausePix;
    
    int           m_i;
    int           mx, my, mw, mh, mdx, mdy, mix, miy, mi, mj, mSubType;
    int           mx0, my0, mx1, my1, mwait;
    double        mfx, mfy, mAlpha, mfd;
    int*          mIntArray;
};

#endif /* REGSLIDESHOW_H */
