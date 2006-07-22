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

#include <vector>
using namespace std;

#include <qimage.h>
#include <qpointarray.h>

#include "iconview.h"
#include "sequence.h"

class QTimer;

class SingleView : public MythDialog
{
    Q_OBJECT
    
  public:
    SingleView(ThumbList itemList, int pos, int slideShow, int sortorder,
               MythMainWindow *parent, const char *name=0);
    ~SingleView();

    // Effect subtype enumeration
    static const int kSweepRightToLeft    = 0;
    static const int kSweepLeftToRight    = 1;
    static const int kSweepBottomToTop    = 2;
    static const int kSweepTopToBottom    = 3;

    static const int kIncomingEdgesFixed  = 0;
    static const int kIncomingEdgesMoving = 1;

  protected:
    // Initialize
    void  RegisterEffects(void);

    // Commands
    void  Rotate(int angle);
    void  DisplayNext(bool reset, bool loadImage);
    void  DisplayPrev(bool reset, bool loadImage);
    void  LoadImage(void);
    virtual void paintEvent(QPaintEvent *e);
    virtual void keyPressEvent(QKeyEvent *e);

    // Sets
    void  SetZoom(float zoom);

    // Gets
    QString GetDescription(const ThumbItem *item, const QSize &sz);
    typedef void (SingleView::*EffectMethod)();
    EffectMethod GetRandomEffect(void);

  private:
    void  StartPainter(void);
    void  CreateEffectPixmap(void);

    // Various special effects methods
    void  EffectNone(void);
    void  EffectChessboard(void);
    void  EffectSweep(void);
    void  EffectGrowing(void);
    void  EffectHorizLines(void);
    void  EffectVertLines(void);
    void  EffectMeltdown(void);  
    void  EffectIncomingEdges(void);
    void  EffectMultiCircleOut(void);
    void  EffectSpiralIn(void);
    void  EffectCircleOut(void);
    void  EffectBlobs(void);
    void  EffectNoise(void);

    static QPixmap *CreateBackground(const QSize &sz);

  private slots:
    void  slotSlideTimeOut(void);
    void  slotCaptionTimeOut(void);

  private:
    int           m_pos;
    ThumbList     m_itemList;

    int           m_movieState;
    QPixmap      *m_pixmap;
    QImage        m_image;

    int           m_rotateAngle;
    float         m_zoom;
    QPoint        m_source_loc;

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

    EffectMethod                m_effectMethod;
    QMap<QString,EffectMethod>  m_effectMap;
    bool                        m_effectRandom;
    SequenceBase               *m_sequence;

    // Common effect state variables
    int           m_effect_current_frame;
    int           m_effect_subtype;
    QRect         m_effect_bounds;    ///< effect image bounds
    QPoint        m_effect_delta0;    ///< misc effects delta
    QPoint        m_effect_delta1;    ///< misc effects delta
    int           m_effect_i;         ///< misc effects iterator
    int           m_effect_j;         ///< misc effects iterator
    int           m_effect_framerate; ///< timeout between effects
    float         m_effect_delta2_x;
    float         m_effect_delta2_y;
    float         m_effect_alpha;

    // Unshared effect state variables
    QPoint        m_effect_spiral_tmp0;
    QPoint        m_effect_spiral_tmp1;
    vector<int>   m_effect_meltdown_y_disp;
    float         m_effect_multi_circle_out_delta_alpha;
    QPointArray   m_effect_milti_circle_out_points;
    QPointArray   m_effect_circle_out_points;
};

#endif /* SINGLEVIEW_H */
