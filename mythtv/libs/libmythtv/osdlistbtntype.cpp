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

// ANSI C headers
#include <cmath>

// C++ headers
#include <algorithm>
using namespace std;

// Qt headers
#include <QCoreApplication>
#include <QPixmap>
#include <QPainter>
#include <QImage>
#include <QColor>
#include <QKeyEvent>

// MythTV headers
#include "mythcontext.h"
#include "mythdialogs.h"
#include "osdlistbtntype.h"
#include "mythdirs.h"
#include "mythverbose.h"

#define LOC QString("OSDListTreeType: ")
#define LOC_ERR QString("OSDListTreeType, Error: ")

QEvent::Type OSDListTreeItemEnteredEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type OSDListTreeItemSelectedEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();
QEvent::Type OSDListBtnItemSelectedEvent::kEventType =
    (QEvent::Type) QEvent::registerEventType();

OSDListTreeType::OSDListTreeType(
    const QString &name,      const QRect &area,
    const QRect   &levelsize, int          levelspacing,
    float          wmult,     float        hmult)
    : OSDType(name),
      treetop(NULL),                    currentpos(NULL),
      m_active(NULL),                   m_inactive(NULL),
      m_itemRegBeg(Qt::black),          m_itemRegEnd(QColor(80,80,80)),
      m_itemSelBeg(QColor(82,202,56)),  m_itemSelEnd(QColor(52,152,56)),
      m_itemRegAlpha(100),              m_itemSelAlpha(255),
      m_spacing(0),                     m_margin(0),
      m_levelspacing(levelspacing),
      m_totalarea(area),                m_levelsize(levelsize),
      m_unbiasedspacing(1.0f),          m_unbiasedmargin(1.0f),
      m_unbiasedarea(0,0,0,0),          m_unbiasedsize(0,0,0,0),
      m_wmult(wmult),                   m_hmult(hmult),
      m_depth(0),                        m_levelnum(-1),
      m_visible(true),
      m_arrowAccel(gCoreContext->GetNumSetting("UseArrowAccels", 1)),
      m_listener(NULL)
{
    m_wmult        = (wmult == 0.0f) ? 1.0f : wmult;
    m_hmult        = (hmult == 0.0f) ? 1.0f : hmult;
    m_unbiasedarea = unbias(area,      wmult, hmult);
    m_unbiasedsize = unbias(levelsize, wmult, hmult);
}

OSDListTreeType::~OSDListTreeType()
{
    OSDListBtnList::iterator it = listLevels.begin();
    for (; it != listLevels.end(); ++it)
        delete *it;
}

void OSDListTreeType::Reinit(float wmult, float hmult)
{
    m_wmult     = (wmult == 0.0f) ? 1.0f : wmult;
    m_hmult     = (hmult == 0.0f) ? 1.0f : hmult;
    m_spacing   = (uint) round(m_unbiasedspacing * wmult);
    m_margin    = (uint) round(m_unbiasedmargin  * wmult);
    m_totalarea = bias(m_unbiasedarea, wmult, hmult);
    m_levelsize = bias(m_unbiasedsize, wmult, hmult);

    if (!treetop || m_levelnum < 0)
        return;

    // Save item indices
    vector<uint> list;
    for (uint i = 0; i <= (uint)m_levelnum; i++)
        list.push_back(listLevels[i]->GetItemCurrentPos());

    // Delete old OSD items
    OSDListBtnList clone = listLevels;
    listLevels.clear();
    OSDListBtnList::iterator it = clone.begin();
    for (; it != clone.end(); ++it)
        delete *it;

    // Create new OSD items
    SetAsTree(treetop, &list);
}

void OSDListTreeType::SetGroupCheckState(QString group, int newState)
{
    OSDListBtnList::iterator it = listLevels.begin();
    for (; it != listLevels.end(); ++it)
        (*it)->SetGroupCheckState(group, newState);
}

