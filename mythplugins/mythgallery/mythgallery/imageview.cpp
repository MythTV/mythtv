// -*- Mode: c++ -*-
/* ============================================================
 * File  : imageview.cpp
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

// Qt headers
#include <qptrlist.h>

// MythTV plugin headers
#include <mythtv/mythcontext.h>
#include <mythtv/lcddevice.h>

// MythGallery headers
#include "imageview.h"
#include "galleryutil.h"

ImageView::ImageView(const ThumbList &itemList,
                     int pos, int slideShow, int sortorder)
    : m_screenSize(640,480),
      m_wmult(1.0f),
      m_hmult(1.0f),
      m_pos(pos),
      m_itemList(itemList),
      m_movieState(0),
      m_zoom(1.0f),

      // Info variables
      m_info_show(false),

      // Common slideshow variables
      m_slideshow_running(false),
      m_slideshow_sequencing(slideShow),
      m_slideshow_sequencing_inc_order(sortorder),
      m_slideshow_sequence(NULL),
      m_slideshow_frame_delay(2),
      m_slideshow_frame_delay_state(m_slideshow_frame_delay * 1000),
      m_slideshow_timer(NULL),

      // Common effect state variables
      m_effect_running(false),
      m_effect_current_frame(0),
      m_effect_method(QString::null),
      m_effect_random(false)
{

    int xbase, ybase, screenwidth, screenheight;
    gContext->GetScreenSettings(xbase, screenwidth,  m_wmult,
                                ybase, screenheight, m_hmult);
    m_screenSize = QSize(screenwidth, screenheight);

    // --------------------------------------------------------------------

    bool recurse = gContext->GetNumSetting("GalleryRecursiveSlideshow", 0);

    m_itemList.setAutoDelete(false);

    // remove all dirs from m_itemList;
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

    // --------------------------------------------------------------------

    // since we remove dirs item position might have changed
    ThumbItem *origItem = NULL;
    if ((uint)m_pos < m_itemList.count())
        origItem = m_itemList.at(m_pos);

    if (origItem) 
        m_pos = m_itemList.find(origItem);

    m_pos = (!origItem || (m_pos == -1)) ? 0 : m_pos;

    // --------------------------------------------------------------------

    m_slideshow_frame_delay = gContext->GetNumSetting("SlideshowDelay", 0);
    m_slideshow_frame_delay = (!m_slideshow_frame_delay) ?
        2 : m_slideshow_frame_delay;
    m_slideshow_frame_delay_state = m_slideshow_frame_delay * 1000;

    // --------------------------------------------------------------------

    if (slideShow > 1)
    {
        m_slideshow_sequence = new SequenceShuffle(m_itemList.count());
        m_pos = 0;
    }
    else
    {
        m_slideshow_sequence = new SequenceInc(m_itemList.count());
    }

    m_pos = m_slideshow_sequence->index(m_pos);
}

ImageView::~ImageView()
{
    UpdateLCD(NULL);

    if (m_slideshow_sequence)
    {
        delete m_slideshow_sequence;
        m_slideshow_sequence = NULL;
    }
}

QString ImageView::GetRandomEffect(void) const
{
    QMap<QString,QString> tmpMap = m_effect_map;
    tmpMap.remove("none");
    QStringList t = tmpMap.keys();
    int i = (int) ( (float)(t.count()) * rand() / (RAND_MAX + 1.0f) );
    return tmpMap[t[i]];
}

void ImageView::UpdateLCD(const ThumbItem *item)
{
    LCD *lcd = LCD::Get();
    if (!lcd)
        return;

    if (!item)
    {
        lcd->switchToTime();
        return;
    }

    QString name = item->GetName();
    QString desc = QString::number(m_pos + 1) + " / " +
        QString::number(m_itemList.count());

    QPtrList<LCDTextItem> textItems;
    textItems.setAutoDelete(true);
    textItems.append(new LCDTextItem(
                         1, ALIGN_CENTERED, name, "Generic", true));
    textItems.append(new LCDTextItem(
                         2, ALIGN_CENTERED, desc, "Generic", false));

    lcd->switchToGeneric(&textItems);
}
