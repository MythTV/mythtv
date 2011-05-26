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
#include <algorithm>
using namespace std;

#include <QLabel>
#include <QPixmap>
#include <QPainter>

#include "mythverbose.h"
#include "lcddevice.h"

#include "uilistbtntype.h"

#include "mythfontproperties.h"
#include "mythuihelper.h"

#define LOC      QString("UIListBtn*: ")
#define LOC_WARN QString("UIListBtn*, Warning: ")
#define LOC_ERR  QString("UIListBtn*, Error: ")

UIListGenericTree::UIListGenericTree(UIListGenericTree *parent,
                                     const QString &name, const QString &action,
                                     int check, QPixmap *image)
                 : GenericTree(name)
{
    m_check = check;
    m_action = action;
    m_image = image;
    m_active = true;

    m_physitem = NULL;

    if (!action.isEmpty() && !action.isNull())
        setSelectable(true);

    if (parent)
    {
        parent->addNode(this);
        parent->setDrawArrow(true);
    }
}

UIListGenericTree::~UIListGenericTree()
{
}

void UIListGenericTree::RemoveFromParent(void)
{
    if (m_physitem)
        delete m_physitem;
    m_physitem = NULL;

    if (getParent())
    {
        if (getParent()->childCount() == 1)
            ((UIListGenericTree *)getParent())->setDrawArrow(false);

        getParent()->removeNode(this);
    }
}

void UIListGenericTree::setText(const QString &text)
{
    setString(text);
    if (m_physitem)
        m_physitem->setText(text);
}

void UIListGenericTree::setPixmap(QPixmap *pixmap)
{
    if (m_physitem)
        m_physitem->setPixmap(pixmap);
    m_image = pixmap;
}

void UIListGenericTree::setDrawArrow(bool flag)
{
    if (m_physitem)
        m_physitem->setDrawArrow(flag);
}

void UIListGenericTree::setCheck(int flag)
{
    m_check = flag;

    if (m_physitem)
    {
        m_physitem->setCheckable(flag >= 0);
        m_physitem->setChecked((UIListBtnTypeItem::CheckState)flag);
    }
}

void UIListGenericTree::setActive(bool flag)
{
    if (m_physitem)
        m_physitem->setOverrideInactive(!flag);
    m_active = flag;
}

bool UIListGenericTree::getActive(void)
{
    return m_active;
}

bool UIListGenericTree::movePositionUpDown(bool flag)
{
    if (getParent())
        getParent()->MoveItemUpDown(this, flag);

    if (m_physitem)
        return m_physitem->moveUpDown(flag);

    return false;
}

//////////////////////////////////////////////////////////////////////////////

UIListTreeType::UIListTreeType(const QString &name, const QRect &area,
                               const QRect &levelsize, int levelspacing,
                               int order)
              : UIType(name)
{
    m_totalarea = area;
    m_levelsize = levelsize;
    m_levelspacing = levelspacing;

    levels = 0;
    curlevel = -1;

    treetop = NULL;
    currentpos = NULL;

    currentlevel = NULL;

    m_active = NULL;
    m_inactive = NULL;
    takes_focus = true;

    SetItemRegColor(Qt::black,QColor(80,80,80),100);
    SetItemSelColor(QColor(82,202,56),QColor(52,152,56),255);

    m_spacing = 0;
    m_margin = 0;

    m_order = order;

    //
    //  This is a flag that renders the whole GUI list tree inactive (draws
    //  everything as unselected)
    //

    list_tree_active = true;
}

UIListTreeType::~UIListTreeType()
{
    while (!listLevels.empty())
    {
        delete listLevels.back();
        listLevels.pop_back();
    }
}

void UIListTreeType::SetItemRegColor(const QColor& beg, const QColor& end,
                                     uint alpha)
{
    m_itemRegBeg   = beg;
    m_itemRegEnd   = end;
    m_itemRegAlpha = alpha;
}

void UIListTreeType::SetItemSelColor(const QColor& beg, const QColor& end,
                                     uint alpha)
{
    m_itemSelBeg   = beg;
    m_itemSelEnd   = end;
    m_itemSelAlpha = alpha;
}

void UIListTreeType::SetFontActive(fontProp *font)
{
    m_active = font;
}

void UIListTreeType::SetFontInactive(fontProp *font)
{
    m_inactive = font;
}

void UIListTreeType::SetSpacing(int spacing)
{
    m_spacing = spacing;
}

void UIListTreeType::SetMargin(int margin)
{
    m_margin = margin;
}

void UIListTreeType::SetTree(UIListGenericTree *toplevel)
{
    if (treetop)
    {
        while (!listLevels.empty())
        {
            delete listLevels.back();
            listLevels.pop_back();
        }
        currentlevel = NULL;
        treetop = NULL;
        currentpos = NULL;
        levels = 0;
        curlevel = -1;
    }

    levels = - 1;

    currentpos = (UIListGenericTree *)toplevel->getChildAt(0);

    if (!currentpos)
    {
        //
        //  Not really an error, as UIListTreeType is perfectly capable of drawing an empty list.
        //

        // cerr << "No top-level children?\n";
        return;
    }

    treetop = toplevel;

    CreateLevel(0);

    currentlevel = GetLevel(0);

    if (!currentlevel)
    {
        cerr << "Something is seriously wrong (currentlevel = NULL)\n";
        return;
    }

    FillLevelFromTree(toplevel, currentlevel);

    currentlevel->SetVisible(true);
    currentlevel->SetActive(true);

    currentpos = (UIListGenericTree *)(currentlevel->GetItemFirst()->getData());
    curlevel = 0;

    emit requestUpdate();
    emit itemEntered(this, currentpos);
}

