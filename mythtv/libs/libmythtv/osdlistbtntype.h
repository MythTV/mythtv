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
#include <qevent.h>
#include <qmutex.h>
#include "generictree.h"

class OSDListBtnTypeItem;
class OSDListBtnType;

class OSDGenericTree : public GenericTree
{
  public:
    // This class will _not_ delete the image it's given, if any.
    OSDGenericTree(OSDGenericTree *parent, const QString &name, 
                   const QString &action = "", int check = -1, 
                   OSDTypeImage *image = NULL, QString group = "");

    OSDTypeImage *getImage(void) { return m_image; }
    QString getAction(void) { return m_action; }
    int getCheckable(void) { return m_checkable; }
    QString getGroup(void) { return m_group; }
    void setParentButton(OSDListBtnTypeItem *button)
                         { m_parentButton = button; };
    OSDListBtnTypeItem *getParentButton(void) { return m_parentButton; };

  private:
    OSDTypeImage *m_image;
    QString m_action;
    int m_checkable;
    QString m_group;
    OSDListBtnTypeItem *m_parentButton;
};

// Will _not_ delete the GenericTree that it's given.
class OSDListTreeType : public OSDType
{
    Q_OBJECT
  public:
    OSDListTreeType(const QString &name, const QRect &area, 
                    const QRect &levelsize, int levelspacing,
                    float wmult, float hmult);

    void Reinit(float wchange, float hchange, float wmult, float hmult);
    void SetGroupCheckState(QString group, int newState = 0);

    void SetFontActive(TTFFont *font);
    void SetFontInactive(TTFFont *font);
    void SetSpacing(int spacing);
    void SetMargin(int margin);
    void SetItemRegColor(const QColor& beg, const QColor& end, uint alpha);
    void SetItemSelColor(const QColor& beg, const QColor& end, uint alpha);

    void SetAsTree(OSDGenericTree *toplevel);

    OSDGenericTree *GetCurrentPosition(void);
 
    bool HandleKeypress(QKeyEvent *e);

    void Draw(OSDSurface *surface, int fade, int maxfade, int xoff, int yoff);

    bool IsVisible(void) { return m_visible; }
    void SetVisible(bool visible) { m_visible = visible; }

  signals:
    void itemSelected(OSDListTreeType *parent, OSDGenericTree *item);
    void itemEntered(OSDListTreeType *parent, OSDGenericTree *item);

  private:
    void FillLevelFromTree(OSDGenericTree *item, OSDListBtnType *list);
    OSDListBtnType *GetLevel(int levelnum);
    void SetCurrentPosition(void);

    int levels;
    int curlevel;

    OSDGenericTree *treetop;
    OSDGenericTree *currentpos;

    QPtrList<OSDListBtnType> listLevels;

    OSDListBtnType *currentlevel;

    TTFFont *m_active;
    TTFFont *m_inactive;

    QColor    m_itemRegBeg;
    QColor    m_itemRegEnd;
    QColor    m_itemSelBeg;
    QColor    m_itemSelEnd;
    uint      m_itemRegAlpha;
    uint      m_itemSelAlpha;
   
    int m_spacing;
    int m_margin;

    QRect m_totalarea;
    QRect m_levelsize;
    int m_levelspacing;

    float m_wmult;
    float m_hmult;

    bool m_visible;
    bool m_arrowAccel;
};
 
class OSDListBtnType : public OSDType
{
    Q_OBJECT
  public:
    OSDListBtnType(const QString &name, const QRect& area,
                   float wmult, float hmult,
                   bool showScrollArrows = false);
    ~OSDListBtnType();

    void  Reinit(float wchange, float hchange, float wmult, float hmult);
    void  SetGroupCheckState(QString group, int newState = 0);

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

    bool  IsVisible() { return m_visible; }
    void  SetVisible(bool vis) { m_visible = vis; }

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

    QMutex    m_update;

    bool      m_visible;

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
                       bool showArrow = false, CheckState state = NotChecked);
    ~OSDListBtnTypeItem();

    OSDListBtnType*   parent() const;
    QString           text() const;
    const OSDTypeImage*   pixmap() const;
    bool              checkable() const;
    CheckState        state() const;

    void  setChecked(CheckState state);
    void  setData(void *data);
    void* getData();
    void  setGroup(QString group) { m_group = group; };

    void  Reinit(float wchange, float hchange, float wmult, float hmult);
    
    void  paint(OSDSurface *surface, TTFFont *font, int fade, int maxfade, 
                int x, int y);
    
  protected:

    OSDListBtnType *m_parent;
    QString        m_text;
    OSDTypeImage  *m_pixmap;
    bool           m_checkable;
    CheckState     m_state;
    void          *m_data;
    QString        m_group;

    QRect          m_checkRect;
    QRect          m_pixmapRect;
    QRect          m_textRect;
    QRect          m_arrowRect;

    bool           m_showArrow;    

    friend class OSDListBtnType;
};


#endif /* OSDLISTBTNTYPE_H */