void OSDListTreeType::SetAsTree(OSDGenericTree *toplevel,
                                vector<uint> *select_list)
{
    if (treetop)
    {
        listLevels.clear();
        treetop      = NULL;
        currentpos   = NULL;
        m_depth      = 0;
        m_levelnum   = -1;
    }

    m_depth = toplevel->calculateDepth(0) - 1;
    if (m_depth <= 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "SetAsTree: Need at least one level");
        return;
    }

    currentpos = (OSDGenericTree*) toplevel->getChildAt(0);
    if (!currentpos)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "SetAsTree: Need top-level children");
        return;
    }

    // Create OSD buttons for all levels
    for (uint i = 0; i < (uint)m_depth; i++)
    {
        QString levelname = QString("level%1").arg(i + 1);
        QRect curlevelarea = m_levelsize;
        curlevelarea.translate(m_totalarea.x(), m_totalarea.y());
        curlevelarea.translate((m_levelsize.width() + m_levelspacing) * i, 0);

        OSDListBtnType *newlevel = new OSDListBtnType(
            levelname, curlevelarea, m_wmult, m_hmult, true);

        newlevel->SetFontActive(m_active);
        newlevel->SetFontInactive(m_inactive);
        newlevel->SetItemRegColor(m_itemRegBeg, m_itemRegEnd, m_itemRegAlpha);
        newlevel->SetItemSelColor(m_itemSelBeg, m_itemSelEnd, m_itemSelAlpha);
        newlevel->SetSpacing(m_spacing);
        newlevel->SetMargin(m_margin);

        listLevels.push_back(newlevel);
    }

    // Set up needed levels and selects
    vector<uint> slist;
    slist.push_back(0);
    if (select_list)
        slist = *select_list;

    currentpos = treetop = toplevel;
    for (m_levelnum = 0; m_levelnum < (int)slist.size(); m_levelnum++)
    {
        FillLevelFromTree(currentpos, m_levelnum);
        GetLevel(m_levelnum)->SetActive(true);
        GetLevel(m_levelnum)->SetVisible(true);
        if (slist[m_levelnum])
            GetLevel(m_levelnum)->SetItemCurrent(slist[m_levelnum]);
        EnterItem(); // updates currentpos
    }
    m_levelnum--;
}

static bool has_action(QString action, const QStringList &actions)
{
    QStringList::const_iterator it;
    for (it = actions.begin(); it != actions.end(); ++it)
    {
        if (action == *it)
            return true;
    }
    return false;
}

bool OSDListTreeType::HandleKeypress(QKeyEvent *e)
{
    QStringList actions;
    bool handled = GetMythMainWindow()->TranslateKeyPress(
        "TV Playback", e, actions);

    if (handled)
        return true;

    if (((uint)m_levelnum >= listLevels.size()))
        return false;
    else if (has_action("UP", actions))
    {
        GetLevel(m_levelnum)->MoveUp();
        EnterItem();
    }
    else if (has_action("DOWN", actions))
    {
        GetLevel(m_levelnum)->MoveDown();
        EnterItem();
    }
    else if (has_action("PAGEUP", actions))
    {
        GetLevel(m_levelnum)->MovePageUp();
        EnterItem();
    }
    else if (has_action("PAGEDOWN", actions))
    {
        GetLevel(m_levelnum)->MovePageDown();
        EnterItem();
    }
    else if (has_action("LEFT", actions) && (m_levelnum > 0))
    {
        GetLevel(m_levelnum)->Reset();
        GetLevel(m_levelnum)->SetVisible(false);

        m_levelnum--;
        GetLevel(m_levelnum)->SetVisible(true);
        EnterItem();
    }
    else if ((has_action("LEFT", actions) && m_arrowAccel) ||
             has_action("ESCAPE",   actions) ||
             has_action("CLEAROSD", actions) ||
             has_action("MENU",     actions))
    {
        m_visible = false;
    }
    else if (has_action("RIGHT", actions) &&
             (m_levelnum + 1 < m_depth) &&
             (currentpos->childCount() > 0))
    {
        GetLevel(m_levelnum)->SetActive(false);
        if (m_levelnum - 1 >= 0)
            GetLevel(m_levelnum - 1)->SetVisible(false);
        m_levelnum++;

        FillLevelFromTree(currentpos, m_levelnum);
        GetLevel(m_levelnum)->SetVisible(true);
        EnterItem();
    }
    else if ((has_action("RIGHT", actions) && m_arrowAccel) ||
             has_action("SELECT", actions))
    {
        SelectItem();
    }
    else
    {
        return false;
    }

    return true;
}

