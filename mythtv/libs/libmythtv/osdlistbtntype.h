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

#ifndef OSDLISTBTNTYPE_H
#define OSDLISTBTNTYPE_H

#include "osdtypes.h"
#include "ttfont.h"
#include <qcolor.h>
#include <qptrlist.h>

class OSDListBtnTypeItem;

class OSDListBtnType : public OSDType
{
    Q_OBJECT

  public:

    OSDListBtnType(const QString& name, const QRect& area, 
                   float wmult, float hmult,
                   bool showArrow = true, bool showScrollArrows = false);
    ~OSDListBtnType();

    void  SetFontActive(TTFFont *font);
    void  SetFontInactive(TTFFont *font);
    void  SetSpacing(int spacing);
    void  SetMargin(int margin);
    void  SetItemRegColor(const QColor& beg, const QColor& end, uint alpha);
    void  SetItemSelColor(const QColor& beg, const QColor& end, uint alpha);
    
    void  Draw(OSDSurface *surface, int fade, int maxfade, int xoff, int yoff);

    void  SetActive(bool active);
    void  Reset();

    void  SetItemCurrent(OSDListBtnTypeItem* item);
    void  SetItemCurrent(int pos);
    OSDListBtnTypeItem* GetItemCurrent();
    OSDListBtnTypeItem* GetItemFirst();
    OSDListBtnTypeItem* GetItemNext(OSDListBtnTypeItem *item);
    OSDListBtnTypeItem* GetItemAt(int pos);
    int   GetItemPos(OSDListBtnTypeItem* item);
    int   GetCount();

    void  MoveDown();
    void  MoveUp();

  private:

    void  Init();
    void  LoadPixmap(OSDTypeImage& pix, const QString& fileName);

    void  InsertItem(OSDListBtnTypeItem *item);
    void  RemoveItem(OSDListBtnTypeItem *item);

    int   m_order;
    QRect m_rect;
    QRect m_contentsRect;
    QRect m_arrowsRect;

    float m_wmult;
    float m_hmult;

    int   m_itemHeight;
    int   m_itemSpacing;
    int   m_itemMargin;
    uint  m_itemsVisible;

    bool  m_active;
    bool  m_showScrollArrows;
    bool  m_showArrow;
    bool  m_showUpArrow;
    bool  m_showDnArrow;

    OSDTypeImage   m_itemRegPix;
    OSDTypeImage   m_itemSelActPix;
    OSDTypeImage   m_itemSelInactPix;
    OSDTypeImage   m_upArrowRegPix;
    OSDTypeImage   m_dnArrowRegPix;
    OSDTypeImage   m_upArrowActPix;
    OSDTypeImage   m_dnArrowActPix;
    OSDTypeImage   m_arrowPix;
    OSDTypeImage   m_checkNonePix;
    OSDTypeImage   m_checkHalfPix;
    OSDTypeImage   m_checkFullPix;

    QColor    m_itemRegBeg;
    QColor    m_itemRegEnd;
    QColor    m_itemSelBeg;
    QColor    m_itemSelEnd;
    uint      m_itemRegAlpha;
    uint      m_itemSelAlpha;

    TTFFont* m_fontActive;
    TTFFont* m_fontInactive;

    bool      m_initialized;
    bool      m_clearing;

    OSDListBtnTypeItem* m_topItem;
    OSDListBtnTypeItem* m_selItem;
    QPtrList<OSDListBtnTypeItem> m_itemList;

    friend class OSDListBtnTypeItem;
    
  signals:

    void itemSelected(OSDListBtnTypeItem* item);
};

class OSDListBtnTypeItem
{

  public:

    enum CheckState {
        NotChecked=0,
        HalfChecked,
        FullChecked
    };

    OSDListBtnTypeItem(OSDListBtnType* lbtype, const QString& text,
                       OSDTypeImage *pixmap = 0, bool checkable = false,
                       CheckState state = NotChecked);
    ~OSDListBtnTypeItem();

    OSDListBtnType*   parent() const;
    QString          text() const;
    const OSDTypeImage*   pixmap() const;
    bool             checkable() const;
    CheckState       state() const;

    void  setChecked(CheckState state);
    void  setData(void *data);
    void* getData();
    
    void  paint(OSDSurface *surface, TTFFont *font, int fade, int maxfade, 
                int x, int y);
    
  protected:

    OSDListBtnType *m_parent;
    QString        m_text;
    OSDTypeImage  *m_pixmap;
    bool           m_checkable;
    CheckState     m_state;
    void          *m_data;

    QRect          m_checkRect;
    QRect          m_pixmapRect;
    QRect          m_textRect;
    QRect          m_arrowRect;
    
    friend class OSDListBtnType;
};


#endif /* OSDLISTBTNTYPE_H */
