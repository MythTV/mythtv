// -*- Mode: c++ -*-
/* ============================================================
 * File  : singleview.h
 * Description :  
 * 
 * Copyright 2004-2006 Renchi Raju, Daniel Kristjansson
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

#include "imageview.h"
#include "iconview.h"
#include "sequence.h"

class QTimer;

class SingleView : public MythDialog, public ImageView
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
    // Commands
    virtual void Rotate(int angle);
    virtual void DisplayNext(bool reset, bool loadImage);
    virtual void DisplayPrev(bool reset, bool loadImage);
    virtual void LoadImage(void);
    virtual void paintEvent(QPaintEvent *e);
    virtual void keyPressEvent(QKeyEvent *e);

    // Sets
    virtual void SetZoom(float zoom);
    void SetPixmap(QPixmap*);

    // Effects
    virtual void RegisterEffects(void);
    virtual void RunEffect(const QString &effect);

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
    void  SlideTimeout(void);
    void  CaptionTimeout(void);

  private:
    // General
    QPixmap      *m_pixmap;
    QImage        m_image;
    int           m_angle;
    QPoint        m_source_loc;

    // Info variables
    QPixmap      *m_info_pixmap;

    // Common slideshow variables
    int           m_caption_show;
    QPixmap      *m_caption_pixmap;
    QPixmap      *m_caption_restore_pixmap;
    QTimer       *m_caption_timer;

    // Common effect state variables
    QPixmap      *m_effect_pixmap;
    QPainter     *m_effect_painter;
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
