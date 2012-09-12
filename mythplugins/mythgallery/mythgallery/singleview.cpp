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
#include <QImage>
#include <QTimer>
#include <QPainter>
#include <QKeyEvent>
#include <QPixmap>
#include <QPaintEvent>

// MythTV plugin headers
#include <mythuihelper.h>
#include <mythcontext.h>
#include <mythdate.h>

// MythGallery headers
#include "singleview.h"
#include "galleryutil.h"

#define LOC QString("QtView: ")

template<typename T> T sq(T val) { return val * val; }

SingleView::SingleView(
    ThumbList       itemList,  int *pos,
    int             slideShow, int sortorder,
    MythMainWindow *parent,
    const char *name)
    : MythDialog(parent, name),
      ImageView(itemList, pos, slideShow, sortorder),

      // General
      m_pixmap(NULL),
      m_angle(0),
      m_source_loc(0,0),
      m_scaleMax(kScaleToFit),

      // Info variables
      m_info_pixmap(NULL),

      // Caption variables
      m_caption_show(0),
      m_caption_remove(false),
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
    m_scaleMax = (ScaleMax) gCoreContext->GetNumSetting("GalleryScaleMax", 0);

    m_slideshow_timer = new QTimer(this);
    RegisterEffects();

    // --------------------------------------------------------------------

    QString transType = gCoreContext->GetSetting("SlideshowTransition");
    if (!transType.isEmpty() && m_effect_map.contains(transType))
        m_effect_method = m_effect_map[transType];

    if (m_effect_method.isEmpty() || transType == "random")
    {
        m_effect_method = GetRandomEffect();
        m_effect_random = true;
    }

    // ---------------------------------------------------------------

    m_caption_show = gCoreContext->GetNumSetting("GalleryOverlayCaption", 0);
    if (m_caption_show)
    {
        m_caption_pixmap  = CreateBackground(QSize(screenwidth, 100));
        m_caption_restore_pixmap = new QPixmap(screenwidth, 100);
    }

    // --------------------------------------------------------------------

    setNoErase();
    QString bgtype = gCoreContext->GetSetting("SlideshowBackground");
    if (bgtype != "theme" && !bgtype.isEmpty())
        setPalette(QPalette(QColor(bgtype)));

    // --------------------------------------------------------------------

    connect(m_slideshow_timer, SIGNAL(timeout()), SLOT(SlideTimeout()));
    connect(m_caption_timer,   SIGNAL(timeout()), SLOT(CaptionTimeout()));

    // --------------------------------------------------------------------

    Load();

    // --------------------------------------------------------------------

    if (slideShow)
    {
        GetMythMainWindow()->PauseIdleTimer(true);
        m_slideshow_running = true;
        m_slideshow_timer->stop();
        m_slideshow_timer->setSingleShot(true);
        m_slideshow_timer->start(m_slideshow_frame_delay_state);
        GetMythUI()->DisableScreensaver();
    }
}

SingleView::~SingleView()
{
    if (m_slideshow_running)
    {
        GetMythMainWindow()->PauseIdleTimer(false);
        GetMythUI()->RestoreScreensaver();
    }

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

    // save the current m_scaleMax setting so we can restore it later
    gCoreContext->SaveSetting("GalleryScaleMax", m_scaleMax);
}