void UIListTreeType::CreateLevel(int level)
{
    if (level > levels)
    {
        int oldlevels = levels + 1;
        levels = level;
        for (int i = oldlevels; i <= levels; i++)
        {
            QString levelname = QString("level%1").arg(i + 1);

            QRect curlevelarea = m_levelsize;
            curlevelarea.translate(m_totalarea.x(), m_totalarea.y());
            curlevelarea.translate(
                (m_levelsize.width() + m_levelspacing) * i, 0);

            UIListBtnType *newlevel = new UIListBtnType(levelname, curlevelarea,
                                                        m_order, false, true);

            newlevel->SetFontActive(m_active);
            newlevel->SetFontInactive(m_inactive);
            newlevel->SetItemRegColor(m_itemRegBeg, m_itemRegEnd,
                                      m_itemRegAlpha);
            newlevel->SetItemSelColor(m_itemSelBeg, m_itemSelEnd,
                                      m_itemSelAlpha);
            newlevel->SetSpacing(m_spacing);
            newlevel->SetMargin(m_margin);
            newlevel->SetParentListTree(this);

            listLevels.append(newlevel);
        }
    }
}

UIListGenericTree *UIListTreeType::GetCurrentPosition(void)
{
    return currentpos;
}

void UIListTreeType::Draw(QPainter *p, int order, int context)
{
    if (hidden)
    {
        return;
    }

    if (m_context != -1 && m_context != context)
        return;

    if (m_order != order)
        return;

    int maxx = 0;
    QList<UIListBtnType*>::iterator it = listLevels.begin();
    for (; it != listLevels.end(); ++it)
    {
        if ((*it)->IsVisible())
        {
            maxx = (*it)->GetArea().right();
        }
    }

    for (it = listLevels.begin(); it != listLevels.end(); ++it)
    {
        if (!(*it)->IsVisible())
        {
            break;
        }

        int offset = 0;
        if (maxx > m_totalarea.right())
        {
            offset = -1 * (maxx - m_totalarea.right());
        }

        (*it)->SetDrawOffset(offset);

        if ((*it)->GetArea().right() + offset > m_totalarea.left())
        {
            (*it)->Draw(p, order, context, list_tree_active);
        }
    }
}

void UIListTreeType::DrawRegion(QPainter *p, QRect &area, int order, int context)
{
    if (m_context != -1 && m_context != context)
        return;

    QList<UIListBtnType*>::iterator it = listLevels.begin();
    int maxx = 0;
    for (; it != listLevels.end(); ++it)
    {
        if ((*it)->IsVisible())
            maxx = (*it)->GetArea().right();
    }

    for (it = listLevels.begin(); it != listLevels.end(); ++it)
    {
        if (!(*it)->IsVisible())
            break;

        int offset = 0;
        if (maxx > m_totalarea.right())
            offset = -1 * (maxx - m_totalarea.right());
        (*it)->SetDrawOffset(offset);

        QRect drawRect = (*it)->GetArea();
        drawRect.translate(offset, 0);
        drawRect.translate(m_parent->GetAreaRect().x(),
                           m_parent->GetAreaRect().y());

        if ((*it)->GetArea().right() + offset > m_totalarea.left() &&
            drawRect == area)
        {
            (*it)->SetDrawOffset(0 - (*it)->GetArea().x());
            (*it)->Draw(p, order, context, list_tree_active);
            (*it)->SetDrawOffset(offset);
        }
    }
}

void UIListTreeType::ClearLevel(UIListBtnType *list)
{
    UIListBtnTypeItem *clear = list->GetItemFirst();
    while (clear)
    {
        UIListGenericTree *gt = (UIListGenericTree *)clear->getData();
        gt->setItem(NULL);
        clear = list->GetItemNext(clear);
    }

    list->Reset();
}

void UIListTreeType::RefreshCurrentLevel(void)
{
    if (currentlevel)
    {
        UIListBtnType::iterator it = currentlevel->begin();
        for (; it != currentlevel->end(); ++it)
        {
            UIListGenericTree *ui = (UIListGenericTree*)((*it)->getData());
            ui->setActive(ui->getActive());
        }
    }
}

void UIListTreeType::FillLevelFromTree(UIListGenericTree *item,
                                       UIListBtnType *list)
{
    if (!item || !list)
        return;

    ClearLevel(list);

    vector<GenericTree*> itemlist = item->getAllChildren();

    vector<GenericTree*>::const_iterator it = itemlist.begin();

    for ( ; it != itemlist.end(); ++it)
    {
        UIListGenericTree *uichild =
            dynamic_cast<UIListGenericTree*>(*it);

        if (!uichild)
            continue;

        UIListBtnTypeItem *newitem;

        int check = uichild->getCheck();
        newitem = new UIListBtnTypeItem(list, uichild->getString(),
                                        uichild->getImage(), (check >= 0),
                                        (UIListBtnTypeItem::CheckState)check,
                                        (uichild->childCount() > 0));
        newitem->setData(uichild);

        uichild->setItem(newitem);

        if (!uichild->getActive())
            newitem->setOverrideInactive(true);
    }
}

UIListBtnType *UIListTreeType::GetLevel(int levelnum)
{
    if ((uint)levelnum > (uint)listLevels.size())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "OOB GetLevel call");
        return NULL;
    }

    return listLevels[levelnum];
}

void UIListTreeType::SetCurrentPosition(void)
{
    if (!currentlevel)
        return;

    UIListBtnTypeItem *lbt = currentlevel->GetItemCurrent();

    if (!lbt)
        return;

    currentpos = (UIListGenericTree *)(lbt->getData());
    emit itemEntered(this, currentpos);
}

void UIListTreeType::Redraw(void)
{
    if (!currentlevel)
    {
        return;
    }
    if (currentlevel->GetCount() == 0)
        MoveLeft();
    else
        emit requestUpdate();
}

void UIListTreeType::RedrawCurrent(void)
{
    if (!currentlevel)
    {
        return;
    }
    QRect dr = currentlevel->GetArea();
    dr.translate(currentlevel->GetDrawOffset(), 0);
    dr.translate(m_parent->GetAreaRect().x(), m_parent->GetAreaRect().y());

    emit requestRegionUpdate(dr);
}

