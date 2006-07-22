/* ============================================================
 * File  : singleview.cpp
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

#include <iostream>
#include <cmath>

#include <qevent.h>
#include <qimage.h>
#include <qfileinfo.h>
#include <qdir.h>
#include <qtimer.h>
#include <qpainter.h>

#include <mythtv/mythcontext.h>
#include <mythtv/util.h>
#include <mythtv/lcddevice.h>

#include "singleview.h"
#include "constants.h"
#include "galleryutil.h"

template<typename T> T sq(T val) { return val * val; }

SingleView::SingleView(
    ThumbList       itemList,  int pos,
    int             slideShow, int sortorder,
    MythMainWindow *parent,
    const char *name)
    : MythDialog(parent, name),
      m_pos(pos),
      m_itemList(itemList),

      m_movieState(0),
      m_pixmap(NULL),

      m_rotateAngle(0),
      m_zoom(1.0f),
      m_source_loc(0,0),

      m_info(false),
      m_infoBgPix(NULL),

      m_showcaption(false),
      m_captionBgPix(NULL),
      m_captionbackup(NULL),
      m_ctimer(new QTimer(this)),

      m_tmout(0),
      m_delay(0),
      m_effectRunning(false),
      m_running(false),
      m_slideShow(slideShow),
      m_sstimer(new QTimer(this)),
      m_effectPix(NULL),
      m_painter(NULL),

      m_effectMethod(NULL),
      m_effectRandom(false),
      m_sequence(NULL),

      // Common effect state variables
      m_effect_current_frame(0),
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
    // --------------------------------------------------------------------

    // remove all dirs from m_itemList;
    bool recurse = gContext->GetNumSetting("GalleryRecursiveSlideshow", 0);

    m_itemList.setAutoDelete(false);

    ThumbItem *item = m_itemList.first();
    while (item)
    {
        ThumbItem *next = m_itemList.next();
        if (item->IsDir())
        {
            if (recurse)
                GalleryUtil::LoadDirectory(m_itemList, item->GetPath(),
                                           sortorder, recurse, NULL, NULL);
            m_itemList.remove(item);
        }
        item = next;
    }

    // since we remove dirs item position might have changed
    item = itemList.at(m_pos);
    if (item) 
        m_pos = m_itemList.find(item);

    if (!item || (m_pos == -1))
        m_pos = 0;
    
    // --------------------------------------------------------------------

    RegisterEffects();
    
    QString transType = gContext->GetSetting("SlideshowTransition");
    if (!transType.isEmpty() && m_effectMap.contains(transType))
        m_effectMethod = m_effectMap[transType];
    
    if (!m_effectMethod || transType == "random")
    {
        m_effectMethod = GetRandomEffect();
        m_effectRandom = true;
    }

    // ---------------------------------------------------------------

    m_delay = gContext->GetNumSetting("SlideshowDelay", 0);
    m_delay = (!m_delay) ? 2 : m_delay;
    m_tmout = m_delay * 1000;

    // ----------------------------------------------------------------

    m_showcaption = gContext->GetNumSetting("GalleryOverlayCaption", 0);
    m_showcaption = min(m_delay, m_showcaption);

    if (m_showcaption)
    {
        m_captionBgPix  = CreateBackground(QSize(screenwidth, 100));
        m_captionbackup = new QPixmap(screenwidth, 100);
    }

    // --------------------------------------------------------------------

    setNoErase();
    QString bgtype = gContext->GetSetting("SlideshowBackground");
    if (bgtype != "theme" && !bgtype.isEmpty())
        setPalette(QPalette(QColor(bgtype)));

    // --------------------------------------------------------------------

    connect(m_sstimer, SIGNAL(timeout()), SLOT(slotSlideTimeOut()));
    connect(m_ctimer,  SIGNAL(timeout()), SLOT(slotCaptionTimeOut()));

    // --------------------------------------------------------------------

    if (slideShow > 1)
    {
        m_sequence = new SequenceShuffle(m_itemList.count());
        m_pos = 0;
    }
    else
    {
        m_sequence = new SequenceInc(m_itemList.count());
    }

    m_pos = m_sequence->index(m_pos);
    LoadImage();

    if (slideShow)
    {
        m_running = true;
        m_sstimer->start(m_tmout, true);
        gContext->DisableScreensaver();
    }
}

SingleView::~SingleView()
{
    if (m_painter)
    {
        if (m_painter->isActive())
            m_painter->end();

        delete m_painter;
        m_painter = NULL;
    }

    if (m_sequence)
    {
        delete m_sequence;
        m_sequence = NULL;
    }

    if (m_pixmap)
    {
        delete m_pixmap;
        m_pixmap = NULL;
    }

    if (m_effectPix)
    {
        delete m_effectPix;
        m_effectPix = NULL;
    }
    
    if (m_infoBgPix)
    {
        delete m_infoBgPix;
        m_infoBgPix = NULL;
    }

    if (class LCD *lcd = LCD::Get())
    {
        lcd->switchToTime();
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
            if (!m_running)
            {
                reject();
            }
        }
    }
    else if (!m_effectRunning)
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

            if (m_showcaption && !m_ctimer->isActive())
            {
                ThumbItem *item = m_itemList.at(m_pos);
                if (!item->HasCaption())
                    item->SetCaption(GalleryUtil::GetCaption(item->GetPath()));

                if (item->HasCaption())
                {
                    // Store actual background to restore later
                    bitBlt(m_captionbackup, QPoint(0, 0), &pix,
                           QRect(0, screenheight - 100, screenwidth, 100));

                    // Blit semi-transparent background into place
                    bitBlt(&pix, QPoint(0, screenheight - 100),
                           m_captionBgPix, QRect(0, 0, screenwidth, 100));

                    // Draw caption
                    QPainter p(&pix, this);
                    p.drawText(0, screenheight - 100, screenwidth, 100,
                               Qt::AlignCenter, item->GetCaption());
                    p.end();

                    m_ctimer->start(m_showcaption * 1000, true);
                }
            }

            if (m_zoom != 1.0f)
            {
                QPainter p(&pix, this);
                p.drawText(screenwidth / 10, screenheight / 10,
                           QString::number(m_zoom) + "x Zoom");
                p.end();
            }

            if (m_info)
            {

                if (!m_infoBgPix)
                {
                    m_infoBgPix = CreateBackground(QSize(
                        screenwidth  - 2 * screenwidth  / 10,
                        screenheight - 2 * screenheight / 10));
                }

                bitBlt(&pix, QPoint(screenwidth / 10, screenheight / 10),
                       m_infoBgPix, QRect(0,0,-1,-1), Qt::CopyROP);

                QPainter p(&pix, this);
                ThumbItem *item = m_itemList.at(m_pos);
                QString info = GetDescription(item, m_image.size());
                if (!info.isEmpty())
                {
                    p.drawText(screenwidth  / 10 + (int)(10 * wmult),
                               screenheight / 10 + (int)(10 * hmult),
                               m_infoBgPix->width()  - 2 * (int)(10 * wmult),
                               m_infoBgPix->height() - 2 * (int)(10 * hmult),
                               Qt::AlignLeft, info);
                }
                p.end();
            }

        }
        
        bitBlt(this, QPoint(0,0), &pix, QRect(0,0,-1,-1), Qt::CopyROP);
    }
    else
    {
        if (m_effectMethod)
            (this->*m_effectMethod)();
    }
}

QString SingleView::GetDescription(const ThumbItem *item, const QSize &sz)
{
    if (!item)
        return QString::null;

    QFileInfo fi(item->GetPath());

    QString info = item->GetName();
    info += "\n\n" + tr("Folder: ") + fi.dir().dirName();
    info += "\n" + tr("Created: ") + fi.created().toString();
    info += "\n" + tr("Modified: ") + fi.lastModified().toString();
    info += "\n" + QString(tr("Bytes") + ": %1").arg(fi.size());
    info += "\n" + QString(tr("Width") + ": %1 " + tr("pixels"))
        .arg(sz.width());
    info += "\n" + QString(tr("Height") + ": %1 " + tr("pixels"))
        .arg(sz.height());
    info += "\n" + QString(tr("Pixel Count") + ": %1 " + 
                           tr("megapixels"))
        .arg((float)sz.width() * sz.height() / 1000000, 0, 'f', 2);
    info += "\n" + QString(tr("Rotation Angle") + ": %1 " +
                           tr("degrees")).arg(m_rotateAngle);

    return info;
}

void SingleView::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;

    bool wasRunning = m_running;
    m_sstimer->stop();
    m_ctimer->stop();
    m_running = false;
    gContext->RestoreScreensaver();
    m_effectRunning = false;
    m_tmout = m_delay * 1000;
    if (m_painter && m_painter->isActive())
        m_painter->end();

    bool wasInfo = m_info;
    m_info = false;
    
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
            m_rotateAngle = 0;
            m_running = !wasRunning;
        }
        else if (action == "INFO")
        {
            m_info = !wasInfo;
        }
        else 
            handled = false;
    }

    if (m_running)
    {
        m_sstimer->start(m_tmout, true);
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
        m_rotateAngle = 0;
        m_zoom = 1.0f;
        m_source_loc = QPoint(0, 0);
    }

    // Search for next item that hasn't been deleted.
    // Close viewer in none remain.
    ThumbItem *item;
    int oldpos = m_pos;
    while (true)
    {
        m_pos = m_sequence->next();
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
        m_rotateAngle = 0;
        m_zoom = 1.0f;
        m_source_loc = QPoint(0, 0);
    }

    // Search for next item that hasn't been deleted.
    // Close viewer in none remain.
    ThumbItem *item;
    int oldpos = m_pos;
    while (true)
    {
        m_pos = m_sequence->prev();
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

void SingleView::LoadImage(void)
{
    m_movieState = 0;
    if (m_pixmap)
    {
        delete m_pixmap;
        m_pixmap = 0;
    }
    
    ThumbItem *item = m_itemList.at(m_pos);
    if (item)
    {
        if (GalleryUtil::isMovie(item->GetPath()))
        {
            m_movieState = 1;
        }
        else
        {
            m_image.load(item->GetPath());
        
            if (!m_image.isNull())
            {

                m_rotateAngle = item->GetRotationAngle();
          
                if (m_rotateAngle != 0)
                {
                    QWMatrix matrix;
                    matrix.rotate(m_rotateAngle);
                    m_image = m_image.xForm(matrix);
                }
        
                m_pixmap = new QPixmap(
                    m_image.smoothScale(screenwidth, screenheight,
                                        QImage::ScaleMin));
          
            }
        }

        if (class LCD *lcd = LCD::Get())
        {
            QPtrList<LCDTextItem> textItems;
            textItems.setAutoDelete(true);
            textItems.append(new LCDTextItem(1, ALIGN_CENTERED,
                                             item->GetName(),
                                             "Generic", true));
            textItems.append(new LCDTextItem(
                                 2, ALIGN_CENTERED,
                                 QString::number(m_pos + 1) + 
                                 " / " +
                                 QString::number(m_itemList.count()),
                                 "Generic", false));

            lcd->switchToGeneric(&textItems);
        }
    }
    else
    {
        std::cerr << "SingleView: Failed to load image "
                  << item->GetPath() << std::endl;
    }
}

void SingleView::Rotate(int angle)
{
    m_rotateAngle += angle;
    if (m_rotateAngle >= 360)
        m_rotateAngle -= 360;
    if (m_rotateAngle < 0)
        m_rotateAngle += 360;

    ThumbItem *item = m_itemList.at(m_pos);
    if (item)
        item->SetRotationAngle(m_rotateAngle);
    
    if (!m_image.isNull())
    {
        QWMatrix matrix;
        matrix.rotate(angle);
        m_image = m_image.xForm(matrix);
        if (m_pixmap)
        {
            delete m_pixmap;
            m_pixmap = 0;
        }
        m_pixmap = new QPixmap(m_image.smoothScale((int)(m_zoom*screenwidth),
                                                   (int)(m_zoom*screenheight),
                                                   QImage::ScaleMin));
    }
}

void SingleView::SetZoom(float zoom)
{
    m_zoom = zoom;

    if (m_image.isNull())
        return;

    if (m_pixmap)
    {
        delete m_pixmap;
        m_pixmap = 0;
    }

    m_pixmap = new QPixmap(m_image.smoothScale(
                               (int)(screenwidth  * m_zoom),
                               (int)(screenheight * m_zoom),
                               QImage::ScaleMin));
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
    m_effectMap.insert("none",             &SingleView::EffectNone);
    m_effectMap.insert("chess board",      &SingleView::EffectChessboard);
    m_effectMap.insert("melt down",        &SingleView::EffectMeltdown);
    m_effectMap.insert("sweep",            &SingleView::EffectSweep);
    m_effectMap.insert("noise",            &SingleView::EffectNoise);
    m_effectMap.insert("growing",          &SingleView::EffectGrowing);
    m_effectMap.insert("incoming edges",   &SingleView::EffectIncomingEdges);
    m_effectMap.insert("horizontal lines", &SingleView::EffectHorizLines);
    m_effectMap.insert("vertical lines",   &SingleView::EffectVertLines);
    m_effectMap.insert("circle out",       &SingleView::EffectCircleOut);
    m_effectMap.insert("multicircle out",  &SingleView::EffectMultiCircleOut);
    m_effectMap.insert("spiral in",        &SingleView::EffectSpiralIn);
    m_effectMap.insert("blobs",            &SingleView::EffectBlobs);
}

SingleView::EffectMethod SingleView::GetRandomEffect(void)
{
    QMap<QString,EffectMethod> tmpMap(m_effectMap);
    tmpMap.remove("none");
    
    QStringList t = tmpMap.keys();

    int count = t.count();
    int i = (int) ( (float)(count) * rand() / (RAND_MAX + 1.0f) );
    QString key = t[i];

    return tmpMap[key];
}

void SingleView::StartPainter(void)
{
    if (!m_painter)
        m_painter = new QPainter();

    if (m_painter->isActive())
        m_painter->end();

    QBrush brush;
    if (m_effectPix)
        brush.setPixmap(*m_effectPix);

    m_painter->begin(this);
    m_painter->setBrush(brush);
    m_painter->setPen(Qt::NoPen);
}

void SingleView::CreateEffectPixmap(void)
{
    if (!m_effectPix) 
        m_effectPix = new QPixmap(screenwidth, screenheight);

    m_effectPix->fill(this, 0, 0);

    if (m_pixmap)
    {
        QPoint src_loc((m_effectPix->width()  - m_pixmap->width() ) >> 1,
                       (m_effectPix->height() - m_pixmap->height()) >> 1);
        bitBlt(m_effectPix, src_loc,
               m_pixmap, QRect(0, 0, -1, -1), Qt::CopyROP);
    }
}

void SingleView::EffectNone(void)
{
    m_effectRunning = false;
    m_tmout = -1;
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
        m_effectRunning = false;
        m_tmout = -1;
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
        bitBlt(this, src0, m_effectPix, dst0, Qt::CopyROP, true);
        bitBlt(this, src1, m_effectPix, dst0, Qt::CopyROP, true);
    }

    m_tmout = m_effect_framerate;

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
            m_tmout = -1;
            m_effectRunning = false;
            update();
            return;
        }

        int w, x, i;
        for (w = 2, i = 4, x = m_effect_bounds.x(); i > 0;
             i--, w <<= 1, x -= m_effect_delta0.x())
        {
            bitBlt(this, QPoint(x, 0),
                   m_effectPix, QRect(x, 0, w, m_effect_bounds.height()),
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
            m_tmout = -1;
            m_effectRunning = false;
            update();
            return;
        }

        int h, y, i;
        for (h = 2, i = 4, y = m_effect_bounds.y(); i > 0;
             i--, h <<= 1, y -= m_effect_delta0.y())
        {
            bitBlt(this, QPoint(0, y), m_effectPix,
                   QRect(0, y, m_effect_bounds.width(), h),
                   Qt::CopyROP, true);
        }

        m_effect_bounds.moveTop(m_effect_bounds.y() + m_effect_delta0.y());
    }

    m_tmout = 20;
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
        m_tmout = -1;
        m_effectRunning = false;
        update();
        return;
    }

    QSize dst_sz(m_effect_bounds.width()  - (m_effect_bounds.x() << 1),
                 m_effect_bounds.height() - (m_effect_bounds.y() << 1));

    bitBlt(this, m_effect_bounds.topLeft(),
           m_effectPix, QRect(m_effect_bounds.topLeft(), dst_sz),
           Qt::CopyROP, true);

    m_tmout = 20;
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
        m_tmout = -1;
        m_effectRunning = false;
        update();
        return;
    }

    for (int y = iyPos[m_effect_i]; y < m_effect_bounds.height(); y += 8)
    {
        bitBlt(this, QPoint(0, y),
               m_effectPix, QRect(0, y, m_effect_bounds.width(), 1),
               Qt::CopyROP, true);
    }

    m_effect_i++;
    
    if (iyPos[m_effect_i] >= 0)
    {
        m_tmout = 160;
        m_effect_current_frame     = 1;
    }
    else
    {
        m_tmout = -1;
        m_effectRunning = false;
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
        m_tmout = -1;
        m_effectRunning = false;
        update();
        return;
    }

    for (int x = ixPos[m_effect_i]; x < m_effect_bounds.width(); x += 8)
    {
        bitBlt(this, QPoint(x, 0),
               m_effectPix, QRect(x, 0, 1, m_effect_bounds.height()),
               Qt::CopyROP, true);
    }

    m_effect_i++;
    
    if (ixPos[m_effect_i] >= 0)
    {
        m_tmout = 160;
        m_effect_current_frame     = 1;
    }
    else
    {
        m_tmout = -1;
        m_effectRunning = false;
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
               m_effectPix,
               QRect(x, y, m_effect_delta0.x(), m_effect_delta0.y()),
               Qt::CopyROP, true);

        m_effect_meltdown_y_disp[i] += m_effect_delta0.y();
    }

    if (done)
    {
        m_tmout = -1;
        m_effectRunning = false;
        update();
        return;
    }

    m_tmout = 15;
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
        m_tmout = -1;
        m_effectRunning = false;
        update();
        return;
    }

    int x1 = m_effect_bounds.width()  - m_effect_bounds.x();
    int y1 = m_effect_bounds.height() - m_effect_bounds.y();
    m_effect_i++;

    if (kIncomingEdgesMoving == m_effect_subtype)
    {
        // moving image edges
        bitBlt(this,  0,  0, m_effectPix,
               m_effect_delta1.x() - m_effect_bounds.x(),
               m_effect_delta1.y() - m_effect_bounds.y(),
               m_effect_bounds.x(), m_effect_bounds.y(),
               Qt::CopyROP, true);
        bitBlt(this, x1,  0, m_effectPix,
               m_effect_delta1.x(), m_effect_delta1.y() - m_effect_bounds.y(),
               m_effect_bounds.x(), m_effect_bounds.y(),
               Qt::CopyROP, true);
        bitBlt(this,  0, y1, m_effectPix,
               m_effect_delta1.x() - m_effect_bounds.x(), m_effect_delta1.y(),
               m_effect_bounds.x(), m_effect_bounds.y(),
               Qt::CopyROP, true);
        bitBlt(this, x1, y1, m_effectPix,
               m_effect_delta1.x(), m_effect_delta1.y(),
               m_effect_bounds.x(), m_effect_bounds.y(),
               Qt::CopyROP, true);
    }
    else
    {
        // fixed image edges
        bitBlt(this,  0,  0,
               m_effectPix, 0,   0, m_effect_bounds.x(), m_effect_bounds.y(),
               Qt::CopyROP, true);
        bitBlt(this, x1,  0,
               m_effectPix, x1,  0, m_effect_bounds.x(), m_effect_bounds.y(),
               Qt::CopyROP, true);
        bitBlt(this,  0, y1,
               m_effectPix,  0, y1, m_effect_bounds.x(), m_effect_bounds.y(),
               Qt::CopyROP, true);
        bitBlt(this, x1, y1,
               m_effectPix, x1, y1, m_effect_bounds.x(), m_effect_bounds.y(),
               Qt::CopyROP, true);
    }

    m_tmout = 20;
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
        m_painter->end();

        m_tmout = -1;
        m_effectRunning = false;
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

        m_painter->drawPolygon(m_effect_milti_circle_out_points);
    }

    m_effect_alpha -= m_effect_delta2_x;

    m_tmout = m_effect_framerate;
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
        m_painter->end();

        m_tmout = -1;
        m_effectRunning = false;
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

    bitBlt(this, m_effect_bounds.x(), m_effect_bounds.y(), m_effectPix,
           m_effect_bounds.x(), m_effect_bounds.y(),
           m_effect_delta1.x(), m_effect_delta1.y(),
           Qt::CopyROP, true);

    m_effect_bounds.moveTopLeft(m_effect_bounds.topLeft() + m_effect_delta0);
    m_effect_j--;

    m_tmout = 8;
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
        m_painter->end();
        
        m_tmout = -1;
        m_effectRunning = false;
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

    m_painter->drawPolygon(m_effect_circle_out_points);

    m_tmout = 20;
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
        m_painter->end();

        m_tmout = -1;
        m_effectRunning = false;
        update();
        return;
    }

    m_effect_bounds.setTopLeft(QPoint(rand() % m_effect_bounds.width(),
                               rand() % m_effect_bounds.height()));

    r = (rand() % 200) + 50;

    m_painter->drawEllipse(m_effect_bounds.x() - r,
                           m_effect_bounds.y() - r, r, r);
    m_effect_i--;

    m_tmout = 10;
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
               m_effectPix, QRect(x, y, sz, sz), Qt::CopyROP, true);
    }

    m_tmout = -1;
    m_effectRunning = false;
    update();
    return;
}


void SingleView::slotSlideTimeOut(void)
{
    bool wasMovie = false, isMovie = false;
    if (!m_effectMethod)
    {
        std::cerr << "SingleView: No transition method"
                  << std::endl;
        return;
    }

    if (!m_effectRunning)
    {
        if (m_tmout == -1)
        {
            // wffect was running and is complete now
            // run timer while showing current image
            m_tmout = m_delay * 1000;
            m_effect_current_frame = 0;
        }
        else
        {
            // timed out after showing current image
            // load next image and start effect
            if (m_effectRandom)
                m_effectMethod = GetRandomEffect();

            DisplayNext(false, false);

            wasMovie = m_movieState > 0;
            LoadImage();
            isMovie = m_movieState > 0;
            // If transitioning to/from a movie, don't do an effect,
            // and shorten timeout
            if (wasMovie || isMovie)
            {
                m_tmout = 1;
            }
            else
            {
                CreateEffectPixmap();
                m_effectRunning = true;
                m_tmout = 10;
                m_effect_current_frame = 0;
            }
        }   
    }

    update();
    m_sstimer->start(m_tmout, true);

    // If transitioning to/from a movie, no effect is running so 
    // next timeout should trigger proper immage delay.
    if (wasMovie || isMovie)
    {
        m_tmout = -1;
    }
}

void SingleView::slotCaptionTimeOut(void)
{
    m_ctimer->stop();
    bitBlt(this, QPoint(0, screenheight - 100),
           m_captionbackup, QRect(0,0,-1,-1), Qt::CopyROP);
}
