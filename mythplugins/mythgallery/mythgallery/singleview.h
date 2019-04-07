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

// Qt headers
#include <QImage>
#include <QPolygon>
#include <QPixmap>
#include <QKeyEvent>
#include <QPaintEvent>

// MythGallery headers
#include "mythdialogs.h"
#include "galleryutil.h"
#include "imageview.h"
#include "iconview.h"
#include "sequence.h"

class QTimer;

class SingleView : public MythDialog, public ImageView
{
    Q_OBJECT
    
  public:
    SingleView(const ThumbList& itemList, int *pos, int slideShow, int sortorder,
               MythMainWindow *parent, const char *name="SingleView");
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
    void Rotate(int angle) override; // ImageView
    void DisplayNext(bool reset, bool loadImage) override; // ImageView
    void DisplayPrev(bool reset, bool loadImage) override; // ImageView
    void Load(void) override; // ImageView
    void paintEvent(QPaintEvent *e) override; // QWidget
    void keyPressEvent(QKeyEvent *e) override; // QWidget

    // Sets
    void SetZoom(float zoom) override; // ImageView
    void SetPixmap(QPixmap*);

    // Effects
    void RegisterEffects(void) override; // ImageView
    void RunEffect(const QString &effect) override; // ImageView

  private:
    void  StartPainter(void);
    void  CreateEffectPixmap(void);
    void  CheckPosition(void);

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
    QPixmap      *m_pixmap                 {nullptr};
    QImage        m_image;
    int           m_angle                  {0};
    QPoint        m_source_loc             {0,0};
    ScaleMax      m_scaleMax               {kScaleToFit};

    // Info variables
    QPixmap      *m_info_pixmap            {nullptr};

    // Common slideshow variables
    int           m_caption_show           {0};
    bool          m_caption_remove         {false};
    QPixmap      *m_caption_pixmap         {nullptr};
    QPixmap      *m_caption_restore_pixmap {nullptr};
    QTimer       *m_caption_timer          {nullptr};

    // Common effect state variables
    QPixmap      *m_effect_pixmap    {nullptr};
    QPainter     *m_effect_painter   {nullptr};
    int           m_effect_subtype   {0};
    QRect         m_effect_bounds    {0,0,0,0}; ///< effect image bounds
    QPoint        m_effect_delta0    {0,0};     ///< misc effects delta
    QPoint        m_effect_delta1    {0,0};     ///< misc effects delta
    int           m_effect_i         {0};       ///< misc effects iterator
    int           m_effect_j         {0};       ///< misc effects iterator
    int           m_effect_framerate {0};       ///< timeout between effects
    float         m_effect_delta2_x  {0.0F};
    float         m_effect_delta2_y  {0.0F};
    float         m_effect_alpha     {0.0F};

    // Unshared effect state variables
    QPoint        m_effect_spiral_tmp0                  {0,0};
    QPoint        m_effect_spiral_tmp1                  {0,0};
    vector<int>   m_effect_meltdown_y_disp;
    float         m_effect_multi_circle_out_delta_alpha {0.0F};
    QPolygon      m_effect_milti_circle_out_points      {4};
    QPolygon      m_effect_circle_out_points            {4};
};

#endif /* SINGLEVIEW_H */