void UIListTreeType::MoveDown(MovementUnit unit)
{
    if (!currentlevel)
    {
        return;
    }
    currentlevel->MoveDown((UIListBtnType::MovementUnit)unit);
    SetCurrentPosition();
    RedrawCurrent();
}

void UIListTreeType::MoveDown(int count)
{
    if (!currentlevel)
    {
        return;
    }
    currentlevel->MoveDown(count);
    SetCurrentPosition();
    RedrawCurrent();
}

void UIListTreeType::MoveUp(MovementUnit unit)
{
    if (!currentlevel)
    {
        return;
    }
    currentlevel->MoveUp((UIListBtnType::MovementUnit)unit);
    SetCurrentPosition();
    RedrawCurrent();
}

void UIListTreeType::MoveUp(int count)
{
    if (!currentlevel)
    {
        return;
    }
    currentlevel->MoveUp(count);
    SetCurrentPosition();
    RedrawCurrent();
}

void UIListTreeType::MoveLeft(bool do_refresh)
{
    if (!currentlevel)
    {
        return;
    }
    if (curlevel > 0)
    {
        ClearLevel(currentlevel);
        currentlevel->SetVisible(false);

        curlevel--;

        currentlevel = GetLevel(curlevel);
        currentlevel->SetActive(true);
        SetCurrentPosition();

        if (do_refresh)
        {
            Redraw();
        }
    }
}

bool UIListTreeType::MoveRight(bool do_refresh)
{
    if (!currentpos || !currentlevel)
    {
        return true;
    }
    if (currentpos->childCount() > 0)
    {
        currentlevel->SetActive(false);

        curlevel++;

        CreateLevel(curlevel);

        currentlevel = GetLevel(curlevel);

        FillLevelFromTree(currentpos, currentlevel);

        currentlevel->SetVisible(true);
        currentlevel->SetActive(true);
        SetCurrentPosition();

        if (do_refresh)
        {
            Redraw();
        }

        return true;
    }

    return false;
}

void UIListTreeType::calculateScreenArea()
{
    QRect r = m_totalarea;
    r.translate(m_parent->GetAreaRect().left(),
                m_parent->GetAreaRect().top());
    screen_area = r;

}

void UIListTreeType::GoHome()
{

    while(curlevel > 0)
    {
        MoveLeft(false);
    }
    MoveUp(MoveMax);
    Redraw();
}

void UIListTreeType::select()
{
    if (currentpos)
    {
        emit selected(currentpos);
        emit itemSelected(this, currentpos);
    }
}

QStringList UIListTreeType::getRouteToCurrent()
{
    //
    //  Tell whatever is asking a set of strings (node string/title values)
    //  that make a route to the current node
    //

    QStringList route_to_current;
    if (currentpos)
    {
        GenericTree *climber = currentpos;
        route_to_current.push_front(climber->getString());
        while( (climber = climber->getParent()) )
        {
            route_to_current.push_front(climber->getString());
        }
    }
    return route_to_current;
}

bool UIListTreeType::tryToSetCurrent(QStringList route)
{


    //
    //  NB: this method is extremely inefficient. We move all the way left
    //  (up the tree), then parse our way right (down the tree). Dumb, dumb,
    //  dumb. Someone who actually undersands how this class works is more
    //  than welcome to improve on this.
    //


    //
    //  Move the current node all the way to the "left" (root)
    //

    while(curlevel > 0)
    {
        MoveLeft(false); // false --> don't redraw the screen
    }

    //
    //  If the route is empty, we are done
    //

    if (route.count() < 2)
    {
        return false;
    }

    //
    //  If we have no currentpos (no active node), we are done
    //

    if (!currentpos || !currentlevel)
    {
        return false;
    }

    //
    //  Make sure the absolute root node name/string matches
    //

    if (currentpos->getParent()->getString() != route[0])
    {
        return false;
    }

    //
    //  Make the current position correct
    //

    GenericTree *first_child = currentpos->getParent()->getChildByName(route[1]);
    if (!first_child)
    {
        return false;
    }
    else
    {
        currentpos = (UIListGenericTree *)first_child;
        currentlevel->MoveToNamedPosition(currentpos->getString());
    }


    //
    //  Go as far down the rest of the route as possible
    //

    bool keep_going = true;
    QStringList::Iterator it = route.begin();
    ++it;
    ++it;
    while(keep_going &&  it != route.end())
    {
        GenericTree *next_child = currentpos->getChildByName(*it);
        if (next_child)
        {
            MoveRight(false);
            currentpos = (UIListGenericTree *)next_child;
            if (!currentlevel->MoveToNamedPosition(currentpos->getString()))
            {
                cerr << "uilistbtntype.o: had problem finding "
                     << "something it knows is there"
                     << endl;
                keep_going = false;
            }
        }
        else
        {
            keep_going = false;

            //
            //  Try to at least move to same level that the route was at
            //

            MoveRight(false);
        }
        ++it;
    }

    return keep_going;
}

bool UIListTreeType::incSearchStart(void)
{
    bool res = currentlevel->incSearchStart();

    if (res)
    {
        SetCurrentPosition();
        RedrawCurrent();
    }
    return (res);
}

bool UIListTreeType::incSearchNext(void)
{
    bool res = currentlevel->incSearchNext();

    if (res)
    {
        SetCurrentPosition();
        RedrawCurrent();
    }
    return (res);
}

int UIListTreeType::getDepth()
{
    return curlevel;
}

void UIListTreeType::setActive(bool x)
{
    list_tree_active = x;
    Redraw();
}

void UIListTreeType::enter()
{
    if (currentpos)
    {
        emit itemEntered(this, currentpos);
    }
}

