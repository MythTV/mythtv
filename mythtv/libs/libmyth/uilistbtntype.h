/* ============================================================
 * File  : uilistbtntype.h
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

#ifndef UILISTBTNTYPE_H
#define UILISTBTNTYPE_H

#include "uitypes.h"

class UIListBtnTypeItem;

class UIListBtnType : public UIType
{
    Q_OBJECT

  public:

    UIListBtnType(const QString& name, const QRect& area, int order,
                  bool showArrow=true, bool showScrollArrows=false);
    ~UIListBtnType();

    void  SetFontActive(fontProp *font);
    void  SetFontInactive(fontProp *font);
    void  SetSpacing(int spacing);
    void  SetMargin(int margin);
    void  SetItemRegColor(const QColor& beg, const QColor& end, uint alpha);
    void  SetItemSelColor(const QColor& beg, const QColor& end, uint alpha);
    
    void  Draw(QPainter *p, int order, int);
    void  SetActive(bool active);
    void  Reset();

    void  SetItemCurrent(UIListBtnTypeItem* item);
    void  SetItemCurrent(int pos);
    UIListBtnTypeItem* GetItemCurrent();
    UIListBtnTypeItem* GetItemFirst();
    UIListBtnTypeItem* GetItemNext(UIListBtnTypeItem *item);
    UIListBtnTypeItem* GetItemAt(int pos);
    int   GetItemPos(UIListBtnTypeItem* item);
    int   GetCount();

    enum MovementUnit { MoveItem, MovePage, MoveMax };
    void  MoveDown(MovementUnit unit=MoveItem);
    void  MoveUp(MovementUnit unit=MoveItem);

  private:

    void  Init();
    void  LoadPixmap(QPixmap& pix, const QString& fileName);

    void  InsertItem(UIListBtnTypeItem *item);
    void  RemoveItem(UIListBtnTypeItem *item);

    int   m_order;
    QRect m_rect;
    QRect m_contentsRect;
    QRect m_arrowsRect;

    int   m_itemHeight;
    int   m_itemSpacing;
    int   m_itemMargin;
    uint  m_itemsVisible;

    bool  m_active;
    bool  m_showScrollArrows;
    bool  m_showArrow;
    bool  m_showUpArrow;
    bool  m_showDnArrow;

    QPixmap   m_itemRegPix;
    QPixmap   m_itemSelActPix;
    QPixmap   m_itemSelInactPix;
    QPixmap   m_upArrowRegPix;
    QPixmap   m_dnArrowRegPix;
    QPixmap   m_upArrowActPix;
    QPixmap   m_dnArrowActPix;
    QPixmap   m_arrowPix;
    QPixmap   m_checkNonePix;
    QPixmap   m_checkHalfPix;
    QPixmap   m_checkFullPix;

    QColor    m_itemRegBeg;
    QColor    m_itemRegEnd;
    QColor    m_itemSelBeg;
    QColor    m_itemSelEnd;
    uint      m_itemRegAlpha;
    uint      m_itemSelAlpha;

    fontProp* m_fontActive;
    fontProp* m_fontInactive;

    bool      m_initialized;
    bool      m_clearing;

    UIListBtnTypeItem* m_topItem;
    UIListBtnTypeItem* m_selItem;
    QPtrList<UIListBtnTypeItem> m_itemList;

    friend class UIListBtnTypeItem;
    
  signals:

    void itemSelected(UIListBtnTypeItem* item);
};

class UIListBtnTypeItem
{

  public:

    enum CheckState {
        NotChecked=0,
        HalfChecked,
        FullChecked
    };

    UIListBtnTypeItem(UIListBtnType* lbtype, const QString& text,
                      QPixmap *pixmap = 0, bool checkable = false,
                      CheckState state = NotChecked);
    ~UIListBtnTypeItem();

    UIListBtnType*   parent() const;
    QString          text() const;
    const QPixmap*   pixmap() const;
    bool             checkable() const;
    CheckState       state() const;

    void  setChecked(CheckState state);
    void  setData(void *data);
    void* getData();
    
    void  paint(QPainter *p, fontProp *font, int x, int y);

    
  protected:

    UIListBtnType *m_parent;
    QString        m_text;
    QPixmap       *m_pixmap;
    bool           m_checkable;
    CheckState     m_state;
    void          *m_data;

    QRect          m_checkRect;
    QRect          m_pixmapRect;
    QRect          m_textRect;
    QRect          m_arrowRect;
    
    friend class UIListBtnType;
};



#endif /* UILISTBTNTYPE_H */
