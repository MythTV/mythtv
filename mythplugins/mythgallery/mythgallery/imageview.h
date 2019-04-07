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

#include <QObject>
// MythGallery headers
#include "iconview.h"
#include "sequence.h"

class MThread;
class QImage;
class QMutex;
class QRunnable;
class QTimer;
class QWaitCondition;
class ImageView;
class ThumbItem;

/** Handler for events related to LoadAlbumListener */
class LoadAlbumListener : public QObject {
    Q_OBJECT
    ImageView *m_parent;
public:
    explicit LoadAlbumListener(ImageView *parent)
        : m_parent(parent) {}
    virtual ~LoadAlbumListener() = default;
private slots:
    void FinishLoading() const;
};

class ImageView
{
     Q_DECLARE_TR_FUNCTIONS(ImageView);

  public:
    ImageView(const ThumbList &itemList,
              int *pos, int slideShow, int sortorder);
    ThumbItem *getCurrentItem() const;
    virtual ~ImageView();

  protected:
    static SequenceBase *ComposeSlideshowSequence(int slideshow_sequencing);

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
    virtual void AddItems(const ThumbList &itemList);
    ThumbItem *advanceItem();
    ThumbItem *retreatItem();

  private:
    friend class LoadAlbumListener;
    void FinishLoading();
    double GetSeasonalWeight(ThumbItem *item);

  protected:
    QSize                  m_screenSize                  {640,480};
    float                  m_wmult                       {1.0F};
    float                  m_hmult                       {1.0F};
    int                    m_pos;
    int                   *m_savedPos                    {nullptr};
    int                    m_movieState                  {0};
    float                  m_zoom                        {1.0F};

    bool                   m_info_show                   {false};
    bool                   m_info_show_short             {false};

    // Common slideshow variables
    bool                   m_slideshow_running           {false};
    const int              m_slideshow_sequencing;
    int                    m_slideshow_frame_delay       {2};
    int                    m_slideshow_frame_delay_state {2000};
    QTimer                *m_slideshow_timer             {nullptr};
    const char            *m_slideshow_mode              {nullptr};

    // Common effect state variables
    bool                   m_effect_running              {false};
    int                    m_effect_current_frame        {0};
    QString                m_effect_method;
    QMap<QString,QString>  m_effect_map;
    bool                   m_effect_random               {false};

private:
    Q_DISABLE_COPY(ImageView)
    class LoadAlbumRunnable;

    LoadAlbumRunnable     *m_loaderRunnable              {nullptr};
    LoadAlbumListener      m_listener;
    MThread               *m_loaderThread                {nullptr};
    QWaitCondition         m_imagesLoaded;
    // This lock must be held to access m_itemList and m_slideshow_sequence.
    mutable QMutex         m_itemListLock;
    ThumbList              m_itemList;
    SequenceBase          *m_slideshow_sequence          {nullptr};
    bool                   m_finishedLoading             {false};
};

#endif /* IMAGEVIEW_H */