void SingleView::paintEvent(QPaintEvent *)
{
    if (1 == m_movieState)
    {
        m_movieState = 2;

        ThumbItem *item = m_itemList.at(m_pos);

        if (item)
            GalleryUtil::PlayVideo(item->GetPath());

        if (!m_slideshow_running && item)
        {
            QImage image;
            GetScreenShot(image, item);
            if (image.isNull())
                return;

            image = image.scaled(800, 600);

            // overlay "Press SELECT to play again" text
            QPainter p(&image);
            QRect rect = QRect(20, image.height() - 100,
                               image.width() - 40, 80);
            p.fillRect(rect, QBrush(QColor(0,0,0,100)));
            p.setFont(QFont("Arial", 25, QFont::Bold));
            p.setPen(QColor(255,255,255));
            p.drawText(rect, Qt::AlignCenter, tr("Press SELECT to play again"));
            p.end();

            m_image = image;
            SetZoom(1.0);
        }
    }

    if (!m_effect_running)
    {
        QPixmap pix(screenwidth, screenheight);
        pix.fill(this, 0, 0);

        if (m_pixmap)
        {
            if (m_pixmap->width() <= screenwidth &&
                m_pixmap->height() <= screenheight)
            {
                QPainter p(&pix);
                p.drawPixmap(QPoint((screenwidth  - m_pixmap->width())  >> 1,
                                    (screenheight - m_pixmap->height()) >> 1),
                             *m_pixmap, QRect(0,0,-1,-1));
            }
            else
            {
                QPainter p(&pix);
                p.drawPixmap(QPoint(0,0),
                             *m_pixmap, QRect(m_source_loc, pix.size()));
            }

            if (m_caption_remove)
            {
                m_caption_remove = false;
                QPainter p(this);
                p.drawPixmap(QPoint(0, screenheight - 100),
                             *m_caption_restore_pixmap, QRect(0,0,-1,-1));
                p.end();
            }
            else if (m_caption_show && !m_caption_timer->isActive())
            {
                ThumbItem *item = m_itemList.at(m_pos);
                if (!item->HasCaption())
                    item->InitCaption(true);

                if (item->HasCaption())
                {
                    // Store actual background to restore later
                    QPainter sb(m_caption_restore_pixmap);
                    sb.drawPixmap(QPoint(0, 0),
                                  pix,
                                  QRect(0, screenheight - 100,
                                        screenwidth, 100));
                    sb.end();

                    // Blit semi-transparent background into place
                    QPainter pbg(&pix);
                    pbg.drawPixmap(QPoint(0, screenheight - 100),
                                   *m_caption_pixmap,
                                   QRect(0, 0, screenwidth, 100));
                    pbg.end();

                    // Draw caption
                    QPainter p(&pix);
                    p.initFrom(this);
                    p.drawText(0, screenheight - 100, screenwidth, 100,
                               Qt::AlignCenter, item->GetCaption());
                    p.end();

                    m_caption_timer->stop();
                    m_caption_timer->setSingleShot(true);
                    m_caption_timer->start(m_caption_show * 1000);
                }
            }

            if (m_zoom != 1.0f)
            {
                QPainter p(&pix);
                p.initFrom(this);
                p.drawText(screenwidth / 10, screenheight / 10,
                           QString::number(m_zoom) + "x Zoom");
                p.end();
            }

            if (m_info_show || m_info_show_short)
            {
                if (!m_info_pixmap)
                {
                    m_info_pixmap = CreateBackground(QSize(
                        screenwidth  - 2 * screenwidth  / 10,
                        screenheight - 2 * screenheight / 10));
                }

                QPainter ip(&pix);
                ip.drawPixmap(QPoint(screenwidth / 10, screenheight / 10),
                              *m_info_pixmap, QRect(0,0,-1,-1));
                ip.end();

                QPainter p(&pix);
                p.initFrom(this);
                ThumbItem *item = m_itemList.at(m_pos);
                QString info = QString::null;
                if (item)
                {
                    info = item->GetDescription(GetDescriptionStatus(),
                                                m_image.size(), m_angle);
                }

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

        QPainter p(this);
        p.drawPixmap(QPoint(0,0), pix, QRect(0,0,-1,-1));
        p.end();
    }
    else if (!m_effect_method.isEmpty())
        RunEffect(m_effect_method);
}

void SingleView::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;

    if (m_slideshow_running)
        GetMythMainWindow()->PauseIdleTimer(false);
    bool wasRunning = m_slideshow_running;
    m_slideshow_timer->stop();
    m_caption_timer->stop();
    m_slideshow_running = false;
    GetMythUI()->RestoreScreensaver();
    m_effect_running = false;
    m_slideshow_frame_delay_state = m_slideshow_frame_delay * 1000;
    if (m_effect_painter && m_effect_painter->isActive())
        m_effect_painter->end();

    bool wasInfo = m_info_show;
    m_info_show = false;
    bool wasInfoShort = m_info_show_short;
    m_info_show_short = false;

    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("Gallery", e, actions);

    int scrollX = screenwidth / 10;
    int scrollY = screenheight / 10;

    for (unsigned int i = 0; i < (unsigned int) actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "LEFT" || action == "UP")
        {
            m_info_show = wasInfo;
            m_slideshow_running = wasRunning;
            DisplayPrev(true, true);
        }
        else if (action == "RIGHT" || action == "DOWN")
        {
            m_info_show = wasInfo;
            m_slideshow_running = wasRunning;
            DisplayNext(true, true);
        }
        else if (action == "ZOOMOUT")
        {
            if (m_zoom > 0.5f)
            {
                SetZoom(m_zoom - 0.5f);
                if (m_zoom > 1.0)
                {
                    m_source_loc.setY(m_source_loc.y() - (screenheight / 4));
                    m_source_loc.setX(m_source_loc.x() - (screenwidth / 4));
                    CheckPosition();
                }
                else
                    m_source_loc = QPoint(0, 0);
            }
        }
        else if (action == "ZOOMIN")
        {
            if (m_zoom < 4.0f)
            {
                SetZoom(m_zoom + 0.5f);
                if (m_zoom > 1.0)
                {
                    m_source_loc.setY(m_source_loc.y() + (screenheight / 4));
                    m_source_loc.setX(m_source_loc.x() + (screenwidth / 4));
                    CheckPosition();
                }
                else
                    m_source_loc = QPoint(0, 0);
            }
        }
        else if (action == "FULLSIZE")
        {
            m_source_loc = QPoint(0, 0);
            if (m_zoom != 1.0f)
                SetZoom(1.0f);
        }
        else if (action == "SCROLLLEFT")
        {
            if (m_zoom > 1.0f)
            {
                m_source_loc.setX(m_source_loc.x() - scrollX);
                m_source_loc.setX(
                    (m_source_loc.x() < 0) ? 0 : m_source_loc.x());
            }
        }
        else if (action == "SCROLLRIGHT")
        {
            if (m_zoom > 1.0f && m_pixmap)
            {
                m_source_loc.setX(m_source_loc.x() + scrollX);
                m_source_loc.setX(min(m_source_loc.x(),
                                  m_pixmap->width() - screenwidth));
            }
        }
        else if (action == "SCROLLDOWN")
        {
            if (m_zoom > 1.0f && m_pixmap)
            {
                m_source_loc.setY(m_source_loc.y() + scrollY);
                m_source_loc.setY(min(m_source_loc.y(),
                                  m_pixmap->height() - screenheight));
            }
        }
        else if (action == "SCROLLUP")
        {
            if (m_zoom > 1.0f)
            {
                m_source_loc.setY(m_source_loc.y() - scrollY);
                m_source_loc.setY(
                    (m_source_loc.y() < 0) ? 0 : m_source_loc.y());
            }
        }
        else if (action == "RECENTER")
        {
            if (m_zoom > 1.0f && m_pixmap)
            {
                m_source_loc = QPoint(
                    (m_pixmap->width()  - screenwidth)  >> 1,
                    (m_pixmap->height() - screenheight) >> 1);
            }
        }
        else if (action == "UPLEFT")
        {
            if (m_zoom > 1.0f)
            {
                m_source_loc = QPoint(0,0);
            }
        }
        else if (action == "LOWRIGHT")
        {
            if (m_zoom > 1.0f && m_pixmap)
            {
                m_source_loc = QPoint(
                    m_pixmap->width()  - scrollX - screenwidth,
                    m_pixmap->height() - scrollY - screenheight);
            }
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
            m_info_show = wasInfo;
            m_slideshow_running = wasRunning;
        }
        else if (action == "PLAY" || action == "SLIDESHOW" ||
                 action == "RANDOMSHOW")
        {
            m_source_loc = QPoint(0, 0);
            m_zoom = 1.0f;
            m_angle = 0;
            m_info_show = wasInfo;
            m_info_show_short = true;
            m_slideshow_running = !wasRunning;
        }
        else if (action == "INFO")
        {
            m_info_show = !wasInfo && !wasInfoShort;
            m_slideshow_running = wasRunning;
        }
        else if (action == "FULLSCREEN")
        {
            m_scaleMax = (ScaleMax) ((m_scaleMax + 1) % kScaleMaxCount);
            m_source_loc = QPoint(0, 0);
            SetZoom(1.0f);
        }
        else
        {
            handled = false;
        }
    }

    if (m_slideshow_running || m_info_show_short)
    {
        m_slideshow_timer->stop();
        m_slideshow_timer->setSingleShot(true);
        m_slideshow_timer->start(m_slideshow_frame_delay_state);
    }
    if (m_slideshow_running)
    {
        GetMythMainWindow()->PauseIdleTimer(true);
        GetMythUI()->DisableScreensaver();
    }

    update();

    if (!handled)
        MythDialog::keyPressEvent(e);
}