void UIListTreeType::moveAwayFrom(UIListGenericTree *node)
{
    if (!currentpos || !node)
    {
        return;
    }

    //
    //  Something that is programatically altering the tree that this list
    //  displays may want to delete an item. That item might be the
    //  currentpos highlight item. This method allows client code to get the
    //  UIListTreeType to check if a given node is currentpos, and, if so,
    //  to do something about it. This is preferable to just segfaulting :-)
    //

    if (currentpos == node)
    {
        GenericTree *sibling = node->prevSibling(1);
        if (sibling)
        {
            if (UIListGenericTree *ui_sibling = dynamic_cast<UIListGenericTree*>(sibling) )
            {
                currentpos = ui_sibling;
                return;
            }
        }
        sibling = node->nextSibling(1);
        if (sibling)
        {
            if (UIListGenericTree *ui_sibling = dynamic_cast<UIListGenericTree*>(sibling) )
            {
                currentpos = ui_sibling;
                return;
            }
        }
        currentpos = NULL;
    }
}

int UIListTreeType::getNumbItemsVisible()
{
    if (!currentlevel)
    {
        return 0;
    }
    return (int) currentlevel->GetNumbItemsVisible();
}

bool UIListTreeType::takeFocus()
{
    setActive(true);
    return UIType::takeFocus();
}

void UIListTreeType::looseFocus()
{
    setActive(false);
    UIType::looseFocus();
}

//////////////////////////////////////////////////////////////////////////////

UIListBtnType::UIListBtnType(const QString& name, const QRect& area,
                             int order, bool showArrow, bool showScrollArrows)
             : UIType(name)
{
    m_parentListTree   = NULL;
    m_order            = order;
    m_rect             = area;

    m_showArrow        = showArrow;
    m_showScrollArrows = showScrollArrows;

    m_active           = false;
    m_visible          = true;
    takes_focus        = true;
    m_showUpArrow      = false;
    m_showDnArrow      = false;

    m_topItem = 0;
    m_selItem = 0;
    m_selPosition = 0;
    m_topPosition = 0;
    m_itemCount = 0;
    m_incSearch = "";
    m_bIncSearchContains = false;

    m_initialized     = false;
    m_clearing        = false;
    m_itemSpacing     = 0;
    m_itemMargin      = 0;
    m_itemHeight      = 0;
    m_itemsVisible    = 0;
    m_fontActive      = 0;
    m_fontInactive    = 0;
    m_justify         = Qt::AlignLeft | Qt::AlignVCenter;

    m_xdrawoffset     = 0;

    SetItemRegColor(Qt::black,QColor(80,80,80),100);
    SetItemSelColor(QColor(82,202,56),QColor(52,152,56),255);
}

UIListBtnType::~UIListBtnType()
{
    Reset();
}

void UIListBtnType::SetItemRegColor(const QColor& beg,
                                    const QColor& end,
                                    uint alpha)
{
    m_itemRegBeg   = beg;
    m_itemRegEnd   = end;
    m_itemRegAlpha = alpha;
}

void UIListBtnType::SetItemSelColor(const QColor& beg,
                                    const QColor& end,
                                    uint alpha)
{
    m_itemSelBeg   = beg;
    m_itemSelEnd   = end;
    m_itemSelAlpha = alpha;
}

void UIListBtnType::SetFontActive(fontProp *font)
{
    m_fontActive   = font;
}

void UIListBtnType::SetFontInactive(fontProp *font)
{
    m_fontInactive = font;
}

void UIListBtnType::SetSpacing(int spacing)
{
    m_itemSpacing = spacing;
}

void UIListBtnType::SetMargin(int margin)
{
    m_itemMargin = margin;
}

void UIListBtnType::SetActive(bool active)
{
    m_active = active;
}

void UIListBtnType::Reset()
{
    m_clearing = true;

    while (!m_itemList.empty())
    {
        delete m_itemList.back();
        m_itemList.pop_back();
    }

    m_clearing = false;

    m_topItem     = 0;
    m_selItem     = 0;
    m_selPosition = 0;
    m_topPosition = 0;
    m_itemCount   = 0;

    m_showUpArrow = false;
    m_showDnArrow = false;
}

void UIListBtnType::InsertItem(UIListBtnTypeItem *item)
{
    UIListBtnTypeItem* lastItem = NULL;

    if (!m_itemList.isEmpty())
        lastItem = m_itemList.last();

    m_itemList.append(item);

    m_itemCount++;

    if (m_showScrollArrows && m_itemCount > (int)m_itemsVisible)
        m_showDnArrow = true;
    else
        m_showDnArrow = false;

    if (!lastItem)
    {
        m_topItem = item;
        m_selItem = item;
        m_selPosition = m_topPosition = 0;
        emit itemSelected(item);
    }
}

void UIListBtnType::RemoveItem(UIListBtnTypeItem *item)
{
    if (m_clearing)
        return;

    if (m_itemList.isEmpty() || m_itemList.indexOf(item) == -1)
        return;

    if (item == m_topItem)
    {
        if (m_topItem != m_itemList.last())
        {
            ++m_topPosition;
            m_topItem = m_itemList[m_topPosition];
        }
        else if (m_topItem != m_itemList.first())
        {
            --m_topPosition;
            m_topItem = m_itemList[m_topPosition];
        }
        else
        {
            m_topItem = NULL;
            m_topPosition = 0;
        }
    }

    if (item == m_selItem)
    {
        if (m_selItem != m_itemList.last())
        {
            ++m_selPosition;
            m_selItem = m_itemList[m_selPosition];
        }
        else if (m_selItem != m_itemList.first())
        {
            --m_selPosition;
            m_selItem = m_itemList[m_selPosition];
        }
        else
        {
            m_selItem = NULL;
            m_selPosition = 0;
        }
    }

    m_itemList.removeAll(item);

    m_itemCount--;

    if (!m_itemList.isEmpty() && m_topItem != m_itemList.first())
        m_showUpArrow = true;
    else
        m_showUpArrow = false;

    if (m_topPosition + (int)m_itemsVisible < m_itemCount)
        m_showDnArrow = true;
    else
        m_showDnArrow = false;

    if (m_selItem)
        emit itemSelected(m_selItem);
}