void OSDListTreeType::Draw(OSDSurface *surface, int fade, int maxfade,
                           int xoff, int yoff)
{
    bool previousWasVisible = true;

    OSDListBtnList::iterator it = listLevels.begin();
    for (; it != listLevels.end(); ++it)
    {
        // Only display two levels, shift to the left if needed.
        int leftoff = (m_levelnum > 1 || !previousWasVisible) ?
                      (*it)->GetShiftLeftOffset() * max(m_levelnum - 1, 1) : 0;

        (*it)->Draw(surface, fade, maxfade, xoff + leftoff, yoff);

        previousWasVisible = (*it)->IsVisible();
    }
}

void OSDListTreeType::FillLevelFromTree(OSDGenericTree *item,
                                        uint level_num)
{
    OSDListBtnType *list = GetLevel(level_num);
    if (!list)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "FillLevelFromTree() "
                "called with no list, ignoring.");
        return;
    }
    list->Reset();

    vector<GenericTree*> itemlist = item->getAllChildren();
    vector<GenericTree*>::iterator it = itemlist.begin();
    for (; it != itemlist.end(); ++it)
    {
        OSDGenericTree *child = dynamic_cast<OSDGenericTree*>(*it);
        if (!child)
            continue;

        OSDTypeImage *im = child->getImage();
        QString label    = child->getString();
        QString group    = child->getGroup();
        bool    canCheck = child->getCheckable() >= 0;
        bool    hasCheck = child->getCheckable() == 1;
        bool    hasChild = child->childCount()   >  0;

        OSDListBtnTypeItem *newitem =
            new OSDListBtnTypeItem(list, label, im, canCheck, hasChild);

        if (hasCheck)
            newitem->setChecked(OSDListBtnTypeItem::FullChecked);
        newitem->setGroup(group);
        newitem->setData(child);

        child->setParentButton(newitem);
    }
}

OSDListBtnType *OSDListTreeType::GetLevel(uint levelnum)
{
    if (levelnum < listLevels.size())
        return listLevels[levelnum];

    VERBOSE(VB_IMPORTANT, LOC_ERR + "GetLevel("<<levelnum<<") "
            "listLevels.size() is only "<<listLevels.size());
    return NULL;
}

void OSDListTreeType::EnterItem(void)
{
    if ((uint)m_levelnum >= listLevels.size())
        return;

    listLevels[m_levelnum]->SetActive(true);
    OSDListBtnTypeItem *lbt = listLevels[m_levelnum]->GetItemCurrent();
    if (lbt)
    {
        currentpos = (OSDGenericTree*) (lbt->getData());
        SendItemEntered(this, currentpos);
    }
}

void OSDListTreeType::SelectItem(void)
{
    if (!currentpos)
        return;

    SetGroupCheckState(currentpos->getGroup(), OSDListBtnTypeItem::NotChecked);
    currentpos->getParentButton()->setChecked(OSDListBtnTypeItem::FullChecked);

    SendItemSelected(this, currentpos);
}

void OSDListTreeType::SendItemSelected(OSDListTreeType *parent, OSDGenericTree *item)
{
    if (!m_listener)
        return;

    OSDListTreeItemSelectedEvent *event =
        new OSDListTreeItemSelectedEvent(item->getString(), item->getAction());
    QCoreApplication::postEvent(m_listener, event);
}

void OSDListTreeType::SendItemEntered(OSDListTreeType *parent, OSDGenericTree *item)
{
    if (!m_listener)
        return;

    OSDListTreeItemEnteredEvent *event =
        new OSDListTreeItemEnteredEvent(item->getString(), item->getAction());
    QCoreApplication::postEvent(m_listener, event);
}

#undef LOC_ERR
#undef LOC

//////////////////////////////////////////////////////////////////////////

