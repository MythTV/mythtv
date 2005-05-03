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

#include "mythcontext.h"

#include "uilistbtntype.h"

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

    listLevels.setAutoDelete(true);

    m_active = NULL;
    m_inactive = NULL;

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
        listLevels.clear();
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
            curlevelarea.moveBy(m_totalarea.x(), m_totalarea.y());
            curlevelarea.moveBy((m_levelsize.width() + m_levelspacing) * i, 0);

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

            listLevels.append(newlevel);
        }
    }
}

UIListGenericTree *UIListTreeType::GetCurrentPosition(void)
{
    return currentpos;
}

void UIListTreeType::Draw(QPainter *p, int order, int blah)
{
    if (hidden)
    {
        return;
    }
    QPtrListIterator<UIListBtnType> it(listLevels);
    UIListBtnType *child;

    int maxx = 0;
    while ((child = it.current()) != 0)
    {
        if (child->IsVisible())
        {
            maxx = child->GetArea().right();
        }
        ++it;
    }

    it.toFirst();
    while ((child = it.current()) != 0)
    {
        if (!child->IsVisible())
        {
            break;
        }

        int offset = 0;
        if (maxx > m_totalarea.right())
        {
            offset = -1 * (maxx - m_totalarea.right());
        }

        child->SetDrawOffset(offset);

        if (child->GetArea().right() + offset > m_totalarea.left())
        {
            child->Draw(p, order, blah, list_tree_active);
        }
        ++it;
    }
}

void UIListTreeType::DrawRegion(QPainter *p, QRect &area, int order, int blah)
{
    QPtrListIterator<UIListBtnType> it(listLevels);
    UIListBtnType *child;

    int maxx = 0;
    while ((child = it.current()) != 0)
    {
        if (child->IsVisible())
            maxx = child->GetArea().right();
        ++it;
    }

    it.toFirst();
    while ((child = it.current()) != 0)
    {
        if (!child->IsVisible())
            break;

        int offset = 0;
        if (maxx > m_totalarea.right())
            offset = -1 * (maxx - m_totalarea.right());
        child->SetDrawOffset(offset);

        QRect drawRect = child->GetArea();
        drawRect.moveBy(offset, 0);
        drawRect.moveBy(m_parent->GetAreaRect().x(), 
                        m_parent->GetAreaRect().y());

        if (child->GetArea().right() + offset > m_totalarea.left() &&
            drawRect == area)
        {
            child->SetDrawOffset(0 - child->GetArea().x());
            child->Draw(p, order, blah, list_tree_active);
            child->SetDrawOffset(offset);
        }

        ++it;
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
        QPtrListIterator<UIListBtnTypeItem> it = currentlevel->GetIterator();

        UIListBtnTypeItem *item;
        while ((item = it.current()))
        {
            UIListGenericTree *ui = (UIListGenericTree *)item->getData();
            ui->setActive(ui->getActive());
            ++it;
        }
    }
}

void UIListTreeType::FillLevelFromTree(UIListGenericTree *item,
                                       UIListBtnType *list)
{
    if (!item || !list)
        return;

    ClearLevel(list);

    QPtrList<GenericTree> *itemlist = item->getAllChildren();

    QPtrListIterator<GenericTree> it(*itemlist);
    GenericTree *child;

    while ((child = it.current()) != 0)
    {
        UIListGenericTree *uichild = (UIListGenericTree *)child;

        UIListBtnTypeItem *newitem;

        int check = uichild->getCheck();
        newitem = new UIListBtnTypeItem(list, child->getString(),
                                        uichild->getImage(), (check >= 0),
                                        (UIListBtnTypeItem::CheckState)check,
                                        (child->childCount() > 0));
        newitem->setData(uichild);

        uichild->setItem(newitem);

        if (!uichild->getActive())
            newitem->setOverrideInactive(true);

        ++it;
    }
}

UIListBtnType *UIListTreeType::GetLevel(int levelnum)
{
    if ((uint)levelnum > listLevels.count())
    {
        cerr << "OOB GetLevel call\n";
        return NULL;
    }

    return listLevels.at(levelnum);
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
    dr.moveBy(currentlevel->GetDrawOffset(), 0);
    dr.moveBy(m_parent->GetAreaRect().x(), m_parent->GetAreaRect().y());

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
    r.moveBy(m_parent->GetAreaRect().left(),
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
        }
        ++it;
    }
    
    return keep_going;
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
    if(!currentlevel)
    {
        return 0;
    }
    return (int) currentlevel->GetNumbItemsVisible();
}

//////////////////////////////////////////////////////////////////////////////