void UIListBtnType::SetItemCurrent(UIListBtnTypeItem* item)
{
    if (m_itemList.isEmpty())
        return;

    UIListBtnTypeItem *cur;
    bool found = false;
    m_selPosition = 0;
    while ((cur = m_itemList[m_selPosition]) != 0)
    {
        if (cur == item)
        {
            found = true;
            break;
        }

        ++m_selPosition;
    }

    if (!found)
    {
        m_selPosition = 0;
    }

    m_selItem = item;
    m_topItem = m_selItem;
    m_topPosition = m_selPosition;

    // centre the selected item in the list
    int count = m_itemsVisible / 2;

    while (count && m_topPosition > 0)
    {
        --m_topPosition;
        --count;
    }

    // backup if we have scrolled past the end of the list
    if (m_topPosition + (int)m_itemsVisible > m_itemCount)
    {
        while (m_topPosition > 0 && m_topPosition + (int)m_itemsVisible > m_itemCount)
        {
            --m_topPosition;
        }
    }

    if (m_topPosition < 0 || m_topPosition > (int)m_itemList.size())
        m_topPosition = 0;

    m_topItem = m_itemList[m_topPosition];

    if (m_topItem != m_itemList.first())
        m_showUpArrow = true;
    else
        m_showUpArrow = false;

    if (m_topPosition + (int)m_itemsVisible < m_itemCount)
        m_showDnArrow = true;
    else
        m_showDnArrow = false;

    emit itemSelected(m_selItem);
}

void UIListBtnType::SetItemCurrent(int current)
{
    if (m_itemList.empty())
        return;

    SetItemCurrent(
        ((current < 0) || (current >= (int)m_itemList.size())) ?
        m_itemList.first() : m_itemList[current]);
}

UIListBtnTypeItem* UIListBtnType::GetItemCurrent()
{
    return m_selItem;
}

UIListBtnTypeItem* UIListBtnType::GetItemFirst()
{
    if (m_itemList.isEmpty())
        return NULL;

    return m_itemList.first();
}

UIListBtnTypeItem* UIListBtnType::GetItemNext(UIListBtnTypeItem *item)
{
    int idx = m_itemList.indexOf(item);
    if ((idx < 0) || ((idx + 1) >= m_itemList.size()))
        return NULL;

    return m_itemList[idx + 1];
}

int UIListBtnType::GetCount()
{
    return m_itemCount;
}

UIListBtnTypeItem* UIListBtnType::GetItemAt(int pos)
{
    return m_itemList[pos];
}

int UIListBtnType::GetItemPos(UIListBtnTypeItem* item)
{
    return m_itemList.indexOf(item);
}

void UIListBtnType::MoveUp(MovementUnit unit)
{
    if (m_itemList.isEmpty())
        return;

    int pos = m_selPosition;
    if (pos == -1)
        return;

    switch (unit)
    {
        case MoveItem:
            if (m_selPosition > 0)
                --m_selPosition;
            break;
        case MovePage:
            if (pos > (int)m_itemsVisible)
            {
                for (int i = 0; i < (int)m_itemsVisible; i++)
                {
                    --m_selPosition;
                }
                break;
            }
            // fall through
        case MoveMax:
            m_selPosition = 0;
            break;
    }

    if (!m_itemList[m_selPosition])
        return;

    m_selItem = m_itemList[m_selPosition];

    if (m_selPosition <= m_topPosition)
    {
        m_topItem = m_selItem;
        m_topPosition = m_selPosition;
    }

    if (m_topItem != m_itemList.first())
        m_showUpArrow = true;
    else
        m_showUpArrow = false;

    if (m_topPosition + (int)m_itemsVisible < m_itemCount)
        m_showDnArrow = true;
    else
        m_showDnArrow = false;

    emit itemSelected(m_selItem);
}

void UIListBtnType::MoveUp(int count)
{
    if (m_itemList.isEmpty())
        return;

    int pos = m_selPosition;
    if (pos == -1)
        return;

    if (pos > count)
    {
        for (int i = 0; i < count; i++)
        {
            --m_selPosition;
        }
    }

    if (!m_itemList[m_selPosition])
        return;

    m_selItem = m_itemList[m_selPosition];

    if (m_selPosition <= m_topPosition)
    {
        m_topItem = m_selItem;
        m_topPosition = m_selPosition;
    }

    if (m_topItem != m_itemList.first())
        m_showUpArrow = true;
    else
        m_showUpArrow = false;

    if (m_topPosition + (int)m_itemsVisible < m_itemCount)
        m_showDnArrow = true;
    else
        m_showDnArrow = false;

    emit itemSelected(m_selItem);
}

void UIListBtnType::MoveDown(MovementUnit unit)
{
    if (m_itemList.isEmpty())
        return;

    int pos = m_selPosition;
    if (pos == -1)
        return;

    switch (unit)
    {
        case MoveItem:
            if (m_selPosition + 1 < (int)m_itemList.size())
                ++m_selPosition;
            break;
        case MovePage:
            if ((pos + (int)m_itemsVisible) < m_itemCount - 1)
            {
                for (int i = 0; i < (int)m_itemsVisible; i++)
                {
                    ++m_selPosition;
                }
                break;
            }
            // fall through
        case MoveMax:
            m_selPosition = m_itemCount - 1;
            break;
    }

    if (!m_itemList[m_selPosition])
        return;

    m_selItem = m_itemList[m_selPosition];

    while (m_topPosition + (int)m_itemsVisible < m_selPosition + 1)
        ++m_topPosition;

    m_topItem = m_itemList[m_topPosition];

    if (m_topItem != m_itemList.first())
        m_showUpArrow = true;
    else
        m_showUpArrow = false;

    if (m_topPosition + (int)m_itemsVisible < m_itemCount)
        m_showDnArrow = true;
    else
        m_showDnArrow = false;

    emit itemSelected(m_selItem);
}

