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

#include "iconview.h"

class QTimer;
class ThumbGenerator;

class SingleView : public MythDialog
{
    Q_OBJECT

public:

    SingleView(QSqlDatabase *db, const QPtrList<ThumbItem>& images,
               int pos, ThumbGenerator *thumbGen, bool slideShow,
               MythMainWindow *parent, const char *name=0);
    ~SingleView();

    void startShow();
    void stopShow();

protected:

    void paintEvent(QPaintEvent *e);
    void keyPressEvent(QKeyEvent *e);
    
private slots:

    void slotTimeOut();
    
private:

    void loadNext();
    void loadPrev();
    void loadImage();
    void rotate(int angle);
    void zoom();
    void createInfoBg();

    QSqlDatabase          *m_db;
    ThumbGenerator        *m_thumbGen;
    QPtrList<ThumbItem>    m_imageList;

    QPixmap               *m_pixmap;
    QImage                 m_image;
    int                    m_pos;
                          
    int                    m_rotateAngle;
    float                  m_zoom;
    int                    m_sx;
    int                    m_sy;
                          
    bool                   m_info;
    QPixmap               *m_infoBgPix;
                          
    QTimer                *m_timer;
    int                    m_timerSecs;
    bool                   m_timerRunning;

    // transition effects 

    typedef                   int (SingleView::*transMethod)(bool);
                              
    void                      loadTransPixmapNext();
    void                      startTransPainter();
    void                      registerTrans();
    transMethod               getRandomTrans();

    int                       transNone(bool aInit);
    int                       transChessboard(bool aInit);
    int                       transSweep(bool aInit);
    int                       transGrowing(bool aInit);
    int                       transHorizLines(bool aInit);
    int                       transVertLines(bool aInit);
    int                       transMeltdown(bool aInit);  
    int                       transIncomingEdges(bool aInit);
    int                       transMultiCircleOut(bool aInit);
    int                       transSpiralIn(bool aInit);
    int                       transCircleOut(bool aInit);
    int                       transBlobs(bool aInit);
    int                       transNoise(bool aInit);
                           
    QPixmap                  *m_transPix;
    bool                      m_transRunning;
    QPainter                 *m_transPainter;
    transMethod               m_transMethod;
    bool                      m_transRandom;
    QMap<QString,transMethod> m_transMap;
    
    int          mx, my, mw, mh, mdx, mdy, mix, miy, mi, mj, mSubType;
    int          mx0, my0, mx1, my1, mwait;
    double       mfx, mfy, mAlpha, mfd;
    int*         mIntArray;
};

#endif /* SINGLEVIEW_H */