UIListBtnType::UIListBtnType(const QString& name, const QRect& area,
                             int order, bool showArrow, bool showScrollArrows)
             : UIType(name)
{
    m_order            = order;
    m_rect             = area;

    m_showArrow        = showArrow;
    m_showScrollArrows = showScrollArrows;

    m_active           = false;
    m_visible          = true;
    m_showUpArrow      = false;
    m_showDnArrow      = false;

    m_itemList.setAutoDelete(false);
    m_topItem = 0;
    m_selItem = 0;
    m_selIterator = new QPtrListIterator<UIListBtnTypeItem>(m_itemList);
    m_topIterator = new QPtrListIterator<UIListBtnTypeItem>(m_itemList);
    m_selPosition = 0;
    m_topPosition = 0;
    m_itemCount = 0;

    m_initialized     = false;
    m_clearing        = false;
    m_itemSpacing     = 0;
    m_itemMargin      = 0;
    m_itemHeight      = 0;
    m_itemsVisible    = 0;
    m_fontActive      = 0;
    m_fontInactive    = 0;

    m_xdrawoffset     = 0;

    SetItemRegColor(Qt::black,QColor(80,80,80),100);
    SetItemSelColor(QColor(82,202,56),QColor(52,152,56),255);
}

UIListBtnType::~UIListBtnType()
{    
    Reset();
    delete m_topIterator;
    delete m_selIterator;
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

    UIListBtnTypeItem* item = 0;
    for (item = m_itemList.first(); item; item = m_itemList.next()) {
        delete item;
    }

    m_clearing = false;
    m_itemList.clear();
    
    m_topItem     = 0;
    m_selItem     = 0;
    m_selPosition = 0;
    m_topPosition = 0;
    m_itemCount   = 0;
    m_selIterator->toFirst(); 
    m_topIterator->toFirst();

    m_showUpArrow = false;
    m_showDnArrow = false;
}

void UIListBtnType::InsertItem(UIListBtnTypeItem *item)
{
    UIListBtnTypeItem* lastItem = m_itemList.last();
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
        m_selIterator->toFirst();
        m_topIterator->toFirst();
        m_selPosition = m_topPosition = 0;
        emit itemSelected(item);
    }
}

void UIListBtnType::RemoveItem(UIListBtnTypeItem *item)
{
    if (m_clearing)
        return;
    
    if (m_itemList.findRef(item) == -1)
        return;

    if (item == m_topItem)
    {
        if (m_topItem != m_itemList.last())
        {
            ++(*m_topIterator);
            ++m_topPosition;
            m_topItem = m_topIterator->current();
        }
        else if (m_topItem != m_itemList.first())
        {
            --(*m_topIterator);
            --m_topPosition;
            m_topItem = m_topIterator->current();
        }
        else
        {
            m_topItem = NULL;
            m_topPosition = 0;
            m_topIterator->toFirst();
        }
    }

    if (item == m_selItem)
    {
        if (m_selItem != m_itemList.last())
        {
            ++(*m_selIterator);
            ++m_selPosition;
            m_selItem = m_selIterator->current();
        }
        else if (m_selItem != m_itemList.first())
        {
            --(*m_selIterator);
            --m_selPosition;
            m_selItem = m_selIterator->current();
        }
        else
        {
            m_selItem = NULL;
            m_selPosition = 0;
            m_selIterator->toFirst();
        }
    }

    m_itemList.remove(item);
    m_itemCount--;

    if (m_topItem != m_itemList.first())
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
    m_selIterator->toFirst();
    UIListBtnTypeItem *cur;
    bool found = false;
    m_selPosition = 0;
    while ((cur = m_selIterator->current()) != 0)
    {
        if (cur == item)
        {
            found = true;
            break;
        }

        ++(*m_selIterator);
        ++m_selPosition;
    }

    if (!found)
    {
        m_selIterator->toFirst();
        m_selPosition = 0;
    }

    m_itemCount = m_selPosition;
    m_topItem = item;
    m_selItem = item;
    (*m_topIterator) = (*m_selIterator);

    if (m_showScrollArrows && m_itemCount > (int)m_itemsVisible)
        m_showDnArrow = true;
    else
        m_showDnArrow = false;

    emit itemSelected(m_selItem);
}

void UIListBtnType::SetItemCurrent(int current)
{
    UIListBtnTypeItem* item = m_itemList.at(current);
    if (!item)
        item = m_itemList.first();

    SetItemCurrent(item);
}

UIListBtnTypeItem* UIListBtnType::GetItemCurrent()
{
    return m_selItem;
}

UIListBtnTypeItem* UIListBtnType::GetItemFirst()
{
    return m_itemList.first();    
}