void UIListBtnType::MoveDown(int count)
{
    if (m_itemList.isEmpty())
        return;

    int pos = m_selPosition;
    if (pos == -1)
        return;

    if ((pos + count) < m_itemCount - 1)
    {
        for (int i = 0; i < count; i++)
        {
             ++m_selPosition;
        }
    }

    if (!m_itemList[m_selPosition])
        return;

    m_selItem = m_itemList[m_selPosition];

    while (m_topPosition + (int)m_itemsVisible < m_selPosition + 1)
        ++m_topPosition;

    m_topItem = m_itemList[m_topPosition];

    if (m_topItem != m_itemList.first())
        m_showUpArrow = true;
    else
        m_showUpArrow = false;

    if (m_topPosition + (int)m_itemsVisible < m_itemCount)
        m_showDnArrow = true;
    else
        m_showDnArrow = false;

    emit itemSelected(m_selItem);
}

bool UIListBtnType::MoveToNamedPosition(const QString &position_name)
{
    if (m_itemList.isEmpty())
        return false;

    if (m_selPosition < 0)
    {
        return false;
    }

    m_selPosition = 0;

    bool found_it = false;
    while(m_itemList[m_selPosition])
    {
        if (m_itemList[m_selPosition]->text() == position_name)
        {
            found_it = true;
            break;
        }
        ++m_selPosition;
    }

    if (!found_it)
    {
        m_selPosition = -1;
        return false;
    }

    m_selItem = m_itemList[m_selPosition];

    while (m_topPosition + (int)m_itemsVisible < m_selPosition + 1)
        ++m_topPosition;

    m_topItem = m_itemList[m_topPosition];

    if (m_topItem != m_itemList.first())
        m_showUpArrow = true;
    else
        m_showUpArrow = false;

    if (m_topPosition + (int)m_itemsVisible < m_itemCount)
        m_showDnArrow = true;
    else
        m_showDnArrow = false;

    return true;
}

bool UIListBtnType::MoveItemUpDown(UIListBtnTypeItem *item, bool flag)
{
    if (m_itemList.isEmpty())
        return false;

    if (item != m_selItem)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Can't move non-selected item");
        return false;
    }

    if (item == m_itemList.front() && flag)
        return false;
    if (item == m_itemList.back() && !flag)
        return false;

    int oldpos = m_selPosition;
    int insertat = 0;
    bool dolast = false;

    if (flag)
    {
        insertat = m_selPosition - 1;
        if (item == m_itemList.back())
            dolast = true;
        else
            ++m_selPosition;

        if (item == m_topItem)
            ++m_topPosition;
    }
    else
        insertat = m_selPosition + 1;

    if (m_selPosition >= 0 && m_selPosition < m_itemList.size() &&
        m_itemList[m_selPosition] == item)
    {
        m_itemList.removeAt(m_selPosition);
    }
    else
    {
        m_itemList.removeAt(oldpos);
    }

    m_itemList.insert(insertat, item);

    if (flag)
    {
        MoveUp();
        if (!dolast)
            MoveUp();
    }
    else
        MoveDown();

    return true;
}

bool UIListBtnType::incSearchStart(void)
{
    MythPopupBox *popup = new MythPopupBox(GetMythMainWindow(),
                                           "incserach_popup");

    QLabel *caption = popup->addLabel(tr("Search"), MythPopupBox::Large);
    caption->setAlignment(Qt::AlignCenter);

    MythComboBox *modeCombo = new MythComboBox(false, popup, "mode_combo" );
    modeCombo->insertItem(tr("Starts with text"));
    modeCombo->insertItem(tr("Contains text"));
    popup->addWidget(modeCombo);

    MythLineEdit *searchEdit = new MythLineEdit(popup, "mode_combo");
    searchEdit->setText(m_incSearch);
    popup->addWidget(searchEdit);
    searchEdit->setFocus();

    popup->addButton(tr("Search"));
    popup->addButton(tr("Cancel"), popup, SLOT(reject()));

    DialogCode res = popup->ExecPopup();

    if (kDialogCodeButton0 == res)
    {
        m_incSearch = searchEdit->text();
        m_bIncSearchContains = (modeCombo->currentIndex() == 1);
        incSearchNext();
    }

    popup->hide();
    popup->deleteLater();

    return (kDialogCodeButton0 == res);
}

bool UIListBtnType::incSearchNext(void)
{
    if (!m_selItem)
    {
        return false;
    }

    //
    //  Move the active node to the node whose string value
    //  starts or contains the search text
    //
    uint i = m_selPosition;
    for (; i < (uint)m_itemList.size(); i++)
    {
        if (m_bIncSearchContains)
        {
            if (m_itemList[i]->text().indexOf(
                    m_incSearch, 0, Qt::CaseInsensitive) != -1)
            {
                break;
            }
        }
        else
        {
            if (m_itemList[i]->text().startsWith(
                    m_incSearch, Qt::CaseInsensitive))
            {
                break;
            }
        }
    }

    // if it is NULL, we reached the end of the list. wrap to the
    // beginning and try again
    if (i >= (uint)m_itemList.size())
    {
        for (i = 0; i < (uint)m_itemList.size(); i++)
        {
            // we're back at the current_node, which means there are no
            // matching nodes
            if (m_itemList[i] == m_selItem)
            {
                break;
            }

            if (m_bIncSearchContains)
            {
                if (m_itemList[i]->text().indexOf(
                        m_incSearch, 0, Qt::CaseInsensitive) != -1)
                {
                    break;
                }
            }
            else
            {
                if (m_itemList[i]->text().startsWith(
                        m_incSearch, Qt::CaseInsensitive))
                {
                    break;
                }
            }
        }
    }

    if (i < (uint)m_itemList.size())
    {
        SetItemCurrent(m_itemList[i]);
        return true;
    }

    return false;
}

void UIListBtnType::Draw(QPainter *p, int order, int context)
{
    //
    //  Just call the other Draw() function. Tried to accomplish the same
    //  goal with default parameters, but that broke MythGallery (?)
    //

    Draw(p, order, context, true);
}