OSDListBtnType::OSDListBtnType(const QString &name, const QRect &area,
                               float wmult, float hmult,
                               bool showScrollArrows)
    : OSDType(name),
      m_order(0),                       m_rect(area),
      m_contentsRect(0,0,0,0),          m_arrowsRect(0,0,0,0),
      m_wmult(wmult),                   m_hmult(hmult),
      m_itemHeight(0),                  m_itemSpacing(0),
      m_itemMargin(0),                  m_itemsVisible(0),
      m_active(false),                  m_showScrollArrows(showScrollArrows),
      m_showUpArrow(false),             m_showDnArrow(false),
      m_initialized(false),             m_clearing(false),
      m_visible(false),
      m_itemRegBeg(Qt::black),          m_itemRegEnd(QColor(80,80,80)),
      m_itemSelBeg(QColor(82,202,56)),  m_itemSelEnd(QColor(52,152,56)),
      m_itemRegAlpha(100),              m_itemSelAlpha(255),
      m_fontActive(NULL),               m_fontInactive(NULL),
      m_topIndx(0),                     m_selIndx(0),
      m_update(QMutex::Recursive),      m_listener(NULL)
{
}

OSDListBtnType::~OSDListBtnType()
{
    Reset();
}

void OSDListBtnType::SetGroupCheckState(QString group, int newState)
{
    OSDListBtnItemList::iterator it;
    for (it = m_itemList.begin(); it != m_itemList.end(); ++it)
        if ((*it)->getGroup() == group)
            (*it)->setChecked((OSDListBtnTypeItem::CheckState) newState);
}

void OSDListBtnType::Reset(void)
{
    QMutexLocker lock(&m_update);

    m_clearing = true;
    OSDListBtnItemList::iterator it;
    OSDListBtnItemList clone = m_itemList;
    m_itemList.clear();
    for (it = clone.begin(); it != clone.end(); ++it)
        delete (*it);
    m_clearing = false;

    m_topIndx     = 0;
    m_selIndx     = 0;
    m_showUpArrow = false;
    m_showDnArrow = false;
}

void OSDListBtnType::InsertItem(OSDListBtnTypeItem *item)
{
    QMutexLocker lock(&m_update);
    m_itemList.push_back(item);
    m_showDnArrow = m_showScrollArrows && m_itemList.size() > m_itemsVisible;
    if (m_itemList.size() == 1)
        SendItemSelected(item);
}

int find(const OSDListBtnItemList &list, const OSDListBtnTypeItem *item)
{
    for (uint i = 0; i < list.size(); i++)
        if (list[i] == item)
            return i;
    return -1;
}

void OSDListBtnType::RemoveItem(OSDListBtnTypeItem *item)
{
    QMutexLocker lock(&m_update);
    if (m_clearing)
        return;

    int i = find(m_itemList, item);
    if (i < 0)
        return;

    m_itemList.erase(m_itemList.begin()+i);

    m_showUpArrow = false;
    m_showDnArrow = m_itemList.size() > m_itemsVisible;
    m_selIndx     = 0;
    m_topIndx     = 0;

    if (m_itemList.size())
        SendItemSelected(m_itemList[m_selIndx]);
}

void OSDListBtnType::SetItemCurrent(const OSDListBtnTypeItem* item)
{
    QMutexLocker lock(&m_update);
    int i = find(m_itemList, item);
    if (i >= 0)
        SetItemCurrent(i);
}

void OSDListBtnType::SetItemCurrent(uint current)
{
    QMutexLocker lock(&m_update);
    if (current >= m_itemList.size())
        return;

    m_selIndx     = current;
    m_topIndx     = max(m_selIndx - (int)m_itemsVisible, 0);
    m_showUpArrow = m_topIndx;
    m_showDnArrow = m_topIndx + m_itemsVisible < m_itemList.size();
    SendItemSelected(m_itemList[m_selIndx]);
}

int OSDListBtnType::GetItemCurrentPos(void) const
{
    QMutexLocker lock(&m_update);
    return (m_itemList.size()) ? m_selIndx : -1;
}

