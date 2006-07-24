// -*- Mode: c++ -*-
/* ============================================================
 * File  : singleview.cpp
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

// ANSI C headers
#include <cmath>

// C++ headers
#include <iostream>
using namespace std;

// Qt headers
#include <qevent.h>
#include <qimage.h>
#include <qtimer.h>
#include <qpainter.h>

// MythTV plugin headers
#include <mythtv/mythcontext.h>
#include <mythtv/util.h>

// MythGallery headers
#include "singleview.h"
#include "constants.h"
#include "galleryutil.h"

#define LOC QString("QtView: ")
#define LOC_ERR QString("QtView, Error: ")

template<typename T> T sq(T val) { return val * val; }

SingleView::SingleView(
    ThumbList       itemList,  int pos,
    int             slideShow, int sortorder,
    MythMainWindow *parent,
    const char *name)
    : MythDialog(parent, name),
      ImageView(itemList, pos, slideShow, sortorder),

      // General
      m_pixmap(NULL),
      m_angle(0),
      m_source_loc(0,0),

      // Info variables
      m_info_pixmap(NULL),

      // Caption variables
      m_caption_show(false),
      m_caption_pixmap(NULL),
      m_caption_restore_pixmap(NULL),
      m_caption_timer(new QTimer(this)),

      // Common effect state variables
      m_effect_pixmap(NULL),
      m_effect_painter(NULL),
      m_effect_subtype(0),
      m_effect_bounds(0,0,0,0),
      m_effect_delta0(0,0),
      m_effect_delta1(0,0),
      m_effect_i(0),
      m_effect_j(0),
      m_effect_framerate(0),
      m_effect_delta2_x(0.0f),
      m_effect_delta2_y(0.0f),
      m_effect_alpha(0.0f),

      // Unshared effect state variables
      m_effect_spiral_tmp0(0,0),
      m_effect_spiral_tmp1(0,0),
      m_effect_multi_circle_out_delta_alpha(0.0f),
      m_effect_milti_circle_out_points(4),
      m_effect_circle_out_points(4)
{
    m_slideshow_timer = new QTimer(this);
    RegisterEffects();

    // --------------------------------------------------------------------

    QString transType = gContext->GetSetting("SlideshowTransition");
    if (!transType.isEmpty() && m_effect_map.contains(transType))
        m_effect_method = m_effect_map[transType];
    
    if (m_effect_method.isEmpty() || transType == "random")
    {
        m_effect_method = GetRandomEffect();
        m_effect_random = true;
    }

    // ---------------------------------------------------------------

    m_caption_show = gContext->GetNumSetting("GalleryOverlayCaption", 0);
    m_caption_show = min(m_slideshow_frame_delay, m_caption_show);

    if (m_caption_show)
    {
        m_caption_pixmap  = CreateBackground(QSize(screenwidth, 100));
        m_caption_restore_pixmap = new QPixmap(screenwidth, 100);
    }

    // --------------------------------------------------------------------

    setNoErase();
    QString bgtype = gContext->GetSetting("SlideshowBackground");
    if (bgtype != "theme" && !bgtype.isEmpty())
        setPalette(QPalette(QColor(bgtype)));

    // --------------------------------------------------------------------

    connect(m_slideshow_timer, SIGNAL(timeout()), SLOT(SlideTimeout()));
    connect(m_caption_timer,   SIGNAL(timeout()), SLOT(CaptionTimeout()));

    // --------------------------------------------------------------------

    LoadImage();

    // --------------------------------------------------------------------

    if (slideShow)
    {
        m_slideshow_running = true;
        m_slideshow_timer->start(m_slideshow_frame_delay_state, true);
        gContext->DisableScreensaver();
    }
}

SingleView::~SingleView()
{
    if (m_effect_painter)
    {
        if (m_effect_painter->isActive())
            m_effect_painter->end();

        delete m_effect_painter;
        m_effect_painter = NULL;
    }

    SetPixmap(NULL);

    if (m_effect_pixmap)
    {
        delete m_effect_pixmap;
        m_effect_pixmap = NULL;
    }
    
    if (m_info_pixmap)
    {
        delete m_info_pixmap;
        m_info_pixmap = NULL;
    }
}

void SingleView::paintEvent(QPaintEvent *)
{
    if (m_movieState > 0)
    {
        if (m_movieState == 1)
        {
            m_movieState = 2;
            ThumbItem *item = m_itemList.at(m_pos);
            QString path = QString("\"") + item->GetPath() + "\"";
            QString cmd = gContext->GetSetting("GalleryMoviePlayerCmd");
            cmd.replace("%s", path);
            myth_system(cmd);
            if (!m_slideshow_running)
            {
                reject();
            }
        }
    }
    else if (!m_effect_running)
    {
        QPixmap pix(screenwidth, screenheight);
        pix.fill(this, 0, 0);

        if (m_pixmap)
        {
            if (m_pixmap->width() <= screenwidth &&
                m_pixmap->height() <= screenheight)
            {
                bitBlt(&pix,
                       (screenwidth  - m_pixmap->width())  >> 1,
                       (screenheight - m_pixmap->height()) >> 1,
                       m_pixmap,0,0,-1,-1,Qt::CopyROP);
            }
            else
            {
                bitBlt(&pix, QPoint(0, 0),
                       m_pixmap, QRect(m_source_loc, pix.size()));
            }

            if (m_caption_show && !m_caption_timer->isActive())
            {
                ThumbItem *item = m_itemList.at(m_pos);
                if (!item->HasCaption())
                    item->SetCaption(GalleryUtil::GetCaption(item->GetPath()));

                if (item->HasCaption())
                {
                    // Store actual background to restore later
                    bitBlt(m_caption_restore_pixmap, QPoint(0, 0), &pix,
                           QRect(0, screenheight - 100, screenwidth, 100));

                    // Blit semi-transparent background into place
                    bitBlt(&pix, QPoint(0, screenheight - 100),
                           m_caption_pixmap, QRect(0, 0, screenwidth, 100));

                    // Draw caption
                    QPainter p(&pix, this);
                    p.drawText(0, screenheight - 100, screenwidth, 100,
                               Qt::AlignCenter, item->GetCaption());
                    p.end();

                    m_caption_timer->start(m_caption_show * 1000, true);
                }
            }

            if (m_zoom != 1.0f)
            {
                QPainter p(&pix, this);
                p.drawText(screenwidth / 10, screenheight / 10,
                           QString::number(m_zoom) + "x Zoom");
                p.end();
            }

            if (m_info_show)
            {
                if (!m_info_pixmap)
                {
                    m_info_pixmap = CreateBackground(QSize(
                        screenwidth  - 2 * screenwidth  / 10,
                        screenheight - 2 * screenheight / 10));
                }

                bitBlt(&pix, QPoint(screenwidth / 10, screenheight / 10),
                       m_info_pixmap, QRect(0,0,-1,-1), Qt::CopyROP);

                QPainter p(&pix, this);
                ThumbItem *item = m_itemList.at(m_pos);
                QString info = QString::null;
                if (item)
                    info = item->GetDescription(m_image.size(), m_angle);

                if (!info.isEmpty())
                {
                    p.drawText(screenwidth  / 10 + (int)(10 * wmult),
                               screenheight / 10 + (int)(10 * hmult),
                               m_info_pixmap->width()  - 2 * (int)(10 * wmult),
                               m_info_pixmap->height() - 2 * (int)(10 * hmult),
                               Qt::AlignLeft, info);
                }
                p.end();
            }

        }
        
        bitBlt(this, QPoint(0,0), &pix, QRect(0,0,-1,-1), Qt::CopyROP);
    }
    else if (!m_effect_method.isEmpty())
        RunEffect(m_effect_method);
}

void SingleView::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;

    bool wasRunning = m_slideshow_running;
    m_slideshow_timer->stop();
    m_caption_timer->stop();
    m_slideshow_running = false;
    gContext->RestoreScreensaver();
    m_effect_running = false;
    m_slideshow_frame_delay_state = m_slideshow_frame_delay * 1000;
    if (m_effect_painter && m_effect_painter->isActive())
        m_effect_painter->end();

    bool wasInfo = m_info_show;
    m_info_show = false;
    
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
            DisplayPrev(true, true);
        }
        else if (action == "RIGHT" || action == "DOWN")
        {
            DisplayNext(true, true);
        }
        else if (action == "ZOOMOUT")
        {
            m_source_loc = QPoint(0, 0);
            if (m_zoom > 0.5f)
                SetZoom(m_zoom * 0.5f);
            else
                handled = false;
        }
        else if (action == "ZOOMIN")
        {
            m_source_loc = QPoint(0, 0);
            if (m_zoom < 4.0f)
                SetZoom(m_zoom * 2.0f);
            else
                handled = false;
        }
        else if (action == "FULLSIZE")
        {
            m_source_loc = QPoint(0, 0);
            if (m_zoom != 1.0f)
                SetZoom(1.0f);
            else
                handled = false;
        }
        else if (action == "SCROLLLEFT")
        {
            if (m_zoom > 1.0f)
            {
                m_source_loc.setX(m_source_loc.x() - scrollX);
                m_source_loc.setX(
                    (m_source_loc.x() < 0) ? 0 : m_source_loc.x());
            }
            else
                handled = false;
        }
        else if (action == "SCROLLRIGHT")
        {
            if (m_zoom > 1.0f && m_pixmap)
            {
                m_source_loc.setX(m_source_loc.x() + scrollX);
                m_source_loc.setX(
                    min(m_source_loc.x(),
                        m_pixmap->width() - scrollX - screenwidth));
            }
            else
                handled = false;
        }
        else if (action == "SCROLLUP")
        {
            if (m_zoom > 1.0f && m_pixmap)
            {
                m_source_loc.setY(m_source_loc.y() + scrollY);
                m_source_loc.setY( 
                    min(m_source_loc.y(),
                        m_pixmap->height() - scrollY - screenheight));
            }
            else
                handled = false;
        }
        else if (action == "SCROLLDOWN")
        {
            if (m_zoom > 1.0f)
            {
                m_source_loc.setY(m_source_loc.y() - scrollY);
                m_source_loc.setY(
                    (m_source_loc.y() < 0) ? 0 : m_source_loc.y());
            }
            else
                handled = false;
        }
        else if (action == "RECENTER")
        {
            if (m_zoom > 1.0f && m_pixmap)
            {
                m_source_loc = QPoint(
                    (m_pixmap->width()  - screenwidth)  >> 1,
                    (m_pixmap->height() - screenheight) >> 1);
            }
            else
                handled = false;
        }
        else if (action == "UPLEFT")
        {
            if (m_zoom > 1.0f)
            {
                m_source_loc = QPoint(0,0);
            }
            else
                handled = false;
        }
        else if (action == "LOWRIGHT")
        {
            if (m_zoom > 1.0f && m_pixmap)
            {
                m_source_loc = QPoint(
                    m_pixmap->width()  - scrollX - screenwidth,
                    m_pixmap->height() - scrollY - screenheight);
            }
            else
                handled = false;
        }
        else if (action == "ROTRIGHT")
        {
            m_source_loc = QPoint(0, 0);
            Rotate(90);
        }
        else if (action == "ROTLEFT")
        {
            m_source_loc = QPoint(0, 0);
            Rotate(-90);
        }
        else if (action == "DELETE")
        {
            ThumbItem *item = m_itemList.at(m_pos);
            if (item && GalleryUtil::Delete(item->GetPath()))
            {
                item->SetPixmap(NULL);
                DisplayNext(true, true);
            }
        }
        else if (action == "PLAY" || action == "SLIDESHOW" ||
                 action == "RANDOMSHOW")
        {
            m_source_loc = QPoint(0, 0);
            m_zoom = 1.0f;
            m_angle = 0;
            m_slideshow_running = !wasRunning;
        }
        else if (action == "INFO")
        {
            m_info_show = !wasInfo;
        }
        else 
            handled = false;
    }

    if (m_slideshow_running)
    {
        m_slideshow_timer->start(m_slideshow_frame_delay_state, true);
        gContext->DisableScreensaver();
    }

    if (handled)
    {
        update();
    }
    else
    {
        MythDialog::keyPressEvent(e);
    }
}

void SingleView::DisplayNext(bool reset, bool loadImage)
{
    if (reset)
    {
        m_angle = 0;
        m_zoom = 1.0f;
        m_source_loc = QPoint(0, 0);
    }

    // Search for next item that hasn't been deleted.
    // Close viewer in none remain.
    ThumbItem *item;
    int oldpos = m_pos;
    while (true)
    {
        m_pos = m_slideshow_sequence->next();
        item = m_itemList.at(m_pos);
        if (item)
        {
            if (QFile::exists(item->GetPath()))
            {
                break;
            }
        }
        if (m_pos == oldpos)
        {
            // No valid items!!!
            reject();
        }
    }

    if (loadImage)
        LoadImage();
}

void SingleView::DisplayPrev(bool reset, bool loadImage)
{
    if (reset)
    {
        m_angle = 0;
        m_zoom = 1.0f;
        m_source_loc = QPoint(0, 0);
    }

    // Search for next item that hasn't been deleted.
    // Close viewer in none remain.
    int oldpos = m_pos;
    while (true)
    {
        m_pos = m_slideshow_sequence->prev();

        ThumbItem *item = m_itemList.at(m_pos);
        if (item && QFile::exists(item->GetPath()))
            break;

        if (m_pos == oldpos)
        {
            // No valid items!!!
            reject();
        }
    }

    if (loadImage)
        LoadImage();
}

void SingleView::LoadImage(void)
{
    m_movieState = 0;

    SetPixmap(NULL);
    
    ThumbItem *item = m_itemList.at(m_pos);
    if (!item)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "No item at "<<m_pos);
        return;
    }

    if (GalleryUtil::isMovie(item->GetPath()))
    {
        m_movieState = 1;
        return;
    }

    m_image.load(item->GetPath());
    if (m_image.isNull())
        return;

    m_angle = item->GetRotationAngle();
    if (m_angle != 0)
    {
        QWMatrix matrix;
        matrix.rotate(m_angle);
        m_image = m_image.xForm(matrix);
    }
        
    SetZoom(m_zoom);

    UpdateLCD(item);
}

void SingleView::Rotate(int angle)
{
    m_angle += angle;

    m_angle = (m_angle >= 360) ? m_angle - 360 : m_angle;
    m_angle = (m_angle < 0)    ? m_angle + 360 : m_angle;

    ThumbItem *item = m_itemList.at(m_pos);
    if (item)
        item->SetRotationAngle(m_angle);
    
    if (m_image.isNull())
        return;

    QWMatrix matrix;
    matrix.rotate(angle);
    m_image = m_image.xForm(matrix);

    SetZoom(m_zoom);
}

void SingleView::SetZoom(float zoom)
{
    m_zoom = zoom;

    if (m_image.isNull())
        return;

    QImage img = m_image.smoothScale((int) (screenwidth  * m_zoom),
                                     (int) (screenheight * m_zoom),
                                     QImage::ScaleMin);

    SetPixmap(new QPixmap(img));
}

void SingleView::SetPixmap(QPixmap *pixmap)
{
    if (m_pixmap)
    {
        delete m_pixmap;
        m_pixmap = NULL;
    }
    m_pixmap = pixmap;
}

QPixmap *SingleView::CreateBackground(const QSize &sz)
{
    QImage img(sz.width(), sz.height(), 32);
    img.setAlphaBuffer(true);

    for (int y = 0; y < img.height(); y++) 
    {
        for (int x = 0; x < img.width(); x++) 
        {
            uint *p = (uint *)img.scanLine(y) + x;
            *p = qRgba(0, 0, 0, 150);
        }
    }

    return new QPixmap(img);
}

void SingleView::RegisterEffects(void)
{
    m_effect_map.insert("none",             "EffectNone");
    m_effect_map.insert("chess board",      "EffectChessboard");
    m_effect_map.insert("melt down",        "EffectMeltdown");
    m_effect_map.insert("sweep",            "EffectSweep");
    m_effect_map.insert("noise",            "EffectNoise");
    m_effect_map.insert("growing",          "EffectGrowing");
    m_effect_map.insert("incoming edges",   "EffectIncomingEdges");
    m_effect_map.insert("horizontal lines", "EffectHorizLines");
    m_effect_map.insert("vertical lines",   "EffectVertLines");
    m_effect_map.insert("circle out",       "EffectCircleOut");
    m_effect_map.insert("multicircle out",  "EffectMultiCircleOut");
    m_effect_map.insert("spiral in",        "EffectSpiralIn");
    m_effect_map.insert("blobs",            "EffectBlobs");
}

void SingleView::RunEffect(const QString &effect)
{
    if (effect == "EffectChessboard")
        EffectChessboard();
    else if (effect == "EffectMeltdown")
        EffectMeltdown();
    else if (effect == "EffectSweep")
        EffectSweep();
    else if (effect == "EffectNoise")
        EffectNoise();
    else if (effect == "EffectGrowing")
        EffectGrowing();
    else if (effect == "EffectIncomingEdges")
        EffectIncomingEdges();
    else if (effect == "EffectHorizLines")
        EffectHorizLines();
    else if (effect == "EffectVertLines")
        EffectVertLines();
    else if (effect == "EffectCircleOut")
        EffectCircleOut();
    else if (effect == "EffectMultiCircleOut")
        EffectMultiCircleOut();
    else if (effect == "EffectSpiralIn")
        EffectSpiralIn();
    else if (effect == "EffectBlobs")
        EffectBlobs();
    else //if (effect == "EffectNone")
        EffectNone();
}

void SingleView::StartPainter(void)
{
    if (!m_effect_painter)
        m_effect_painter = new QPainter();

    if (m_effect_painter->isActive())
        m_effect_painter->end();

    QBrush brush;
    if (m_effect_pixmap)
        brush.setPixmap(*m_effect_pixmap);

    m_effect_painter->begin(this);
    m_effect_painter->setBrush(brush);
    m_effect_painter->setPen(Qt::NoPen);
}

void SingleView::CreateEffectPixmap(void)
{
    if (!m_effect_pixmap) 
        m_effect_pixmap = new QPixmap(screenwidth, screenheight);

    m_effect_pixmap->fill(this, 0, 0);

    if (m_pixmap)
    {
        QPoint src_loc((m_effect_pixmap->width()  - m_pixmap->width() ) >> 1,
                       (m_effect_pixmap->height() - m_pixmap->height()) >> 1);
        bitBlt(m_effect_pixmap, src_loc,
               m_pixmap, QRect(0, 0, -1, -1), Qt::CopyROP);
    }
}

void SingleView::EffectNone(void)
{
    m_effect_running = false;
    m_slideshow_frame_delay_state = -1;
    update();
    return;
}

void SingleView::EffectChessboard(void)
{
    if (m_effect_current_frame == 0)
    {
        m_effect_delta0 = QPoint(8, 8);       // tile size
        // m_effect_j = number of tiles
        m_effect_j = (width() + m_effect_delta0.x() - 1) / m_effect_delta0.x();
        m_effect_delta1 = QPoint(0, 0);       // growing offsets from screen border
        m_effect_framerate = 800 / m_effect_j;

        // x = shrinking x-offset from screen border
        // y = 0 or tile size for shrinking tiling effect
        m_effect_bounds = QRect(
            m_effect_j * m_effect_delta0.x(), (m_effect_j & 1) ? 0 : m_effect_delta0.y(),
            width(), height());
    }

    if (m_effect_delta1.x() >= m_effect_bounds.width())
    {
        m_effect_running = false;
        m_slideshow_frame_delay_state = -1;
        update();
        return;
    }

    m_effect_delta1 = QPoint(m_effect_delta1.x() + m_effect_delta0.x(),
                      (m_effect_delta1.y()) ? 0 : m_effect_delta0.y());
    QPoint t = QPoint(m_effect_bounds.x() - m_effect_delta0.x(),
                      (m_effect_bounds.y()) ? 0 : m_effect_delta0.y());
    m_effect_bounds = QRect(t, m_effect_bounds.size());

    for (int y = 0; y < m_effect_bounds.width(); y += (m_effect_delta0.y()<<1))
    {
        QPoint src0(m_effect_delta1.x(), y + m_effect_delta1.y());
        QRect  dst0(m_effect_delta1.x(), y + m_effect_delta1.y(),
                    m_effect_delta0.x(), m_effect_delta0.y());
        QPoint src1(m_effect_bounds.x(), y + m_effect_bounds.y());
        QRect  dst1(m_effect_bounds.x(), y + m_effect_bounds.y(),
                    m_effect_delta0.x(), m_effect_delta0.y());
        bitBlt(this, src0, m_effect_pixmap, dst0, Qt::CopyROP, true);
        bitBlt(this, src1, m_effect_pixmap, dst0, Qt::CopyROP, true);
    }

    m_slideshow_frame_delay_state = m_effect_framerate;

    m_effect_current_frame = 1;
}

void SingleView::EffectSweep(void)
{
    if (m_effect_current_frame == 0)
    {
        m_effect_subtype = rand() % 4;
        m_effect_delta0 = QPoint(
            (kSweepLeftToRight == m_effect_subtype) ? 16 : -16,
            (kSweepTopToBottom == m_effect_subtype) ? 16 : -16);
        m_effect_bounds = QRect(
            (kSweepLeftToRight == m_effect_subtype) ? 0 : width(),
            (kSweepTopToBottom == m_effect_subtype) ? 0 : height(),
            width(), height());
    }

    if (kSweepRightToLeft == m_effect_subtype ||
        kSweepLeftToRight == m_effect_subtype)
    {
        // horizontal sweep
        if ((kSweepRightToLeft == m_effect_subtype &&
             m_effect_bounds.x() < -64) ||
            (kSweepLeftToRight == m_effect_subtype &&
             m_effect_bounds.x() > m_effect_bounds.width() + 64))
        {
            m_slideshow_frame_delay_state = -1;
            m_effect_running = false;
            update();
            return;
        }

        int w, x, i;
        for (w = 2, i = 4, x = m_effect_bounds.x(); i > 0;
             i--, w <<= 1, x -= m_effect_delta0.x())
        {
            bitBlt(this, QPoint(x, 0),
                   m_effect_pixmap, QRect(x, 0, w, m_effect_bounds.height()),
                   Qt::CopyROP, true);
        }

        m_effect_bounds.moveLeft(m_effect_bounds.x() + m_effect_delta0.x());
    }
    else
    {
        // vertical sweep
        if ((kSweepBottomToTop == m_effect_subtype &&
             m_effect_bounds.y() < -64) ||
            (kSweepTopToBottom == m_effect_subtype &&
             m_effect_bounds.y() > m_effect_bounds.height() + 64))
        {
            m_slideshow_frame_delay_state = -1;
            m_effect_running = false;
            update();
            return;
        }

        int h, y, i;
        for (h = 2, i = 4, y = m_effect_bounds.y(); i > 0;
             i--, h <<= 1, y -= m_effect_delta0.y())
        {
            bitBlt(this, QPoint(0, y), m_effect_pixmap,
                   QRect(0, y, m_effect_bounds.width(), h),
                   Qt::CopyROP, true);
        }

        m_effect_bounds.moveTop(m_effect_bounds.y() + m_effect_delta0.y());
    }

    m_slideshow_frame_delay_state = 20;
    m_effect_current_frame = 1;
}

void SingleView::EffectGrowing(void)
{
    if (m_effect_current_frame == 0)
    {
        m_effect_bounds = QRect(width() >> 1, height() >> 1, width(), height());
        m_effect_i = 0;
        m_effect_delta2_x = m_effect_bounds.x() * 0.01f;
        m_effect_delta2_y = m_effect_bounds.y() * 0.01f;
    }

    m_effect_bounds.moveTopLeft(
        QPoint((m_effect_bounds.width()  >> 1) - (int)(m_effect_i * m_effect_delta2_x),
               (m_effect_bounds.height() >> 1) - (int)(m_effect_i * m_effect_delta2_y)));

    m_effect_i++;

    if (m_effect_bounds.x() < 0 || m_effect_bounds.y() < 0)
    {
        m_slideshow_frame_delay_state = -1;
        m_effect_running = false;
        update();
        return;
    }

    QSize dst_sz(m_effect_bounds.width()  - (m_effect_bounds.x() << 1),
                 m_effect_bounds.height() - (m_effect_bounds.y() << 1));

    bitBlt(this, m_effect_bounds.topLeft(),
           m_effect_pixmap, QRect(m_effect_bounds.topLeft(), dst_sz),
           Qt::CopyROP, true);

    m_slideshow_frame_delay_state = 20;
    m_effect_current_frame     = 1;
}

void SingleView::EffectHorizLines(void)
{
    static const int iyPos[] = { 0, 4, 2, 6, 1, 5, 3, 7, -1 };

    if (m_effect_current_frame == 0)
    {
        m_effect_bounds.setSize(size());
        m_effect_i = 0;
    }

    if (iyPos[m_effect_i] < 0)
    {
        m_slideshow_frame_delay_state = -1;
        m_effect_running = false;
        update();
        return;
    }

    for (int y = iyPos[m_effect_i]; y < m_effect_bounds.height(); y += 8)
    {
        bitBlt(this, QPoint(0, y),
               m_effect_pixmap, QRect(0, y, m_effect_bounds.width(), 1),
               Qt::CopyROP, true);
    }

    m_effect_i++;
    
    if (iyPos[m_effect_i] >= 0)
    {
        m_slideshow_frame_delay_state = 160;
        m_effect_current_frame     = 1;
    }
    else
    {
        m_slideshow_frame_delay_state = -1;
        m_effect_running = false;
        update();
        return;
    }        
}

void SingleView::EffectVertLines(void)
{
    static const int ixPos[] = { 0, 4, 2, 6, 1, 5, 3, 7, -1 };

    if (m_effect_current_frame == 0)
    {
        m_effect_bounds.setSize(size());
        m_effect_i = 0;
    }

    if (ixPos[m_effect_i] < 0)
    {
        m_slideshow_frame_delay_state = -1;
        m_effect_running = false;
        update();
        return;
    }

    for (int x = ixPos[m_effect_i]; x < m_effect_bounds.width(); x += 8)
    {
        bitBlt(this, QPoint(x, 0),
               m_effect_pixmap, QRect(x, 0, 1, m_effect_bounds.height()),
               Qt::CopyROP, true);
    }

    m_effect_i++;
    
    if (ixPos[m_effect_i] >= 0)
    {
        m_slideshow_frame_delay_state = 160;
        m_effect_current_frame     = 1;
    }
    else
    {
        m_slideshow_frame_delay_state = -1;
        m_effect_running = false;
        update();
        return;
    }        
}

void SingleView::EffectMeltdown(void)
{
    if (m_effect_current_frame == 0)
    {
        m_effect_bounds.setSize(size());
        m_effect_delta0 = QPoint(4, 16);
        m_effect_delta1 = QPoint(m_effect_bounds.width() / m_effect_delta0.x(), 0);
        m_effect_meltdown_y_disp.resize(m_effect_delta1.x());
    }

    int x = 0;
    bool done = true;
    for (int i = 0; i < m_effect_delta1.x(); i++, x += m_effect_delta0.x())
    {
        int y = m_effect_meltdown_y_disp[i];
        if (y >= m_effect_bounds.height())
            continue;

        done = false;
        if ((rand() & 0xF) < 6)
            continue;

        bitBlt(this, QPoint(x, y),
               m_effect_pixmap,
               QRect(x, y, m_effect_delta0.x(), m_effect_delta0.y()),
               Qt::CopyROP, true);

        m_effect_meltdown_y_disp[i] += m_effect_delta0.y();
    }

    if (done)
    {
        m_slideshow_frame_delay_state = -1;
        m_effect_running = false;
        update();
        return;
    }

    m_slideshow_frame_delay_state = 15;
    m_effect_current_frame     = 1;
}

void SingleView::EffectIncomingEdges(void)
{
    if (m_effect_current_frame == 0)
    {
        m_effect_bounds.setSize(size());
        m_effect_delta1 = QPoint(m_effect_bounds.width() >> 1, m_effect_bounds.height() >> 1);
        m_effect_delta2_x = m_effect_delta1.x() * 0.01f;
        m_effect_delta2_y = m_effect_delta1.y() * 0.01f;
        m_effect_i = 0;
        m_effect_subtype = rand() & 1;
    }

    m_effect_bounds.moveTopLeft(QPoint((int)(m_effect_delta2_x * m_effect_i),
                                (int)(m_effect_delta2_y * m_effect_i)));

    if (m_effect_bounds.x() > m_effect_delta1.x() || m_effect_bounds.y() > m_effect_delta1.y())
    {
        m_slideshow_frame_delay_state = -1;
        m_effect_running = false;
        update();
        return;
    }

    int x1 = m_effect_bounds.width()  - m_effect_bounds.x();
    int y1 = m_effect_bounds.height() - m_effect_bounds.y();
    m_effect_i++;

    if (kIncomingEdgesMoving == m_effect_subtype)
    {
        // moving image edges
        bitBlt(this,  0,  0, m_effect_pixmap,
               m_effect_delta1.x() - m_effect_bounds.x(),
               m_effect_delta1.y() - m_effect_bounds.y(),
               m_effect_bounds.x(), m_effect_bounds.y(),
               Qt::CopyROP, true);
        bitBlt(this, x1,  0, m_effect_pixmap,
               m_effect_delta1.x(), m_effect_delta1.y() - m_effect_bounds.y(),
               m_effect_bounds.x(), m_effect_bounds.y(),
               Qt::CopyROP, true);
        bitBlt(this,  0, y1, m_effect_pixmap,
               m_effect_delta1.x() - m_effect_bounds.x(), m_effect_delta1.y(),
               m_effect_bounds.x(), m_effect_bounds.y(),
               Qt::CopyROP, true);
        bitBlt(this, x1, y1, m_effect_pixmap,
               m_effect_delta1.x(), m_effect_delta1.y(),
               m_effect_bounds.x(), m_effect_bounds.y(),
               Qt::CopyROP, true);
    }
    else
    {
        // fixed image edges
        bitBlt(this,  0,  0,
               m_effect_pixmap, 0,   0, m_effect_bounds.x(), m_effect_bounds.y(),
               Qt::CopyROP, true);
        bitBlt(this, x1,  0,
               m_effect_pixmap, x1,  0, m_effect_bounds.x(), m_effect_bounds.y(),
               Qt::CopyROP, true);
        bitBlt(this,  0, y1,
               m_effect_pixmap,  0, y1, m_effect_bounds.x(), m_effect_bounds.y(),
               Qt::CopyROP, true);
        bitBlt(this, x1, y1,
               m_effect_pixmap, x1, y1, m_effect_bounds.x(), m_effect_bounds.y(),
               Qt::CopyROP, true);
    }

    m_slideshow_frame_delay_state = 20;
    m_effect_current_frame     = 1;
}

void SingleView::EffectMultiCircleOut(void)
{
    int x, y, i;
    double alpha;

    if (m_effect_current_frame == 0)
    {
        StartPainter();
        m_effect_bounds = QRect(width(), height() >> 1,
                         width(), height());

        m_effect_milti_circle_out_points.setPoint(
            0, m_effect_bounds.width() >> 1, m_effect_bounds.height() >> 1);
        m_effect_milti_circle_out_points.setPoint(
            3, m_effect_bounds.width() >> 1, m_effect_bounds.height() >> 1);

        m_effect_delta2_y = sqrtf(sq(m_effect_bounds.width())  * 1.0f + 
                           sq(m_effect_bounds.height()) * 0.5f);
        m_effect_i = (rand() & 0xf) + 2;
        m_effect_multi_circle_out_delta_alpha = M_PI * 2 / m_effect_i;
        m_effect_alpha = m_effect_multi_circle_out_delta_alpha;
        m_effect_framerate = 10 * m_effect_i;
        m_effect_delta2_x = M_PI / 32;  // divisor must be powers of 8
    }

    if (m_effect_alpha < 0)
    {
        m_effect_painter->end();

        m_slideshow_frame_delay_state = -1;
        m_effect_running = false;
        update();
        return;
    }

    for (alpha = m_effect_alpha, i = m_effect_i; i >= 0;
         i--, alpha += m_effect_multi_circle_out_delta_alpha)
    {
        x = (m_effect_bounds.width()  >> 1) + (int)(m_effect_delta2_y * cos(-alpha));
        y = (m_effect_bounds.height() >> 1) + (int)(m_effect_delta2_y * sin(-alpha));

        m_effect_bounds.moveTopLeft(
            QPoint((m_effect_bounds.width()  >> 1) +
                   (int)(m_effect_delta2_y * cos(-alpha + m_effect_delta2_x)),
                   (m_effect_bounds.height() >> 1) +
                   (int)(m_effect_delta2_y * sin(-alpha + m_effect_delta2_x))));

        m_effect_milti_circle_out_points.setPoint(1, x, y);
        m_effect_milti_circle_out_points.setPoint(2, m_effect_bounds.x(), m_effect_bounds.y());

        m_effect_painter->drawPolygon(m_effect_milti_circle_out_points);
    }

    m_effect_alpha -= m_effect_delta2_x;

    m_slideshow_frame_delay_state = m_effect_framerate;
    m_effect_current_frame     = 1;
}

void SingleView::EffectSpiralIn(void)
{
    if (m_effect_current_frame == 0)
    {
        StartPainter();
        m_effect_delta0   = QPoint(width() >> 3, 0);
        m_effect_delta1   = QPoint(width() >> 3, height() >> 3);
        m_effect_i = 0;
        m_effect_j = 16 * 16;
        m_effect_bounds   = QRect(QPoint(0,0), size());
        m_effect_spiral_tmp0 = QPoint(0, m_effect_delta1.y());
        m_effect_spiral_tmp1 = QPoint(m_effect_bounds.width()  - m_effect_delta1.x(),
                                      m_effect_bounds.height() - m_effect_delta1.y());
    }

    if (m_effect_i == 0 && m_effect_spiral_tmp0.x() >= m_effect_spiral_tmp1.x())
    {
        m_effect_painter->end();

        m_slideshow_frame_delay_state = -1;
        m_effect_running = false;
        update();
        return;
    }

    if (m_effect_i == 0 && m_effect_bounds.x() >= m_effect_spiral_tmp1.x())
    {
        // switch to: down on right side
        m_effect_i = 1;
        m_effect_delta0 = QPoint(0, m_effect_delta1.y());
        m_effect_spiral_tmp1.setX(m_effect_spiral_tmp1.x() - m_effect_delta1.x());
    }
    else if (m_effect_i == 1 && m_effect_bounds.y() >= m_effect_spiral_tmp1.y())
    {
        // switch to: right to left on bottom side
        m_effect_i = 2;
        m_effect_delta0 = QPoint(-m_effect_delta1.x(), 0);
        m_effect_spiral_tmp1.setY(m_effect_spiral_tmp1.y() - m_effect_delta1.y());
    }
    else if (m_effect_i == 2 && m_effect_bounds.x() <= m_effect_spiral_tmp0.x())
    {
        // switch to: up on left side
        m_effect_i = 3;
        m_effect_delta0 = QPoint(0, -m_effect_delta1.y());
        m_effect_spiral_tmp0.setX(m_effect_spiral_tmp0.x() + m_effect_delta1.x());
    }
    else if (m_effect_i == 3 && m_effect_bounds.y() <= m_effect_spiral_tmp0.y())
    {
        // switch to: left to right on top side
        m_effect_i = 0;
        m_effect_delta0 = QPoint(m_effect_delta1.x(), 0);
        m_effect_spiral_tmp0.setY(m_effect_spiral_tmp0.y() + m_effect_delta1.y());
    }

    bitBlt(this, m_effect_bounds.x(), m_effect_bounds.y(), m_effect_pixmap,
           m_effect_bounds.x(), m_effect_bounds.y(),
           m_effect_delta1.x(), m_effect_delta1.y(),
           Qt::CopyROP, true);

    m_effect_bounds.moveTopLeft(m_effect_bounds.topLeft() + m_effect_delta0);
    m_effect_j--;

    m_slideshow_frame_delay_state = 8;
    m_effect_current_frame     = 1;
}

void SingleView::EffectCircleOut(void)
{
    if (m_effect_current_frame == 0)
    {
        StartPainter();
        m_effect_bounds = QRect(QPoint(width(), height() >> 1), size());
        m_effect_alpha = 2 * M_PI;

        m_effect_circle_out_points.setPoint(
            0, m_effect_bounds.width() >> 1, m_effect_bounds.height() >> 1);
        m_effect_circle_out_points.setPoint(
            3, m_effect_bounds.width() >> 1, m_effect_bounds.height() >> 1);

        m_effect_delta2_x = M_PI / 16;  // divisor must be powers of 8
        m_effect_delta2_y = sqrtf(sq(m_effect_bounds.width())  * 1.0f +
                           sq(m_effect_bounds.height()) * 0.5f);
    }

    if (m_effect_alpha < 0)
    {
        m_effect_painter->end();
        
        m_slideshow_frame_delay_state = -1;
        m_effect_running = false;
        update();
        return;
    }

    QPoint tmp = m_effect_bounds.topLeft();

    m_effect_bounds.moveTopLeft(
        QPoint((m_effect_bounds.width()  >> 1) +
               (int)(m_effect_delta2_y * cos(m_effect_alpha)),
               (m_effect_bounds.height() >> 1) +
               (int)(m_effect_delta2_y * sin(m_effect_alpha))));

    m_effect_alpha -= m_effect_delta2_x;

    m_effect_circle_out_points.setPoint(1, tmp);
    m_effect_circle_out_points.setPoint(2, m_effect_bounds.topLeft());

    m_effect_painter->drawPolygon(m_effect_circle_out_points);

    m_slideshow_frame_delay_state = 20;
    m_effect_current_frame     = 1;
}

void SingleView::EffectBlobs(void)
{
    int r;

    if (m_effect_current_frame == 0)
    {
        StartPainter();
        m_effect_alpha = M_PI * 2;
        m_effect_bounds.setSize(size());
        m_effect_i = 150;
    }

    if (m_effect_i <= 0)
    {
        m_effect_painter->end();

        m_slideshow_frame_delay_state = -1;
        m_effect_running = false;
        update();
        return;
    }

    m_effect_bounds.setTopLeft(QPoint(rand() % m_effect_bounds.width(),
                               rand() % m_effect_bounds.height()));

    r = (rand() % 200) + 50;

    m_effect_painter->drawEllipse(m_effect_bounds.x() - r,
                           m_effect_bounds.y() - r, r, r);
    m_effect_i--;

    m_slideshow_frame_delay_state = 10;
    m_effect_current_frame     = 1;
}

void SingleView::EffectNoise(void)
{
    int x, y, i, w, h, fact, sz;

    fact = (rand() % 3) + 1;

    w = width() >> fact;
    h = height() >> fact;
    sz = 1 << fact;

    for (i = (w * h) << 1; i > 0; i--)
    {
        x = (rand() % w) << fact;
        y = (rand() % h) << fact;
        bitBlt(this, QPoint(x, y),
               m_effect_pixmap, QRect(x, y, sz, sz), Qt::CopyROP, true);
    }

    m_slideshow_frame_delay_state = -1;
    m_effect_running = false;
    update();
    return;
}

void SingleView::SlideTimeout(void)
{
    bool wasMovie = false, isMovie = false;
    if (m_effect_method.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "No transition method");
        return;
    }

    if (!m_effect_running)
    {
        if (m_slideshow_frame_delay_state == -1)
        {
            // wffect was running and is complete now
            // run timer while showing current image
            m_slideshow_frame_delay_state = m_slideshow_frame_delay * 1000;
            m_effect_current_frame = 0;
        }
        else
        {
            // timed out after showing current image
            // load next image and start effect
            if (m_effect_random)
                m_effect_method = GetRandomEffect();

            DisplayNext(false, false);

            wasMovie = m_movieState > 0;
            LoadImage();
            isMovie = m_movieState > 0;
            // If transitioning to/from a movie, don't do an effect,
            // and shorten timeout
            if (wasMovie || isMovie)
            {
                m_slideshow_frame_delay_state = 1;
            }
            else
            {
                CreateEffectPixmap();
                m_effect_running = true;
                m_slideshow_frame_delay_state = 10;
                m_effect_current_frame = 0;
            }
        }   
    }

    update();
    m_slideshow_timer->start(m_slideshow_frame_delay_state, true);

    // If transitioning to/from a movie, no effect is running so 
    // next timeout should trigger proper immage delay.
    if (wasMovie || isMovie)
    {
        m_slideshow_frame_delay_state = -1;
    }
}

void SingleView::CaptionTimeout(void)
{
    m_caption_timer->stop();
    bitBlt(this, QPoint(0, screenheight - 100),
           m_caption_restore_pixmap, QRect(0,0,-1,-1), Qt::CopyROP);
}