UIListBtnTypeItem* UIListBtnType::GetItemNext(UIListBtnTypeItem *item)
{
    if (m_itemList.findRef(item) == -1)
        return 0;

    return m_itemList.next();
}

int UIListBtnType::GetCount()
{
    return m_itemCount;
}

QPtrListIterator<UIListBtnTypeItem> UIListBtnType::GetIterator(void)
{
    return QPtrListIterator<UIListBtnTypeItem>(m_itemList);
}

UIListBtnTypeItem* UIListBtnType::GetItemAt(int pos)
{
    return m_itemList.at(pos);    
}

int UIListBtnType::GetItemPos(UIListBtnTypeItem* item)
{
    return m_itemList.findRef(item);    
}

void UIListBtnType::MoveUp(MovementUnit unit)
{
    int pos = m_selPosition;
    if (pos == -1)
        return;

    switch (unit)
    {
        case MoveItem:
            if (!m_selIterator->atFirst())
            {
                --(*m_selIterator);
                --m_selPosition;
            }
            break;
        case MovePage:
            if (pos > (int)m_itemsVisible)
            {
                for (int i = 0; i < (int)m_itemsVisible; i++)
                {
                    --(*m_selIterator);
                    --m_selPosition;
                }
                break;
            }
            // fall through
        case MoveMax:
            m_selIterator->toFirst();
            m_selPosition = 0;
            break;
    }

    if (!m_selIterator->current())
        return;

    m_selItem = m_selIterator->current();

    if (m_selPosition <= m_topPosition)
    {
        m_topItem = m_selItem;
        (*m_topIterator) = (*m_selIterator);
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
    int pos = m_selPosition;
    if (pos == -1)
        return;

    if (pos > count)
    {
        for (int i = 0; i < count; i++)
        {
            --(*m_selIterator);
            --m_selPosition;
        }
    }

    if (!m_selIterator->current())
        return;

    m_selItem = m_selIterator->current();

    if (m_selPosition <= m_topPosition)
    {
        m_topItem = m_selItem;
        (*m_topIterator) = (*m_selIterator);
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
    int pos = m_selPosition;
    if (pos == -1)
        return;

    switch (unit)
    {
        case MoveItem:
            if (!m_selIterator->atLast())
            {
                ++(*m_selIterator);
                ++m_selPosition;
            }
            break;
        case MovePage:
            if ((pos + (int)m_itemsVisible) < m_itemCount - 1)
            {
                for (int i = 0; i < (int)m_itemsVisible; i++)
                {
                    ++(*m_selIterator);
                    ++m_selPosition;
                }
                break;
            }
            // fall through
        case MoveMax:
            m_selIterator->toLast();
            m_selPosition = m_itemCount - 1;
            break;
    }

    if (!m_selIterator->current()) 
        return;

    m_selItem = m_selIterator->current();

    while (m_topPosition + (int)m_itemsVisible < m_selPosition + 1) 
    {
        ++(*m_topIterator);
        ++m_topPosition;
    }
   
    m_topItem = m_topIterator->current();

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
    int pos = m_selPosition;
    if (pos == -1)
        return;

    if ((pos + count) < m_itemCount - 1)
    {
        for (int i = 0; i < count; i++)
        {
             ++(*m_selIterator);
             ++m_selPosition;
        }
    }

    if (!m_selIterator->current()) 
        return;

    m_selItem = m_selIterator->current();

    while (m_topPosition + (int)m_itemsVisible < m_selPosition + 1) 
    {
        ++(*m_topIterator);
        ++m_topPosition;
    }
   
    m_topItem = m_topIterator->current();

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
    if (m_selPosition < 0)
    {
        return false;
    }

    if (!m_selIterator->toFirst())
    {
        return false;
    }
    m_selPosition = 0;
    
    bool found_it = false;
    while(m_selIterator->current())
    {
        if (m_selIterator->current()->text() == position_name)
        {
            found_it = true;
            break;
        }
        ++(*m_selIterator);
        ++m_selPosition;
    }

    if (!found_it)
    {
        m_selPosition = -1;
        return false;
    }

    m_selItem = m_selIterator->current();

    while (m_topPosition + (int)m_itemsVisible < m_selPosition + 1) 
    {
        ++(*m_topIterator);
        ++m_topPosition;
    }
   
    m_topItem = m_topIterator->current();

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
    if (item != m_selItem)
    {
        cerr << "Can't move non-selected item\n";
        return false;
    }

    if (item == m_itemList.getFirst() && flag)
        return false;
    if (item == m_itemList.getLast() && !flag)
        return false;

    int oldpos = m_selPosition;
    int insertat = 0;
    bool dolast = false;

    if (flag)
    {
        insertat = m_selPosition - 1;
        if (item == m_itemList.getLast())
            dolast = true;
        else
            ++m_selPosition;

        if (item == m_topItem)
            ++m_topPosition;
    }
    else
        insertat = m_selPosition + 1;

    if (m_itemList.current() == item)
    {
        m_itemList.take();
        //  cout << "speedy\n";
    }
    else
        m_itemList.take(oldpos);

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


void UIListBtnType::Draw(QPainter *p, int order, int blah)
{
    //
    //  Just call the other Draw() function. Tried to accomplish the same
    //  goal with default parameters, but that broke MythGallery (?)
    //

    Draw(p, order, blah, true);
}


void UIListBtnType::Draw(QPainter *p, int order, int, bool active_on)
{
    if (!m_visible)
        return;

    if (!m_initialized)
        Init();

    if (m_order != order)
        return;

    fontProp* font = m_active ? m_fontActive : m_fontInactive;
    
    if (!active_on)
    {
        font = m_fontInactive;
    }
    
    p->setFont(font->face);
    p->setPen(font->color);

    int x = m_rect.x() + m_xdrawoffset;

    int y = m_rect.y();
    QPtrListIterator<UIListBtnTypeItem> it = (*m_topIterator);
    while (it.current() && 
           (y - m_rect.y()) <= (m_contentsRect.height() - m_itemHeight)) 
    {
        if (active_on && it.current()->getOverrideInactive())
        {
            font = m_fontInactive;
            p->setFont(font->face);
            p->setPen(font->color);
            it.current()->paint(p, font, x, y, active_on);
            font = m_active ? m_fontActive : m_fontInactive;;
            p->setFont(font->face);
            p->setPen(font->color);
        }
        else
        {
            it.current()->paint(p, font, x, y, active_on);
        }

        y += m_itemHeight + m_itemSpacing;

        ++it;
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
    QSize sz1 = fm.size(Qt::SingleLine, "XXXXX");
    fm = QFontMetrics(m_fontInactive->face);
    QSize sz2 = fm.size(Qt::SingleLine, "XXXXX");
    m_itemHeight = QMAX(sz1.height(), sz2.height()) + (int)(2 * m_itemMargin);

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

    QImage img(m_rect.width(), m_itemHeight, 32);
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

        m_itemRegPix = QPixmap(img);
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
        p.setPen(black);
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

        m_itemSelInactPix = QPixmap(img);
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
        p.setPen(black);
        p.drawLine(0, 0, 0, img.height() - 1);
        p.drawLine(0, 0, img.width() - 1, 0);
        p.drawLine(0, img.height() - 1, img.width() - 1, img.height() - 1);
        p.drawLine(img.width() - 1, 0, img.width() - 1, img.height() - 1);
        p.end();

        img.setAlphaBuffer(false);
        
        m_itemSelActPix = QPixmap(img);
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
        p.setPen(black);
        p.drawLine(0, 0, 0, img.height() - 1);
        p.drawLine(0, 0, img.width() - 1, 0);
        p.drawLine(0, img.height() - 1, img.width() - 1, img.height() - 1);
        p.drawLine(img.width() - 1, 0, img.width() - 1, img.height() - 1);
        p.end();

    }

    if (m_itemList.count() > m_itemsVisible && m_showScrollArrows)
        m_showDnArrow = true;
    else
        m_showDnArrow = false;

    m_initialized = true;
}

void UIListBtnType::LoadPixmap(QPixmap& pix, const QString& fileName)
{
    QString file = "lb-" + fileName + ".png";
    
    QPixmap *p = gContext->LoadScalePixmap(file);
    if (p) 
    {
        pix = *p;
        delete p;
    }
}

//////////////////////////////////////////////////////////////////////////////

UIListBtnTypeItem::UIListBtnTypeItem(UIListBtnType* lbtype, const QString& text,
                                     QPixmap *pixmap, bool checkable,
                                     CheckState state, bool showArrow)
{
    m_parent    = lbtype;
    m_text      = text;
    m_pixmap    = pixmap;
    m_checkable = checkable;
    m_state     = state;
    m_showArrow = showArrow;
    m_data      = 0;
    m_overrideInactive = false;

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
            ar.moveBy(x,y);
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
        cr.moveBy(x, y);
        
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
        pr.moveBy(x, y);
        p->drawPixmap(pr, *m_pixmap);
    }

    QRect tr(m_textRect);
    tr.moveBy(x,y);
    QString text = m_parent->cutDown(m_text, &(font->face), false,
                                     tr.width(), tr.height());
    p->drawText(tr, Qt::AlignLeft|Qt::AlignVCenter, text);    
}

