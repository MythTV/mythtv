/* ============================================================
 * File  : uilistbtntype.cpp
 * Author: Renchi Raju <renchi@pooh.tam.uiuc.edu>
 * Date  : 2004-02-04
 * Description : 
 * 
 * Copyright 2004 by Renchi Raju

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

#include <qapplication.h>
#include <qpixmap.h>
#include <qpainter.h>
#include <qimage.h>
#include <qcolor.h>

#include "mythcontext.h"
#include "mythdialogs.h"

#include "osdlistbtntype.h"


OSDGenericTree::OSDGenericTree(OSDGenericTree *parent, const QString &name, 
                               const QString &action, int check, 
                               OSDTypeImage *image, QString group)
              : GenericTree(name)
{
    m_checkable = check;
    m_action = action;
    m_image = image;
    m_parentButton = NULL;

    if (group != "")
        m_group = group;
    else
        m_group = action;

    if (!action.isEmpty() && !action.isNull())
        setSelectable(true);

    if (parent)
        parent->addNode(this);
}

////////////////////////////////////////////////////////////////////////////

OSDListTreeType::OSDListTreeType(const QString &name, const QRect &area,
                                 const QRect &levelsize, int levelspacing,
                                 float wmult, float hmult)
               : OSDType(name)
{
    m_wmult = wmult;
    m_hmult = hmult;

    m_totalarea = area;
    m_levelsize = levelsize;
    m_levelspacing = levelspacing;

    if (gContext->GetNumSetting("UseArrowAccels", 1))
        m_arrowAccel = true;
    else
        m_arrowAccel = false;
    
    levels = 0;
    curlevel = -1;

    treetop = NULL;
    currentpos = NULL;

    currentlevel = NULL;

    listLevels.setAutoDelete(true);

    m_active = NULL;
    m_inactive = NULL;

    SetItemRegColor(Qt::black,QColor(80,80,80),100);
    SetItemSelColor(QColor(82,202,56),QColor(52,152,56),255);

    m_spacing = 0;
    m_margin = 0;
}

void OSDListTreeType::Reinit(float wchange, float hchange, float wmult,
                             float hmult)
{
    m_wmult = wmult;
    m_hmult = hmult;

    m_spacing = (int)(m_spacing * wchange);
    m_margin = (int)(m_margin * wchange);

    int width = (int)(m_totalarea.width() * wchange);
    int height = (int)(m_totalarea.height() * hchange);
    int x = (int)(m_totalarea.x() * wchange);
    int y = (int)(m_totalarea.y() * hchange);

    m_totalarea = QRect(x, y, width, height);

    width = (int)(m_levelsize.width() * wchange);
    height = (int)(m_levelsize.height() * hchange);
    x = (int)(m_levelsize.x() * wchange);
    y = (int)(m_levelsize.y() * hchange);

    m_levelsize = QRect(x, y, width, height);

    QPtrListIterator<OSDListBtnType> it(listLevels);
    OSDListBtnType *child;

    while ((child = it.current()) != 0)
    {
        child->Reinit(wchange, hchange, wmult, hmult);
        ++it;
    }
}

void OSDListTreeType::SetGroupCheckState(QString group, int newState)
{
    QPtrListIterator<OSDListBtnType> it(listLevels);
    OSDListBtnType *child;
    while ((child = it.current()) != 0)
    {
        child->SetGroupCheckState(group, newState);
        ++it;
    }
}

void OSDListTreeType::SetItemRegColor(const QColor& beg, const QColor& end,
                                      uint alpha)
{
    m_itemRegBeg   = beg;
    m_itemRegEnd   = end;
    m_itemRegAlpha = alpha;
}

void OSDListTreeType::SetItemSelColor(const QColor& beg, const QColor& end,
                                      uint alpha)
{
    m_itemSelBeg   = beg;
    m_itemSelEnd   = end;
    m_itemSelAlpha = alpha;
}

void OSDListTreeType::SetFontActive(TTFFont *font)
{
    m_active = font;
}

void OSDListTreeType::SetFontInactive(TTFFont *font)
{
    m_inactive = font;
}

void OSDListTreeType::SetSpacing(int spacing)
{
    m_spacing = spacing;
}

void OSDListTreeType::SetMargin(int margin)
{
    m_margin = margin;
}

void OSDListTreeType::SetAsTree(OSDGenericTree *toplevel)
{
    if (treetop)
    {
        listLevels.clear();
        currentlevel = NULL;
        treetop = NULL;
        currentpos = NULL;
        levels = 0;
        curlevel = -1;
    }

    levels = toplevel->calculateDepth(0) - 1;

    if (levels <= 0)
    {
        cerr << "Need at least one level\n";
        return;
    }

    currentpos = (OSDGenericTree *)toplevel->getChildAt(0);

    if (!currentpos)
    {
        cerr << "No top-level children?\n";
        return;
    }

    treetop = toplevel;

    // just for now, remove later
    if (levels > 2)
        levels = 3;

    for (int i = 0; i < levels; i++)
    {
        QString levelname = QString("level%1").arg(i + 1);

        QRect curlevelarea = m_levelsize;
        curlevelarea.moveBy(m_totalarea.x(), m_totalarea.y());

        curlevelarea.moveBy((m_levelsize.width() + m_levelspacing) * i, 0);

        OSDListBtnType *newlevel = new OSDListBtnType(levelname, curlevelarea,
                                                      m_wmult, m_hmult, true);

        newlevel->SetFontActive(m_active);
        newlevel->SetFontInactive(m_inactive);
        newlevel->SetItemRegColor(m_itemRegBeg, m_itemRegEnd, m_itemRegAlpha);
        newlevel->SetItemSelColor(m_itemSelBeg, m_itemSelEnd, m_itemSelAlpha);
        newlevel->SetSpacing(m_spacing);
        newlevel->SetMargin(m_margin);

        listLevels.append(newlevel);
    }

    currentlevel = GetLevel(0);

    if (!currentlevel)
    {
        cerr << "Something is seriously wrong (currentlevel = NULL)\n";
        return;
    }

    FillLevelFromTree(toplevel, currentlevel);

    currentlevel->SetVisible(true);
    currentlevel->SetActive(true);

    currentpos = (OSDGenericTree *)(currentlevel->GetItemFirst()->getData());
    curlevel = 0;

    emit itemEntered(this, currentpos);
}

OSDGenericTree *OSDListTreeType::GetCurrentPosition(void)
{
    return currentpos;
}

bool OSDListTreeType::HandleKeypress(QKeyEvent *e)
{
    if (!currentlevel)
        return false;

    bool handled = false;
    QStringList actions;
    if (gContext->GetMainWindow()->TranslateKeyPress("TV Playback", e, 
                                                     actions))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "UP")
            {
                currentlevel->MoveUp();
                SetCurrentPosition();
            }
            else if (action == "DOWN")
            {
                currentlevel->MoveDown();
                SetCurrentPosition();
            }
            else if (action == "LEFT")
            {
                if (curlevel > 0)
                {
                    currentlevel->Reset();
                    currentlevel->SetVisible(false);

                    curlevel--;

                    currentlevel = GetLevel(curlevel);
                    currentlevel->SetActive(true);
                    SetCurrentPosition();
                }
                else if (m_arrowAccel)
                {
                    m_visible = false;
                }
            }
            else if (action == "RIGHT")
            {
                // FIXME: create new levels if needed..
                if (curlevel + 1 < levels && currentpos->childCount() > 0)
                {
                    currentlevel->SetActive(false);

                    curlevel++;

                    currentlevel = GetLevel(curlevel);

                    FillLevelFromTree(currentpos, currentlevel);

                    currentlevel->SetVisible(true);
                    currentlevel->SetActive(true);
                    SetCurrentPosition();
                }
                else if (m_arrowAccel)
                {
                    SetGroupCheckState(currentpos->getGroup(),
                                       OSDListBtnTypeItem::NotChecked);
                    currentpos->getParentButton()->setChecked(
                                       OSDListBtnTypeItem::FullChecked);
                    emit itemSelected(this, currentpos);
                }
            }
            else if (action == "ESCAPE" || action == "MENU" ||
                     action == "CLEAROSD")
                m_visible = false;
            else if (action == "SELECT")
            {
                SetGroupCheckState(currentpos->getGroup(),
                                   OSDListBtnTypeItem::NotChecked);
                currentpos->getParentButton()->setChecked(
                                   OSDListBtnTypeItem::FullChecked);
                emit itemSelected(this, currentpos);
            }
            else
                handled = false;
        }
    }

    return handled;
}

void OSDListTreeType::Draw(OSDSurface *surface, int fade, int maxfade, 
                           int xoff, int yoff)
{
    QPtrListIterator<OSDListBtnType> it(listLevels);
    OSDListBtnType *child;

    while ((child = it.current()) != 0)
    {
        child->Draw(surface, fade, maxfade, xoff, yoff);
        ++it;
    }
}

void OSDListTreeType::FillLevelFromTree(OSDGenericTree *item, 
                                        OSDListBtnType *list)
{
    list->Reset();

    QPtrList<GenericTree> *itemlist = item->getAllChildren();

    QPtrListIterator<GenericTree> it(*itemlist);
    GenericTree *child;

    while ((child = it.current()) != 0)
    {
        OSDGenericTree *osdchild = (OSDGenericTree *)child;

        OSDListBtnTypeItem *newitem;
        newitem = new OSDListBtnTypeItem(list, child->getString(),
                                         osdchild->getImage(), 
                                         (osdchild->getCheckable() >= 0),
                                         (child->childCount() > 0));
        if (osdchild->getCheckable() == 1)
            newitem->setChecked(OSDListBtnTypeItem::FullChecked);
        newitem->setGroup(osdchild->getGroup());
        newitem->setData(osdchild);
        osdchild->setParentButton(newitem);

        ++it;
    }
}

OSDListBtnType *OSDListTreeType::GetLevel(int levelnum)
{
    if ((uint)levelnum > listLevels.count())
    {
        cerr << "OOB GetLevel call\n";
        return NULL;
    }

    return listLevels.at(levelnum);
}

void OSDListTreeType::SetCurrentPosition(void)
{
    if (!currentlevel)
        return;

    OSDListBtnTypeItem *lbt = currentlevel->GetItemCurrent();

    if (!lbt)
        return;

    currentpos = (OSDGenericTree *)(lbt->getData());
    emit itemEntered(this, currentpos);
}
 
//////////////////////////////////////////////////////////////////////////

OSDListBtnType::OSDListBtnType(const QString &name, const QRect &area,
                               float wmult, float hmult,
                               bool showScrollArrows)
              : OSDType(name)
{
    m_rect             = area;

    m_wmult            = wmult;
    m_hmult            = hmult;

    m_showScrollArrows = showScrollArrows;

    m_active           = false;
    m_showUpArrow      = false;
    m_showDnArrow      = false;

    m_itemList.setAutoDelete(false);
    m_topItem = 0;
    m_selItem = 0;

    m_initialized     = false;
    m_clearing        = false;
    m_itemSpacing     = 0;
    m_itemMargin      = 0;
    m_itemHeight      = 0;
    m_itemsVisible    = 0;
    m_fontActive      = 0;
    m_fontInactive    = 0;

    SetItemRegColor(Qt::black,QColor(80,80,80),100);
    SetItemSelColor(QColor(82,202,56),QColor(52,152,56),255);

    m_visible = false;
}

OSDListBtnType::~OSDListBtnType()
{    
    Reset();
}

void OSDListBtnType::Reinit(float wchange, float hchange, float wmult,
                            float hmult)
{
    m_wmult = wmult;
    m_hmult = hmult;

    m_itemHeight = (int)(m_itemHeight * hchange);
    m_itemSpacing = (int)(m_itemSpacing * wchange);
    m_itemMargin = (int)(m_itemMargin * wchange);

    int width = (int)(m_rect.width() * wchange);
    int height = (int)(m_rect.height() * hchange);
    int x = (int)(m_rect.x() * wchange);
    int y = (int)(m_rect.y() * hchange);

    m_rect = QRect(x, y, width, height);

    Init();

    OSDListBtnTypeItem* item = 0;
    for (item = m_itemList.first(); item; item = m_itemList.next()) {
        item->Reinit(wchange, hchange, wmult, hmult);
    }

}

void OSDListBtnType::SetGroupCheckState(QString group, int newState)
{
    OSDListBtnTypeItem* item = 0;
    for (item = m_itemList.first(); item; item = m_itemList.next()) {
        if (item->getGroup() == group)
            item->setChecked((OSDListBtnTypeItem::CheckState)newState);
    }
}

void OSDListBtnType::SetItemRegColor(const QColor& beg, const QColor& end, 
                                     uint alpha)
{
    m_itemRegBeg   = beg;
    m_itemRegEnd   = end;
    m_itemRegAlpha = alpha;
}

void OSDListBtnType::SetItemSelColor(const QColor& beg, const QColor& end,
                                     uint alpha)
{
    m_itemSelBeg   = beg;
    m_itemSelEnd   = end;
    m_itemSelAlpha = alpha;
}

void OSDListBtnType::SetFontActive(TTFFont *font)
{
    m_fontActive   = font;
}

void OSDListBtnType::SetFontInactive(TTFFont *font)
{
    m_fontInactive = font;
}

void OSDListBtnType::SetSpacing(int spacing)
{
    m_itemSpacing = spacing;    
}

void OSDListBtnType::SetMargin(int margin)
{
    m_itemMargin = margin;    
}

void OSDListBtnType::SetActive(bool active)
{
    m_active = active;
}

void OSDListBtnType::Reset()
{
    QMutexLocker lock(&m_update);

    m_clearing = true;

    OSDListBtnTypeItem* item = 0;
    for (item = m_itemList.first(); item; item = m_itemList.next()) {
        delete item;
    }

    m_clearing = false;
    m_itemList.clear();
    
    m_topItem     = 0;
    m_selItem     = 0;
    m_showUpArrow = false;
    m_showDnArrow = false;
}

void OSDListBtnType::InsertItem(OSDListBtnTypeItem *item)
{
    OSDListBtnTypeItem* lastItem = m_itemList.last();
    m_itemList.append(item);

    if (m_showScrollArrows && m_itemList.count() > m_itemsVisible)
        m_showDnArrow = true;
    else
        m_showDnArrow = false;

    if (!lastItem) 
    {
        m_topItem = item;
        m_selItem = item;
        emit itemSelected(item);
    }
}

void OSDListBtnType::RemoveItem(OSDListBtnTypeItem *item)
{
    if (m_clearing)
        return;
    
    if (m_itemList.find(item) == -1)
        return;

    m_topItem = m_itemList.first();
    m_selItem = m_itemList.first();

    m_itemList.remove(item);

    m_showUpArrow = false;
    
    if (m_showScrollArrows && m_itemList.count() > m_itemsVisible)
        m_showDnArrow = true;
    else
        m_showDnArrow = false;

    if (m_selItem) {
        emit itemSelected(m_selItem);
    }
}

void OSDListBtnType::SetItemCurrent(OSDListBtnTypeItem* item)
{
    bool locked = m_update.tryLock();

    if (m_itemList.find(item) == -1)
        return;

    m_topItem = item;
    m_selItem = item;

    if (m_showScrollArrows && m_itemList.count() > m_itemsVisible)
        m_showDnArrow = true;
    else
        m_showDnArrow = false;

    emit itemSelected(m_selItem);

    if (locked)
        m_update.unlock();
}

void OSDListBtnType::SetItemCurrent(int current)
{
    QMutexLocker lock(&m_update);

    OSDListBtnTypeItem* item = m_itemList.at(current);
    if (!item)
        item = m_itemList.first();

    SetItemCurrent(item);
}

OSDListBtnTypeItem* OSDListBtnType::GetItemCurrent()
{
    return m_selItem;
}

OSDListBtnTypeItem* OSDListBtnType::GetItemFirst()
{
    return m_itemList.first();    
}

OSDListBtnTypeItem* OSDListBtnType::GetItemNext(OSDListBtnTypeItem *item)
{
    QMutexLocker lock(&m_update);

    if (m_itemList.find(item) == -1)
        return 0;

    return m_itemList.next();
}

int OSDListBtnType::GetCount()
{
    return m_itemList.count();
}

OSDListBtnTypeItem* OSDListBtnType::GetItemAt(int pos)
{
    return m_itemList.at(pos);    
}

int OSDListBtnType::GetItemPos(OSDListBtnTypeItem* item)
{
    QMutexLocker lock(&m_update);

    return m_itemList.find(item);    
}

void OSDListBtnType::MoveUp()
{
    QMutexLocker lock(&m_update);

    if (m_itemList.find(m_selItem) == -1)
        return;

    OSDListBtnTypeItem *item = m_itemList.prev();
    if (!item)
    {
        item = m_itemList.last();
        if (!item)
            return;

        if (m_itemList.count() > m_itemsVisible)
            m_topItem = m_itemList.at(m_itemList.count() - m_itemsVisible);
        else
            m_topItem = m_itemList.first();
    }

    m_selItem = item;

    if (m_itemList.find(m_selItem) < m_itemList.find(m_topItem))
        m_topItem = m_selItem;

    if (m_topItem != m_itemList.first())
        m_showUpArrow = true;
    else
        m_showUpArrow = false;

    if (m_itemList.find(m_topItem) + m_itemsVisible < m_itemList.count())
        m_showDnArrow = true;
    else
        m_showDnArrow = false;

    emit itemSelected(m_selItem);
}

void OSDListBtnType::MoveDown()
{
    QMutexLocker lock(&m_update);

    if (m_itemList.find(m_selItem) == -1)
        return;

    OSDListBtnTypeItem *item = m_itemList.next();
    if (!item)
    {
        item = m_itemList.first();
        if (!item)
            return;

        m_topItem = item;
    }

    m_selItem = item;

    if (m_itemList.find(m_topItem) + m_itemsVisible <=
        (unsigned int)m_itemList.find(m_selItem)) 
    {
        m_topItem = m_itemList.at(m_itemList.find(m_topItem) + 1);
    }
    
    if (m_topItem != m_itemList.first())
        m_showUpArrow = true;
    else
        m_showUpArrow = false;

    if (m_itemList.find(m_topItem) + m_itemsVisible < m_itemList.count())
        m_showDnArrow = true;
    else
        m_showDnArrow = false;
    
    emit itemSelected(m_selItem);
}

void OSDListBtnType::Draw(OSDSurface *surface, int fade, int maxfade, int xoff,
                          int yoff)
{
    (void)xoff;
    (void)yoff;

    if (!m_visible)
        return;

    QMutexLocker lock(&m_update);

    if (!m_initialized)
        Init();

    TTFFont *font = m_active ? m_fontActive : m_fontInactive;
    
    int y = m_rect.y();
    m_itemList.find(m_topItem);
    OSDListBtnTypeItem *it = m_itemList.current();
    while (it && (y - m_rect.y()) <= (m_contentsRect.height() - m_itemHeight)) 
    {
        it->paint(surface, font, fade, maxfade, m_rect.x()+ xoff, y + yoff);

        y += m_itemHeight + m_itemSpacing;

        it = m_itemList.next();
    }

    if (m_showScrollArrows) 
    {
        if (m_showUpArrow)
            m_upArrowActPix.Draw(surface, fade, maxfade, 
                                 m_rect.x() + m_arrowsRect.x() + xoff,
                                 m_rect.y() + m_arrowsRect.y() + yoff);
        else
            m_upArrowRegPix.Draw(surface, fade, maxfade,
                                 m_rect.x() + m_arrowsRect.x() + xoff,
                                 m_rect.y() + m_arrowsRect.y() + yoff);
        if (m_showDnArrow)
            m_dnArrowActPix.Draw(surface, fade, maxfade,
                                 m_rect.x() + m_arrowsRect.x() +
                                 m_upArrowRegPix.width() + m_itemMargin + xoff,
                                 m_rect.y() + m_arrowsRect.y() + yoff);
        else
            m_dnArrowRegPix.Draw(surface, fade, maxfade,
                                 m_rect.x() + m_arrowsRect.x() +
                                 m_upArrowRegPix.width() + m_itemMargin + xoff,
                                 m_rect.y() + m_arrowsRect.y() + yoff);
    }
}

void OSDListBtnType::Init()
{
    qApp->lock();

    int sz1 = m_fontActive->Size() * 3 / 2;
    int sz2 = m_fontInactive->Size() * 3 / 2;
    m_itemHeight = QMAX(sz1, sz2) + (int)(2 * m_itemMargin);

    m_itemHeight = (m_itemHeight / 2) * 2;

    if (m_showScrollArrows) 
    {
        LoadPixmap(m_upArrowRegPix, "uparrow-reg");
        LoadPixmap(m_upArrowActPix, "uparrow-sel");
        LoadPixmap(m_dnArrowRegPix, "dnarrow-reg");
        LoadPixmap(m_dnArrowActPix, "dnarrow-sel");

        m_arrowsRect = QRect(0, m_rect.height() - m_upArrowActPix.height() - 1,
                             m_rect.width(), m_upArrowActPix.height());
    }
    else 
        m_arrowsRect = QRect(0, 0, 0, 0);
        
    m_contentsRect = QRect(0, 0, m_rect.width(), m_rect.height() -
                           m_arrowsRect.height() - 2 * m_itemMargin);

    m_itemsVisible = 0;
    int y = 0;
    while (y <= m_contentsRect.height() - m_itemHeight) 
    {
        y += m_itemHeight + m_itemSpacing;
        m_itemsVisible++;
    }

    LoadPixmap(m_checkNonePix, "check-empty");
    LoadPixmap(m_checkHalfPix, "check-half");
    LoadPixmap(m_checkFullPix, "check-full");
    LoadPixmap(m_arrowPix, "arrow");

    int itemWidth = (m_rect.width() / 2) * 2;

    QImage img(itemWidth, m_itemHeight, 32);
    img.setAlphaBuffer(true);

    for (int y = 0; y < img.height(); y++) 
    {
        for (int x = 0; x < img.width(); x++) 
        {
            uint *p = (uint *)img.scanLine(y) + x;
            *p = qRgba(0, 0, 0, m_itemRegAlpha);
        }
    }

    {
        float rstep = float(m_itemRegEnd.red() - m_itemRegBeg.red()) / 
                      float(m_itemHeight);
        float gstep = float(m_itemRegEnd.green() - m_itemRegBeg.green()) / 
                      float(m_itemHeight);
        float bstep = float(m_itemRegEnd.blue() - m_itemRegBeg.blue()) / 
                      float(m_itemHeight);

        QPixmap tmpRegPix = QPixmap(img);
        QPainter p(&tmpRegPix);

        float r = m_itemRegBeg.red();
        float g = m_itemRegBeg.green();
        float b = m_itemRegBeg.blue();
        for (int y = 0; y < img.height(); y++) 
        {
            QColor c((int)r, (int)g, (int)b);
            p.setPen(c);
            p.drawLine(0, y, img.width(), y);
            r += rstep;
            g += gstep;
            b += bstep;
        }
        p.setPen(Qt::black);
        p.drawLine(0, 0, 0, img.height() - 1);
        p.drawLine(0, 0, img.width() - 1, 0);
        p.drawLine(0, img.height() - 1, img.width() - 1, img.height() - 1);
        p.drawLine(img.width() - 1, 0, img.width() - 1, img.height() - 1);
        p.end();

        QImage tmpImg = tmpRegPix.convertToImage();
        m_itemRegPix.LoadFromQImage(tmpImg);
    }   

    {
        float rstep = float(m_itemSelEnd.red() - m_itemSelBeg.red()) /
                      float(m_itemHeight);
        float gstep = float(m_itemSelEnd.green() - m_itemSelBeg.green()) / 
                      float(m_itemHeight);
        float bstep = float(m_itemSelEnd.blue() - m_itemSelBeg.blue()) /
                      float(m_itemHeight);

        QPixmap tmpSelInactPix = QPixmap(img);
        QPainter p(&tmpSelInactPix);

        float r = m_itemSelBeg.red();
        float g = m_itemSelBeg.green();
        float b = m_itemSelBeg.blue();
        for (int y = 0; y < img.height(); y++) 
        {
            QColor c((int)r, (int)g, (int)b);
            p.setPen(c);
            p.drawLine(0, y, img.width(), y);
            r += rstep;
            g += gstep;
            b += bstep;
        }
        p.setPen(Qt::black);
        p.drawLine(0, 0, 0, img.height() - 1);
        p.drawLine(0, 0, img.width() - 1, 0);
        p.drawLine(0, img.height() - 1, img.width() - 1, img.height() - 1);
        p.drawLine(img.width() - 1, 0, img.width() - 1, img.height() - 1);
        p.end();

        QImage tmpImg = tmpSelInactPix.convertToImage();
        m_itemSelInactPix.LoadFromQImage(tmpImg);

        img.setAlphaBuffer(false);
        
        QPixmap tmpSelActPix = QPixmap(img);
        p.begin(&tmpSelActPix);

        r = m_itemSelBeg.red();
        g = m_itemSelBeg.green();
        b = m_itemSelBeg.blue();
        for (int y = 0; y < img.height(); y++) 
        {
            QColor c((int)r, (int)g, (int)b);
            p.setPen(c);
            p.drawLine(0, y, img.width(), y);
            r += rstep;
            g += gstep;
            b += bstep;
        }
        p.setPen(Qt::black);
        p.drawLine(0, 0, 0, img.height() - 1);
        p.drawLine(0, 0, img.width() - 1, 0);
        p.drawLine(0, img.height() - 1, img.width() - 1, img.height() - 1);
        p.drawLine(img.width() - 1, 0, img.width() - 1, img.height() - 1);
        p.end();

        tmpImg = tmpSelActPix.convertToImage();
        m_itemSelActPix.LoadFromQImage(tmpImg);
    }

    if (m_itemList.count() > m_itemsVisible && m_showScrollArrows)
        m_showDnArrow = true;
    else
        m_showDnArrow = false;

    m_initialized = true;

    qApp->unlock();
}

void OSDListBtnType::LoadPixmap(OSDTypeImage& pix, const QString& fileName)
{
    QString file = gContext->GetThemesParentDir() + "default/lb-" + fileName + ".png";
    pix.LoadImage(file, m_wmult, m_hmult);
}

/////////////////////////////////////////////////////////////////////////////
OSDListBtnTypeItem::OSDListBtnTypeItem(OSDListBtnType* lbtype, 
                                       const QString& text,
                                       OSDTypeImage *pixmap, bool checkable,
                                       bool showArrow, CheckState state)
{
    if (!lbtype) {
        VERBOSE(VB_IMPORTANT, "OSDListBtnTypeItem: trying to creating item without parent");
        exit(-16);
    }
    
    m_parent    = lbtype;
    m_text      = text;
    m_pixmap    = pixmap;
    m_checkable = checkable;
    m_state     = state;
    m_showArrow = showArrow;
    m_data      = 0;

    if (!m_parent->m_initialized)
        m_parent->Init();

    int  margin    = m_parent->m_itemMargin;
    int  width     = m_parent->m_rect.width();
    int  height    = m_parent->m_itemHeight;

    OSDTypeImage& checkPix = m_parent->m_checkNonePix;
    OSDTypeImage& arrowPix = m_parent->m_arrowPix;
    
    int cw = checkPix.ImageSize().width();
    int ch = checkPix.ImageSize().height();
    int aw = arrowPix.ImageSize().width();
    int ah = arrowPix.ImageSize().height();
    int pw = m_pixmap ? m_pixmap->ImageSize().width() : 0;
    int ph = m_pixmap ? m_pixmap->ImageSize().height() : 0;
    
    if (m_checkable) 
        m_checkRect = QRect(margin, (height - ch)/2, cw, ch);
    else
        m_checkRect = QRect(0,0,0,0);

    if (m_showArrow) 
        m_arrowRect = QRect(width - aw - margin, (height - ah)/2,
                            aw, ah);
    else
        m_arrowRect = QRect(0,0,0,0);

    if (m_pixmap) 
        m_pixmapRect = QRect(m_checkable ? (2*margin + m_checkRect.width()) :
                             margin, (height - ph)/2,
                             pw, ph);
    else
        m_pixmapRect = QRect(0,0,0,0);

    m_textRect = QRect(margin +
                       (m_checkable ? m_checkRect.width() + margin : 0) +
                       (m_pixmap    ? m_pixmapRect.width() + margin : 0),
                       0,
                       width - 2*margin -
                       (m_checkable ? m_checkRect.width() + margin : 0) -
                       (m_showArrow ? m_arrowRect.width() + margin : 0) -
                       (m_pixmap ? m_pixmapRect.width() + margin : 0),
                       height);

    m_parent->InsertItem(this);
}

OSDListBtnTypeItem::~OSDListBtnTypeItem()
{
    if (m_parent)
        m_parent->RemoveItem(this);
}

QString OSDListBtnTypeItem::text() const
{
    return m_text;
}

const OSDTypeImage* OSDListBtnTypeItem::pixmap() const
{
    return m_pixmap;
}

bool OSDListBtnTypeItem::checkable() const
{
    return m_checkable;
}

OSDListBtnTypeItem::CheckState OSDListBtnTypeItem::state() const
{
    return m_state;
}

OSDListBtnType* OSDListBtnTypeItem::parent() const
{
    return m_parent;
}

void OSDListBtnTypeItem::setChecked(CheckState state)
{
    if (!m_checkable)
        return;
    m_state = state;
}

void OSDListBtnTypeItem::setData(void *data)
{
    m_data = data;    
}

void* OSDListBtnTypeItem::getData()
{
    return m_data;
}

void OSDListBtnTypeItem::paint(OSDSurface *surface, TTFFont *font, 
                               int fade, int maxfade, int x, int y)
{
    if (this == m_parent->m_selItem)
    {
        if (m_parent->m_active)
            m_parent->m_itemSelActPix.Draw(surface, fade, maxfade, x, y);
        else
            m_parent->m_itemSelInactPix.Draw(surface, fade, maxfade, x, y);

        if (m_showArrow)
        {
            QRect ar(m_arrowRect);
            ar.moveBy(x, y);
            m_parent->m_arrowPix.Draw(surface, fade, maxfade, ar.x(), ar.y());
        }
    }
    else
    {
        if (m_parent->m_active)
            m_parent->m_itemRegPix.Draw(surface, fade, maxfade, x, y);
        else
            m_parent->m_itemRegPix.Draw(surface, fade, maxfade, x, y);
    }

    if (m_checkable)
    {
        QRect cr(m_checkRect);
        cr.moveBy(x, y);
        
        if (m_state == HalfChecked)
            m_parent->m_checkHalfPix.Draw(surface, fade, maxfade, cr.x(), cr.y());
        else if (m_state == FullChecked)
            m_parent->m_checkFullPix.Draw(surface, fade, maxfade, cr.x(), cr.y());
        else
            m_parent->m_checkNonePix.Draw(surface, fade, maxfade, cr.x(), cr.y());
    }

    if (m_pixmap)
    {
        QRect pr(m_pixmapRect);
        pr.moveBy(x, y);
        m_pixmap->Draw(surface, fade, maxfade, pr.x(), pr.y());
    }

    QRect tr(m_textRect);
    tr.moveBy(x, y);
    tr.moveBy(0, font->Size() / 4);
    font->DrawString(surface, tr.x(), tr.y(), m_text, tr.right(), tr.bottom());
}

void OSDListBtnTypeItem::Reinit(float wchange, float hchange, 
                                float wmult, float hmult)
{
    (void)wmult;
    (void)hmult;

    int width = (int)(m_checkRect.width() * wchange);
    int height = (int)(m_checkRect.height() * hchange);
    int x = (int)(m_checkRect.x() * wchange);
    int y = (int)(m_checkRect.y() * hchange);

    m_checkRect = QRect(x, y, width, height);

    width = (int)(m_pixmapRect.width() * wchange);
    height = (int)(m_pixmapRect.height() * hchange);
    x = (int)(m_pixmapRect.x() * wchange);
    y = (int)(m_pixmapRect.y() * hchange);

    m_pixmapRect = QRect(x, y, width, height);

    width = (int)(m_textRect.width() * wchange);
    height = (int)(m_textRect.height() * hchange);
    x = (int)(m_textRect.x() * wchange);
    y = (int)(m_textRect.y() * hchange);

    m_textRect = QRect(x, y, width, height);

    width = (int)(m_arrowRect.width() * wchange);
    height = (int)(m_arrowRect.height() * hchange);
    x = (int)(m_arrowRect.x() * wchange);
    y = (int)(m_arrowRect.y() * hchange);

    m_arrowRect = QRect(x, y, width, height);
}