OSDListBtnTypeItem* OSDListBtnType::GetItemCurrent(void)
{
    QMutexLocker lock(&m_update);
    if (!m_itemList.size())
        return NULL;
    return m_itemList[m_selIndx];
}

OSDListBtnTypeItem* OSDListBtnType::GetItemFirst(void)
{
    QMutexLocker lock(&m_update);
    if (!m_itemList.size())
        return NULL;
    return m_itemList[0];
}

OSDListBtnTypeItem* OSDListBtnType::GetItemNext(const OSDListBtnTypeItem *item)
{
    QMutexLocker lock(&m_update);
    int i = find(m_itemList, item) + 1;
    if (i <= 0 || i >= (int)m_itemList.size())
        return NULL;
    return m_itemList[i];
}

int OSDListBtnType::GetCount(void) const
{
    QMutexLocker lock(&m_update);
    return m_itemList.size();
}

OSDListBtnTypeItem* OSDListBtnType::GetItemAt(int pos)
{
    QMutexLocker lock(&m_update);
    return m_itemList[pos];
}

int OSDListBtnType::GetItemPos(const OSDListBtnTypeItem *item) const
{
    QMutexLocker lock(&m_update);
    return find(m_itemList, item);
}

void OSDListBtnType::MoveUp(void)
{
    QMutexLocker lock(&m_update);
    if (!m_itemList.size())
        return;

    if (--m_selIndx < 0)
    {
        m_selIndx = m_itemList.size() - 1;
        m_topIndx = (m_itemList.size() > m_itemsVisible) ?
            m_itemList.size() - m_itemsVisible : 0;
    }

    m_topIndx     = (m_selIndx < m_topIndx) ? m_selIndx : m_topIndx;
    m_showUpArrow = m_topIndx;
    m_showDnArrow = m_topIndx + m_itemsVisible < m_itemList.size();

    SendItemSelected(m_itemList[m_selIndx]);
}

void OSDListBtnType::MoveDown(void)
{
    QMutexLocker lock(&m_update);
    if (!m_itemList.size())
        return;

    if (++m_selIndx >= (int)m_itemList.size())
        m_selIndx = m_topIndx = 0;

    bool scroll_down = m_topIndx + (int)m_itemsVisible <= m_selIndx;
    m_topIndx = (scroll_down) ? m_topIndx + 1 : m_topIndx;

    m_showUpArrow = m_topIndx;
    m_showDnArrow = m_topIndx + m_itemsVisible < m_itemList.size();

    SendItemSelected(m_itemList[m_selIndx]);
}

void OSDListBtnType::MovePageUp(void)
{
    QMutexLocker lock(&m_update);
    if (!m_itemList.size())
        return;

    if (m_itemsVisible > m_itemList.size())
        return;

    m_selIndx = m_topIndx = m_topIndx - m_itemsVisible;

    if (m_selIndx < 0)
    {
        int top = (int)(m_itemsVisible *
            ceil((float)m_itemList.size()/(float)m_itemsVisible));
        m_selIndx = m_topIndx = top - m_itemsVisible;
    }

    m_showUpArrow = m_topIndx;
    m_showDnArrow = m_topIndx + m_itemsVisible < m_itemList.size();

    SendItemSelected(m_itemList[m_selIndx]);
}

void OSDListBtnType::MovePageDown(void)
{
    QMutexLocker lock(&m_update);
    if (!m_itemList.size())
        return;

    if (m_itemsVisible > m_itemList.size())
        return;

    m_selIndx = m_topIndx+m_itemsVisible;

    if (m_selIndx >= (int)m_itemList.size())
        m_selIndx = m_topIndx = 0;

    bool scroll_down = m_topIndx + (int)m_itemsVisible <= m_selIndx;
    m_topIndx = (scroll_down) ? m_topIndx + m_itemsVisible : m_topIndx;

    m_showUpArrow = m_topIndx;
    m_showDnArrow = m_topIndx + m_itemsVisible < m_itemList.size();

    SendItemSelected(m_itemList[m_selIndx]);
}

