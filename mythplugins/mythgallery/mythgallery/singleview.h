/* ============================================================
 * File  : singleview.h
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

#ifndef SINGLEVIEW_H
#define SINGLEVIEW_H

#include <qimage.h>

#include "iconview.h"
#include "sequence.h"

class QTimer;

class SingleView : public MythDialog
{
    Q_OBJECT
    
public:

    SingleView(ThumbList itemList, int pos, int slideShow, 
               MythMainWindow *parent, const char *name=0);
    ~SingleView();

protected:

    void paintEvent(QPaintEvent *e);
    void keyPressEvent(QKeyEvent *e);

private:
    
    ThumbList     m_itemList;
    int           m_pos;
    
    int           m_movieState;
    QPixmap      *m_pixmap;
    QImage        m_image;
                 
    int           m_rotateAngle;
    float         m_zoom;
    int           m_sx;
    int           m_sy;
                 
    bool          m_info;
    QPixmap      *m_infoBgPix;

    int           m_showcaption;
    QPixmap      *m_captionBgPix;
    QPixmap      *m_captionbackup;
    QTimer       *m_ctimer;

    int           m_tmout;
    int           m_delay;
    bool          m_effectRunning;
    bool          m_running;
    int           m_slideShow;
    QTimer       *m_sstimer;
    QPixmap      *m_effectPix;
    QPainter     *m_painter;
    
    int           m_i;
    int           mx, my, mw, mh;
    int           mdx, mdy, mix, miy, mi, mj, mSubType;
    int           mx0, my0, mx1, my1, mwait;
    double        mfx, mfy, mAlpha, mfd;
    int*          mIntArray;

    typedef void               (SingleView::*EffectMethod)();
    EffectMethod                m_effectMethod;
    QMap<QString,EffectMethod>  m_effectMap;
    bool                        m_effectRandom;
    SequenceBase               *m_sequence;

private:

    void  advanceFrame();
    void  retreatFrame();
    void  loadImage();
    void  rotate(int angle);
    void  zoom();
    QPixmap *createBg(int width, int height);

    void  registerEffects();
    EffectMethod getRandomEffect();
    void  startPainter();
    void  createEffectPix();

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

private slots:

    void  slotSlideTimeOut();
    void  slotCaptionTimeOut();
};

#endif /* SINGLEVIEW_H */