void UIListBtnType::Draw(QPainter *p, int order, int context, bool active_on)
{
    if (!m_visible || hidden)
        return;

    if (!m_initialized)
        Init();

    if (m_order != order)
        return;

    if (m_context != -1 && m_context != context)
        return;

    //  Put something on the LCD device (if one exists)
    if (LCD *lcddev = LCD::Get())
    {
        if (m_active)
        {
            // add max of lcd height menu items either side of the selected item
            // let the lcdserver figure out which ones to display


            // move back up the list a little
            uint count = 0;
            int i;
            for (i = m_selPosition;
                 (i >= 0) && (count < lcddev->getLCDHeight()); --i, ++count);

            i = (i < 0) ? 0 : i;
            count = 0;

            QList<LCDMenuItem> menuItems;
            for (; ((uint)i < (uint)m_itemList.size()) &&
                     (count < lcddev->getLCDHeight() * 2); ++i, ++count)
            {
                UIListBtnTypeItem *curItem = m_itemList[i];
                QString msg = curItem->text();
                bool selected;
                CHECKED_STATE checkState = NOTCHECKABLE;
                if (curItem->checkable())
                {
                    if (curItem->state() == UIListBtnTypeItem::HalfChecked ||
                        curItem->state() == UIListBtnTypeItem::FullChecked)
                        checkState = CHECKED;
                    else
                        checkState = UNCHECKED;
                }

                if (curItem == m_selItem)
                    selected = true;
                else
                    selected = false;

                menuItems.append(LCDMenuItem(selected, checkState, msg));
            }

            QString title = "";

            if (m_parentListTree && m_parentListTree->getDepth() > 0)
                title = "<< ";
            else
                title = "   ";

            if ((m_selItem && m_selItem->getDrawArrow()) || m_showArrow)
                title += " >>";
            else
                title += "   ";

            if (!menuItems.isEmpty())
            {
                lcddev->switchToMenu(menuItems, title);
            }
        }
    }

    fontProp* font = m_active ? m_fontActive : m_fontInactive;

    if (!active_on)
    {
        font = m_fontInactive;
    }

    p->setFont(font->face);
    p->setPen(font->color);

    int x = m_rect.x() + m_xdrawoffset;

    int y = m_rect.y();

    for (int i = m_topPosition;
         (i < (int)m_itemList.size()) &&
             ((y - m_rect.y()) <= (m_contentsRect.height() - m_itemHeight));
         i++)
    {
        if (active_on && m_itemList[i]->getOverrideInactive())
        {
            font = m_fontInactive;
            p->setFont(font->face);
            p->setPen(font->color);
            m_itemList[i]->setJustification(m_justify);
            m_itemList[i]->paint(p, font, x, y, active_on);
            font = m_active ? m_fontActive : m_fontInactive;;
            p->setFont(font->face);
            p->setPen(font->color);
        }
        else
        {
            m_itemList[i]->setJustification(m_justify);
            m_itemList[i]->paint(p, font, x, y, active_on);
        }

        y += m_itemHeight + m_itemSpacing;
    }

    if (m_showScrollArrows)
    {
        if (m_showUpArrow)
            p->drawPixmap(x + m_arrowsRect.x(),
                          m_rect.y() + m_arrowsRect.y(),
                          m_upArrowActPix);
        else
            p->drawPixmap(x + m_arrowsRect.x(),
                          m_rect.y() + m_arrowsRect.y(),
                          m_upArrowRegPix);
        if (m_showDnArrow)
            p->drawPixmap(x + m_arrowsRect.x() +
                          m_upArrowRegPix.width() + m_itemMargin,
                          m_rect.y() + m_arrowsRect.y(),
                          m_dnArrowActPix);
        else
            p->drawPixmap(x + m_arrowsRect.x() +
                          m_upArrowRegPix.width() + m_itemMargin,
                          m_rect.y() + m_arrowsRect.y(),
                          m_dnArrowRegPix);
    }

}

void UIListBtnType::Init()
{
    QFontMetrics fm(m_fontActive->face);
    QSize sz1 = fm.size(Qt::TextSingleLine, "XXXXX");
    fm = QFontMetrics(m_fontInactive->face);
    QSize sz2 = fm.size(Qt::TextSingleLine, "XXXXX");
    m_itemHeight = std::max(
        sz1.height(), sz2.height()) + (int)(2 * m_itemMargin);

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

    QImage img(m_rect.width(), m_itemHeight, QImage::Format_ARGB32);

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

        m_itemRegPix = QPixmap::fromImage(img);
        QPainter p(&m_itemRegPix);

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
    }

    {
        float rstep = float(m_itemSelEnd.red() - m_itemSelBeg.red()) /
                      float(m_itemHeight);
        float gstep = float(m_itemSelEnd.green() - m_itemSelBeg.green()) /
                      float(m_itemHeight);
        float bstep = float(m_itemSelEnd.blue() - m_itemSelBeg.blue()) /
                      float(m_itemHeight);

        m_itemSelInactPix = QPixmap::fromImage(img);
        QPainter p(&m_itemSelInactPix);

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

        //img.setAlphaBuffer(false);

        m_itemSelActPix = QPixmap::fromImage(img);
        p.begin(&m_itemSelActPix);

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

    }

    if ((uint)m_itemList.size() > m_itemsVisible && m_showScrollArrows)
        m_showDnArrow = true;
    else
        m_showDnArrow = false;

    m_initialized = true;
}

void UIListBtnType::LoadPixmap(QPixmap& pix, const QString& fileName)
{
    QString file = "lb-" + fileName + ".png";

    QPixmap *p = GetMythUI()->LoadScalePixmap(file);
    if (p)
    {
        pix = *p;
        delete p;
    }
}

void UIListBtnType::calculateScreenArea()
{
    QRect r = m_rect;
    r.translate(m_parent->GetAreaRect().left(),
                m_parent->GetAreaRect().top());
    screen_area = r;
}

