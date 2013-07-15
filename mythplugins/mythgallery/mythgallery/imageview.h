// -*- Mode: c++ -*-
/* ============================================================
 * File  : imageview.h
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

#ifndef IMAGEVIEW_H
#define IMAGEVIEW_H

#include <vector>
using namespace std;

// MythGallery headers
#include "iconview.h"
#include "sequence.h"

class QImage;
class QTimer;
class ThumbItem;

class ImageView
{
     Q_DECLARE_TR_FUNCTIONS(ImageView)

  public:
    ImageView(const ThumbList &itemList,
              int *pos, int slideShow, int sortorder);
    virtual ~ImageView();

  protected:
    // Commands
    virtual void Rotate(int angle) = 0;
    virtual void DisplayNext(bool reset, bool loadImage) = 0;
    virtual void DisplayPrev(bool reset, bool loadImage) = 0;
    virtual void Load(void) = 0;

    // Sets
    virtual void SetZoom(float zoom) = 0;

    // Effects
    virtual void RegisterEffects(void) = 0;
    virtual QString GetRandomEffect(void) const;
    virtual void RunEffect(const QString &effect) = 0;

    void UpdateLCD(const ThumbItem *item);
    QString GetDescriptionStatus(void) const;
    void GetScreenShot(QImage& image, const ThumbItem *item);

  protected:
    QSize                  m_screenSize;
    float                  m_wmult;
    float                  m_hmult;
    int                    m_pos;
    int                   *m_savedPos;
    ThumbList              m_itemList;
    int                    m_movieState;
    float                  m_zoom;

    bool                   m_info_show;
    bool                   m_info_show_short;

    // Common slideshow variables
    bool                   m_slideshow_running;
    int                    m_slideshow_sequencing;
    int                    m_slideshow_sequencing_inc_order;
    SequenceBase          *m_slideshow_sequence;
    int                    m_slideshow_frame_delay;
    int                    m_slideshow_frame_delay_state;
    QTimer                *m_slideshow_timer;
    const char            *m_slideshow_mode;

    // Common effect state variables
    bool                   m_effect_running;
    int                    m_effect_current_frame;
    QString                m_effect_method;
    QMap<QString,QString>  m_effect_map;
    bool                   m_effect_random;
};

#endif /* IMAGEVIEW_H */