void OSDListBtnType::Draw(OSDSurface *surface,
                          int fade, int maxfade,
                          int xoff, int yoff)
{
    QMutexLocker lock(&m_update);
    if (!m_visible)
        return;
    if (!m_initialized)
        Init();

    TTFFont *font = m_active ? m_fontActive : m_fontInactive;

    int y = m_rect.y();
    for (uint i = m_topIndx; i < m_itemList.size(); i++)
    {
        if (!((y - m_rect.y()) <= (m_contentsRect.height() - m_itemHeight)))
            break;
        m_itemList[i]->paint(surface, font, fade, maxfade,
                             m_rect.x() + xoff, y + yoff);
        y += m_itemHeight + m_itemSpacing;
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

void OSDListBtnType::Init(void)
{
    int sz1 = m_fontActive->Size() * 3 / 2;
    int sz2 = m_fontInactive->Size() * 3 / 2;
    m_itemHeight = max(sz1, sz2) + (int)(2 * m_itemMargin);
    m_itemHeight = m_itemHeight & ~0x1;

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

    uint itemWidth = (m_rect.width() + 1) & (~1);

    InitItem(m_itemRegPix,      itemWidth,    m_itemHeight,
             m_itemRegBeg,      m_itemRegEnd, m_itemRegAlpha);
    InitItem(m_itemSelInactPix, itemWidth,    m_itemHeight,
             m_itemSelBeg,      m_itemSelEnd, m_itemRegAlpha);
    InitItem(m_itemSelActPix,   itemWidth,    m_itemHeight,
             m_itemSelBeg,      m_itemSelEnd, 255);

    m_showDnArrow = m_itemList.size() > m_itemsVisible && m_showScrollArrows;
    m_initialized = true;
}

void OSDListBtnType::InitItem(
    OSDTypeImage &osdImg, uint width, uint height,
    QColor beg, QColor end, int alpha)
{
    float rstep = ((float) (end.red()   - beg.red()))   / m_itemHeight;
    float gstep = ((float) (end.green() - beg.green())) / m_itemHeight;
    float bstep = ((float) (end.blue()  - beg.blue()))  / m_itemHeight;

    if (!width || !height)
        return;

    uint stride = sizeof(uint32_t) * width;

    // Note: this safety margin allows us to skip some
    // checks that would otherwise be needed below.
    const uint safety_margin = stride + sizeof(uint32_t);

    uint data_size = stride * height;
    unsigned char *data = new unsigned char[data_size + safety_margin];
    uint32_t *ptr = (uint32_t*) data;
    uint black = qRgba(0,0,0,alpha);

    for (uint x = 0; x < width; x++, ptr++)
        *ptr = black; // safe due to safety_margin

#define CUR_POS (((uchar*)ptr)-data)
    for (uint y = 1; y+1 < height; y++)
    {
        *ptr = black; // safe due to safety_margin
        ptr++;

        int r = (int) (beg.red()   + (y * rstep));
        int g = (int) (beg.green() + (y * gstep));
        int b = (int) (beg.blue()  + (y * bstep));
        uint32_t color = qRgba(r,g,b,alpha);

        if (CUR_POS + stride < data_size)
        {
            for (uint x = 1; x+1 < width; x++, ptr++)
                *ptr = color;
            *ptr = black;
            ptr++;
        }
    }
    if (CUR_POS + stride < data_size + safety_margin)
    {
        for (uint x = 0; x < width; x++, ptr++)
            *ptr = black;
    }
#undef CUR_POS

    {
        QImage img;
        img = (alpha<255) ?
            QImage(data, width, height, QImage::Format_ARGB32) :
            QImage(data, width, height, QImage::Format_RGB32);

        osdImg.Load(img);
    }
    delete[] data;
}

void OSDListBtnType::LoadPixmap(OSDTypeImage& pix, const QString& fileName)
{
    QString path = GetThemesParentDir() + "default/lb-";
    pix.Load(path + fileName + ".png", m_wmult, m_hmult);
}

void OSDListBtnType::SendItemSelected(OSDListBtnTypeItem* item)
{
    if (!m_listener)
        return;

    OSDListBtnItemSelectedEvent *event =
        new OSDListBtnItemSelectedEvent(item->text());
    QCoreApplication::postEvent(m_listener, event);
}

/////////////////////////////////////////////////////////////////////////////
OSDListBtnTypeItem::OSDListBtnTypeItem(
    OSDListBtnType *lbtype,     const QString  &text,
    OSDTypeImage   *pixmap,     bool            checkable,
    bool            showArrow,  CheckState      state)
    : m_parent(lbtype),       m_pixmap(pixmap),
      m_data(NULL),           m_text(text),
      m_group(QString::null), m_state(state),
      m_showArrow(showArrow), m_checkable(checkable),
      m_checkRect(0,0,0,0),   m_arrowRect(0,0,0,0),
      m_pixmapRect(0,0,0,0),  m_textRect(0,0,0,0)
{
    if (!m_parent->m_initialized)
        m_parent->Init();

    OSDTypeImage &checkPix = m_parent->m_checkNonePix;
    OSDTypeImage &arrowPix = m_parent->m_arrowPix;

    int margin = m_parent->m_itemMargin;
    int width  = m_parent->m_rect.width();
    int height = m_parent->m_itemHeight;
    int cw     = checkPix.ImageSize().width();
    int ch     = checkPix.ImageSize().height();
    int aw     = arrowPix.ImageSize().width();
    int ah     = arrowPix.ImageSize().height();
    int pw     = m_pixmap ? m_pixmap->ImageSize().width() : 0;
    int ph     = m_pixmap ? m_pixmap->ImageSize().height() : 0;

    if (m_checkable)
        m_checkRect  = QRect(margin, (height - ch)/2, cw, ch);

    if (m_showArrow)
        m_arrowRect  = QRect(width - aw - margin, (height - ah)/2, aw, ah);

    if (m_pixmap)
    {
        int tmp = (m_checkable) ? (2 * margin + m_checkRect.width()) : margin;
        m_pixmapRect = QRect(tmp, (height - ph)/2, pw, ph);
    }

    int tx = margin, tw = width - (2 * margin);
    tx += (m_checkable) ? m_checkRect.width()  + margin : 0;
    tx += (m_pixmap)    ? m_pixmapRect.width() + margin : 0;
    tw -= (m_checkable) ? m_checkRect.width()  + margin : 0;
    tw -= (m_showArrow) ? m_arrowRect.width()  + margin : 0;
    tw -= (m_pixmap)    ? m_pixmapRect.width() + margin : 0;
    m_textRect = QRect(tx, 0, tw, height);

    m_parent->InsertItem(this);
}

OSDListBtnTypeItem::~OSDListBtnTypeItem()
{
    if (m_parent)
        m_parent->RemoveItem(this);
}

void OSDListBtnTypeItem::paint(OSDSurface *surface, TTFFont *font,
                               int fade, int maxfade, int x, int y)
{
    if (this == m_parent->GetItemCurrent())
    {
        if (m_parent->m_active)
            m_parent->m_itemSelActPix.Draw(surface, fade, maxfade, x, y);
        else
            m_parent->m_itemSelInactPix.Draw(surface, fade, maxfade, x, y);

        if (m_showArrow)
        {
            QRect ar(m_arrowRect);
            ar.translate(x, y);
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
        cr.translate(x, y);

        if (m_state == HalfChecked)
            m_parent->m_checkHalfPix.Draw(surface, fade, maxfade,
                                          cr.x(), cr.y());
        else if (m_state == FullChecked)
            m_parent->m_checkFullPix.Draw(surface, fade, maxfade,
                                          cr.x(), cr.y());
        else
            m_parent->m_checkNonePix.Draw(surface, fade, maxfade,
                                          cr.x(), cr.y());
    }

    if (m_pixmap)
    {
        QRect pr(m_pixmapRect);
        pr.translate(x, y);
        m_pixmap->Draw(surface, fade, maxfade, pr.x(), pr.y());
    }

    QRect tr(m_textRect);
    tr.translate(x, y);
    tr.translate(0, font->Size() / 4);
    font->DrawString(surface, tr.x(), tr.y(), m_text, tr.right(), tr.bottom());
}