bool UIListBtnType::takeFocus()
{
    SetActive(true);
    return UIType::takeFocus();
}

void UIListBtnType::looseFocus()
{
    SetActive(false);
    UIType::looseFocus();
}

//////////////////////////////////////////////////////////////////////////////

UIListBtnTypeItem::UIListBtnTypeItem(
    UIListBtnType *parent, const QString &text,
    QPixmap *pixmap, bool checkable,
    CheckState state, bool showArrow) :
    m_parent(parent), m_text(text), m_pixmap(pixmap),
    m_checkable(checkable), m_state(state), m_data(NULL),

    m_checkRect(0,0,0,0), m_pixmapRect(0,0,0,0),
    m_textRect(0,0,0,0), m_arrowRect(0,0,0,0),

    m_showArrow(showArrow),

    m_overrideInactive(false),
    m_justify(Qt::AlignLeft | Qt::AlignVCenter)
{
    if (state >= NotChecked)
        m_checkable = true;

    CalcDimensions();

    m_parent->InsertItem(this);
}

void UIListBtnTypeItem::CalcDimensions(void)
{
    if (!m_parent->m_initialized)
        m_parent->Init();

    int  margin    = m_parent->m_itemMargin;
    int  width     = m_parent->m_rect.width();
    int  height    = m_parent->m_itemHeight;
    bool arrow = false;
    if (m_parent->m_showArrow || m_showArrow)
        arrow = true;

    QPixmap& checkPix = m_parent->m_checkNonePix;
    QPixmap& arrowPix = m_parent->m_arrowPix;

    int cw = checkPix.width();
    int ch = checkPix.height();
    int aw = arrowPix.width();
    int ah = arrowPix.height();
    int pw = m_pixmap ? m_pixmap->width() : 0;
    int ph = m_pixmap ? m_pixmap->height() : 0;

    if (m_checkable)
        m_checkRect = QRect(margin, (height - ch)/2, cw, ch);
    else
        m_checkRect = QRect(0,0,0,0);

    if (arrow)
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
                       (arrow ? m_arrowRect.width() + margin : 0) -
                       (m_pixmap ? m_pixmapRect.width() + margin : 0),
                       height);
}

UIListBtnTypeItem::~UIListBtnTypeItem()
{
    if (m_parent)
        m_parent->RemoveItem(this);
}

QString UIListBtnTypeItem::text() const
{
    return m_text;
}

void UIListBtnTypeItem::setText(const QString &text)
{
    m_text = text;
    CalcDimensions();
}

const QPixmap* UIListBtnTypeItem::pixmap() const
{
    return m_pixmap;
}

void UIListBtnTypeItem::setPixmap(QPixmap *pixmap)
{
    m_pixmap = pixmap;
    CalcDimensions();
}

bool UIListBtnTypeItem::checkable() const
{
    return m_checkable;
}

UIListBtnTypeItem::CheckState UIListBtnTypeItem::state() const
{
    return m_state;
}

UIListBtnType* UIListBtnTypeItem::parent() const
{
    return m_parent;
}

void UIListBtnTypeItem::setChecked(UIListBtnTypeItem::CheckState state)
{
    if (!m_checkable)
        return;
    m_state = state;
}

void UIListBtnTypeItem::setCheckable(bool flag)
{
    m_checkable = flag;
    CalcDimensions();
}

void UIListBtnTypeItem::setDrawArrow(bool flag)
{
    m_showArrow = flag;
    CalcDimensions();
}

void UIListBtnTypeItem::setData(void *data)
{
    m_data = data;
}

void *UIListBtnTypeItem::getData()
{
    return m_data;
}

void UIListBtnTypeItem::setOverrideInactive(bool flag)
{
    m_overrideInactive = flag;
}

bool UIListBtnTypeItem::getOverrideInactive(void)
{
    return m_overrideInactive;
}

bool UIListBtnTypeItem::moveUpDown(bool flag)
{
    return m_parent->MoveItemUpDown(this, flag);
}

void UIListBtnTypeItem::paint(QPainter *p, fontProp *font, int x, int y, bool active_on)
{
    if (this == m_parent->m_selItem)
    {
        if (m_parent->m_active && !m_overrideInactive && active_on)
        {
            p->drawPixmap(x, y, m_parent->m_itemSelActPix);
        }
        else
        {
            if (active_on)
            {
                p->drawPixmap(x, y, m_parent->m_itemSelInactPix);
            }
            else
            {
                p->drawPixmap(x, y, m_parent->m_itemRegPix);
            }
        }

        if (m_parent->m_showArrow || m_showArrow)
        {
            QRect ar(m_arrowRect);
            ar.translate(x,y);
            p->drawPixmap(ar, m_parent->m_arrowPix);
        }
    }
    else
    {

        p->drawPixmap(x, y, m_parent->m_itemRegPix);
        /*

            Don't understand this
                    - thor

        if (m_parent->m_active && !m_overrideInactive)
        {
            p->drawPixmap(x, y, m_parent->m_itemRegPix);
        }
        else
        {
            p->drawPixmap(x, y, m_parent->m_itemRegPix);
        }
        */
    }

    if (m_checkable)
    {
        QRect cr(m_checkRect);
        cr.translate(x, y);

        if (m_state == HalfChecked)
            p->drawPixmap(cr, m_parent->m_checkHalfPix);
        else if (m_state == FullChecked)
            p->drawPixmap(cr, m_parent->m_checkFullPix);
        else
            p->drawPixmap(cr, m_parent->m_checkNonePix);
    }

    if (m_pixmap)
    {
        QRect pr(m_pixmapRect);
        pr.translate(x, y);
        p->drawPixmap(pr, *m_pixmap);
    }

    QRect tr(m_textRect);
    tr.translate(x,y);
    QString text = m_parent->cutDown(m_text, &(font->face), false,
                                     tr.width(), tr.height());
    p->drawText(tr, m_justify, text);
}