void  SingleView::CheckPosition(void)
{
    m_source_loc.setX((m_source_loc.x() < 0) ? 0 : m_source_loc.x());
    m_source_loc.setY((m_source_loc.y() < 0) ? 0 : m_source_loc.y());
    m_source_loc.setX(min(m_source_loc.x(), m_pixmap->width() - screenwidth));
    m_source_loc.setY(min(m_source_loc.y(), m_pixmap->height() - screenheight));
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
        Load();
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
        Load();
}

void SingleView::Load(void)
{
    m_movieState = 0;

    SetPixmap(NULL);

    ThumbItem *item = m_itemList.at(m_pos);
    if (!item)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("No item at %1").arg(m_pos));
        return;
    }

    if (GalleryUtil::IsMovie(item->GetPath()))
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
        QMatrix matrix;
        matrix.rotate(m_angle);
        m_image = m_image.transformed(matrix, Qt::SmoothTransformation);
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

    QMatrix matrix;
    matrix.rotate(angle);
    m_image = m_image.transformed(matrix, Qt::SmoothTransformation);

    SetZoom(m_zoom);
}

void SingleView::SetZoom(float zoom)
{
    m_zoom = zoom;

    if (m_image.isNull())
        return;

    QImage img = m_image;

    QSize dest = QSize(
        (int)(screenwidth * m_zoom), (int)(screenheight * m_zoom));

    QSize sz = GalleryUtil::ScaleToDest(m_image.size(), dest, m_scaleMax);
    if ((sz.width() > 0) && (sz.height() > 0))
        img = m_image.scaled(sz, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    SetPixmap(new QPixmap(QPixmap::fromImage(img)));
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
    QImage img(sz.width(), sz.height(), QImage::Format_ARGB32);

    for (int y = 0; y < img.height(); y++)
    {
        for (int x = 0; x < img.width(); x++)
        {
            uint *p = (uint *)img.scanLine(y) + x;
            *p = qRgba(0, 0, 0, 150);
        }
    }

    return new QPixmap(QPixmap::fromImage(img));
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
        brush.setTexture(*m_effect_pixmap);

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
        QPainter p(m_effect_pixmap);
        p.drawPixmap(src_loc, *m_pixmap, QRect(0, 0, -1, -1));
        p.end();
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

    QPainter painter(this);
    for (int y = 0; y < m_effect_bounds.width(); y += (m_effect_delta0.y()<<1))
    {
        QPoint src0(m_effect_delta1.x(), y + m_effect_delta1.y());
        QRect  dst0(m_effect_delta1.x(), y + m_effect_delta1.y(),
                    m_effect_delta0.x(), m_effect_delta0.y());
        QPoint src1(m_effect_bounds.x(), y + m_effect_bounds.y());
        QRect  dst1(m_effect_bounds.x(), y + m_effect_bounds.y(),
                    m_effect_delta0.x(), m_effect_delta0.y());
        painter.drawPixmap(src0, *m_effect_pixmap, dst0);
        painter.drawPixmap(src1, *m_effect_pixmap, dst0);
    }
    painter.end();

    m_slideshow_frame_delay_state = m_effect_framerate;

    m_effect_current_frame = 1;
}

void SingleView::EffectSweep(void)
{
    if (m_effect_current_frame == 0)
    {
        m_effect_subtype = random() % 4;
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
        QPainter p(this);
        for (w = 2, i = 4, x = m_effect_bounds.x(); i > 0;
             i--, w <<= 1, x -= m_effect_delta0.x())
        {
            p.drawPixmap(QPoint(x, 0), *m_effect_pixmap,
                         QRect(x, 0, w, m_effect_bounds.height()));
        }
        p.end();

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
        QPainter p(this);
        for (h = 2, i = 4, y = m_effect_bounds.y(); i > 0;
             i--, h <<= 1, y -= m_effect_delta0.y())
        {
            p.drawPixmap(QPoint(0, y), *m_effect_pixmap,
                         QRect(0, y, m_effect_bounds.width(), h));
        }
        p.end();

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

    QPainter p(this);
    QSize dst_sz(m_effect_bounds.width()  - (m_effect_bounds.x() << 1),
                 m_effect_bounds.height() - (m_effect_bounds.y() << 1));

    p.drawPixmap(m_effect_bounds.topLeft(),
                 *m_effect_pixmap, QRect(m_effect_bounds.topLeft(), dst_sz));
    p.end();

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

    QPainter p(this);
    for (int y = iyPos[m_effect_i]; y < m_effect_bounds.height(); y += 8)
    {
        p.drawPixmap(QPoint(0, y), *m_effect_pixmap,
                     QRect(0, y, m_effect_bounds.width(), 1));
    }
    p.end();

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

    QPainter p(this);
    for (int x = ixPos[m_effect_i]; x < m_effect_bounds.width(); x += 8)
    {
        p.drawPixmap(QPoint(x, 0), *m_effect_pixmap,
                     QRect(x, 0, 1, m_effect_bounds.height()));
    }
    p.end();

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
    QPainter p(this);
    for (int i = 0; i < m_effect_delta1.x(); i++, x += m_effect_delta0.x())
    {
        int y = m_effect_meltdown_y_disp[i];
        if (y >= m_effect_bounds.height())
            continue;

        done = false;
        if ((random() & 0xF) < 6)
            continue;

        p.drawPixmap(QPoint(x, y), *m_effect_pixmap,
                     QRect(x, y, m_effect_delta0.x(), m_effect_delta0.y()));

        m_effect_meltdown_y_disp[i] += m_effect_delta0.y();
    }
    p.end();

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
        m_effect_subtype = random() & 1;
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

    QPainter p(this);
    if (kIncomingEdgesMoving == m_effect_subtype)
    {
        // moving image edges
        p.drawPixmap(0,  0, *m_effect_pixmap,
               m_effect_delta1.x() - m_effect_bounds.x(),
               m_effect_delta1.y() - m_effect_bounds.y(),
               m_effect_bounds.x(), m_effect_bounds.y()
               );
        p.drawPixmap(x1,  0, *m_effect_pixmap,
               m_effect_delta1.x(), m_effect_delta1.y() - m_effect_bounds.y(),
               m_effect_bounds.x(), m_effect_bounds.y()
               );
        p.drawPixmap(0, y1, *m_effect_pixmap,
               m_effect_delta1.x() - m_effect_bounds.x(), m_effect_delta1.y(),
               m_effect_bounds.x(), m_effect_bounds.y()
               );
        p.drawPixmap(x1, y1, *m_effect_pixmap,
               m_effect_delta1.x(), m_effect_delta1.y(),
               m_effect_bounds.x(), m_effect_bounds.y()
               );
    }
    else
    {
        // fixed image edges
        p.drawPixmap( 0,  0, *m_effect_pixmap,  0,  0,
                     m_effect_bounds.x(), m_effect_bounds.y());
        p.drawPixmap(x1,  0, *m_effect_pixmap, x1,  0,
                     m_effect_bounds.x(), m_effect_bounds.y());
        p.drawPixmap( 0, y1, *m_effect_pixmap,  0, y1,
                     m_effect_bounds.x(), m_effect_bounds.y());
        p.drawPixmap(x1, y1, *m_effect_pixmap, x1, y1,
                     m_effect_bounds.x(), m_effect_bounds.y());
    }
    p.end();

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
        m_effect_i = (random() & 0xf) + 2;
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

    QPainter p(this);
    p.drawPixmap(m_effect_bounds.x(), m_effect_bounds.y(), *m_effect_pixmap,
           m_effect_bounds.x(), m_effect_bounds.y(),
           m_effect_delta1.x(), m_effect_delta1.y());
    p.end();

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

    m_effect_bounds.setTopLeft(QPoint(random() % m_effect_bounds.width(),
                               random() % m_effect_bounds.height()));

    r = (random() % 200) + 50;

    m_effect_painter->drawEllipse(m_effect_bounds.x() - r,
                           m_effect_bounds.y() - r, r, r);
    m_effect_i--;

    m_slideshow_frame_delay_state = 10;
    m_effect_current_frame     = 1;
}

void SingleView::EffectNoise(void)
{
    int x, y, i, w, h, fact, sz;

    fact = (random() % 3) + 1;

    w = width() >> fact;
    h = height() >> fact;
    sz = 1 << fact;

    QPainter p(this);
    for (i = (w * h) << 1; i > 0; i--)
    {
        x = (random() % w) << fact;
        y = (random() % h) << fact;
        p.drawPixmap(QPoint(x, y), *m_effect_pixmap, QRect(x, y, sz, sz));
    }
    p.end();

    m_slideshow_frame_delay_state = -1;
    m_effect_running = false;
    update();
    return;
}

void SingleView::SlideTimeout(void)
{
    bool wasMovie = false, isMovie = false;

    if (m_caption_timer->isActive())
    {
        m_caption_timer->stop();
    }

    if (m_effect_method.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "No transition method");
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
            if (m_slideshow_running)
            {
                if (m_effect_random)
                    m_effect_method = GetRandomEffect();

                DisplayNext(false, false);

                wasMovie = m_movieState > 0;
                Load();
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
            m_info_show_short = false;
        }
    }

    update();

    if (m_slideshow_running)
    {
        m_slideshow_timer->stop();
        m_slideshow_timer->setSingleShot(true);
        m_slideshow_timer->start(m_slideshow_frame_delay_state);

        // If transitioning to/from a movie, no effect is running so
        // next timeout should trigger proper immage delay.
        if (wasMovie || isMovie)
        {
            m_slideshow_frame_delay_state = -1;
        }
    }
}

void SingleView::CaptionTimeout(void)
{
    m_caption_timer->stop();
    m_caption_remove = true;
    update();
}
