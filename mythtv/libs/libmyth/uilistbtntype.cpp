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


UIListBtnType::UIListBtnType(const QString& name, const QRect& area,
                             int order, bool showArrow, bool showScrollArrows)
    : UIType(name)
{
    m_order            = order;
    m_rect             = area;

    m_showArrow        = showArrow;
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

    UIListBtnTypeItem* item = 0;
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

void UIListBtnType::InsertItem(UIListBtnTypeItem *item)
{
    UIListBtnTypeItem* lastItem = m_itemList.last();
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

void UIListBtnType::RemoveItem(UIListBtnTypeItem *item)
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

void UIListBtnType::SetItemCurrent(UIListBtnTypeItem* item)
{
    if (m_itemList.find(item) == -1)
        return;

    m_topItem = item;
    m_selItem = item;

    if (m_showScrollArrows && m_itemList.count() > m_itemsVisible)
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
    if (m_itemList.find(item) == -1)
        return 0;

    return m_itemList.next();
}

int UIListBtnType::GetCount()
{
    return m_itemList.count();
}

UIListBtnTypeItem* UIListBtnType::GetItemAt(int pos)
{
    return m_itemList.at(pos);    
}

int UIListBtnType::GetItemPos(UIListBtnTypeItem* item)
{
    return m_itemList.find(item);    
}

void UIListBtnType::MoveUp()
{
    if (m_itemList.find(m_selItem) == -1)
        return;

    UIListBtnTypeItem *item = m_itemList.prev();
    if (!item) 
        return;

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

void UIListBtnType::MoveDown()
{
    if (m_itemList.find(m_selItem) == -1)
        return;
    UIListBtnTypeItem *item = m_itemList.next();
    if (!item) 
        return;

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

void UIListBtnType::Draw(QPainter *p, int order, int)
{
    if (!m_initialized)
        Init();

    if (m_order != order)
        return;

    fontProp* font = m_active ? m_fontActive : m_fontInactive;
    
    p->setFont(font->face);
    p->setPen(font->color);

    int y = m_rect.y();
    m_itemList.find(m_topItem);
    UIListBtnTypeItem *it = m_itemList.current();
    while (it && (y - m_rect.y()) <= (m_contentsRect.height() - m_itemHeight)) 
    {
        it->paint(p, font, m_rect.x(), y);

        y += m_itemHeight + m_itemSpacing;

        it = m_itemList.next();
    }

    if (m_showScrollArrows) 
    {
        if (m_showUpArrow)
            p->drawPixmap(m_rect.x() + m_arrowsRect.x(),
                          m_rect.y() + m_arrowsRect.y(),
                          m_upArrowActPix);
        else
            p->drawPixmap(m_rect.x() + m_arrowsRect.x(),
                          m_rect.y() + m_arrowsRect.y(),
                          m_upArrowRegPix);
        if (m_showDnArrow)
            p->drawPixmap(m_rect.x() + m_arrowsRect.x() +
                          m_upArrowRegPix.width() + m_itemMargin,
                          m_rect.y() + m_arrowsRect.y(),
                          m_dnArrowActPix);
        else
            p->drawPixmap(m_rect.x() + m_arrowsRect.x() +
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
    
    if (m_showArrow) 
    {
        LoadPixmap(m_arrowPix, "arrow");
    }

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

UIListBtnTypeItem::UIListBtnTypeItem(UIListBtnType* lbtype, const QString& text,
                                     QPixmap *pixmap, bool checkable,
                                     UIListBtnTypeItem::CheckState state )
{
    if (!lbtype) {
        std::cerr << "UIListBtnTypeItem: trying to creating item without parent"
                  << std::endl;
        exit(-1);
    }
    
    m_parent    = lbtype;
    m_text      = text;
    m_pixmap    = pixmap;
    m_checkable = checkable;
    m_state     = state;
    m_data      = 0;

    if (!m_parent->m_initialized)
        m_parent->Init();

    int  margin    = m_parent->m_itemMargin;
    int  width     = m_parent->m_rect.width();
    int  height    = m_parent->m_itemHeight;
    bool showArrow = m_parent->m_showArrow;

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

    if (showArrow) 
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
                       (showArrow ? m_arrowRect.width() + margin : 0) -
                       (m_pixmap ? m_pixmapRect.width() + margin : 0),
                       height);

    m_parent->InsertItem(this);
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

const QPixmap* UIListBtnTypeItem::pixmap() const
{
    return m_pixmap;
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

void UIListBtnTypeItem::setData(void *data)
{
    m_data = data;    
}

void* UIListBtnTypeItem::getData()
{
    return m_data;
}

void UIListBtnTypeItem::paint(QPainter *p, fontProp *font, int x, int y)
{
    if (this == m_parent->m_selItem)
    {
        if (m_parent->m_active)
            p->drawPixmap(x, y, m_parent->m_itemSelActPix);
        else
            p->drawPixmap(x, y, m_parent->m_itemSelInactPix);

        if (m_parent->m_showArrow)
        {
            QRect ar(m_arrowRect);
            ar.moveBy(x,y);
            p->drawPixmap(ar, m_parent->m_arrowPix);
        }
    }
    else
    {
        if (m_parent->m_active)
            p->drawPixmap(x, y, m_parent->m_itemRegPix);
        else
            p->drawPixmap(x, y, m_parent->m_itemRegPix);
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

