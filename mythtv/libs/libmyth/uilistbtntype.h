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

class UIListBtnType;
class UIListBtnTypeItem;

class UIListGenericTree : public GenericTree
{
  public:
    // will _not_ delete the image it's given, if any.
    UIListGenericTree(UIListGenericTree *parent, const QString &name,
                      const QString &action = "", int check = -1,
                      QPixmap *image = NULL);
    virtual ~UIListGenericTree();

    QPixmap *getImage(void) { return m_image; }
    QString getAction(void) { return m_action; }
    int getCheck(void) { return m_check; }
    virtual void setCheck(int flag);

    void setItem(UIListBtnTypeItem *item) { m_physitem = item; }
    UIListBtnTypeItem *getItem(void) { return m_physitem; }

    void setText(const QString &text);
    void setPixmap(QPixmap *pixmap);
    void setDrawArrow(bool flag);

    void RemoveFromParent(void);

    void setActive(bool flag);
    bool getActive(void);

    bool movePositionUpDown(bool flag);
    

  protected:
    QPixmap *m_image;
    QString m_action;
    int m_check;

    UIListBtnTypeItem *m_physitem;

    bool m_active;
};

class UIListTreeType : public UIType
{
    Q_OBJECT
  public:
    UIListTreeType(const QString &name, const QRect &area, 
                   const QRect &levelsize, int levelspacing, int order);
   ~UIListTreeType();

    void SetFontActive(fontProp *font);
    void SetFontInactive(fontProp *font);
    void SetSpacing(int spacing);
    void SetMargin(int margin);
    void SetItemRegColor(const QColor& beg, const QColor& end, uint alpha);
    void SetItemSelColor(const QColor& beg, const QColor& end, uint alpha);

    void SetTree(UIListGenericTree *toplevel);

    UIListGenericTree *GetCurrentPosition(void);

    void Draw(QPainter *p, int order, int context);
    void DrawRegion(QPainter *p, QRect &area, int order, int context);

    void Redraw(void);
    void RedrawCurrent(void);

    void RefreshCurrentLevel(void);

    enum MovementUnit { MoveItem, MovePage, MoveMax };
    void MoveDown(MovementUnit unit = MoveItem);
    void MoveDown(int count);
    void MoveUp(MovementUnit unit = MoveItem);
    void MoveUp(int count);
    void MoveLeft(bool do_refresh = true);
    bool MoveRight(bool do_refresh = true);
    void calculateScreenArea();
    void GoHome();
    void select();
    QStringList getRouteToCurrent();
    bool tryToSetCurrent(QStringList route);
    int  getDepth();
    void setActive(bool x);
    bool isActive(){ return list_tree_active; } 
    void enter();
    void moveAwayFrom(UIListGenericTree *node);
    int  getNumbItemsVisible();
    bool incSearchStart();
    bool incSearchNext();

  signals:
    
    void selected(UIListGenericTree *item);
    void itemSelected(UIListTreeType *parent, UIListGenericTree *item);
    void itemEntered(UIListTreeType *parent, UIListGenericTree *item);

  public slots:
    bool takeFocus();
    void looseFocus();
      
  private:
    void FillLevelFromTree(UIListGenericTree *item, UIListBtnType *list);
    void ClearLevel(UIListBtnType *list);
    UIListBtnType *GetLevel(int levelnum);
    void SetCurrentPosition(void);
    void CreateLevel(int level);

    int levels;
    int curlevel;

    UIListGenericTree *treetop;
    UIListGenericTree *currentpos;

    QPtrList<UIListBtnType> listLevels;

    UIListBtnType *currentlevel;

    fontProp *m_active;
    fontProp *m_inactive;

    QColor m_itemRegBeg;
    QColor m_itemRegEnd;
    QColor m_itemSelBeg;
    QColor m_itemSelEnd;
    uint m_itemRegAlpha;
    uint m_itemSelAlpha;

    int m_spacing;
    int m_margin;

    QRect m_totalarea;
    QRect m_levelsize;
    int m_levelspacing;

    bool    list_tree_active;
};

class UIListBtnType : public UIType
{
    Q_OBJECT
  public:
    UIListBtnType(const QString& name, const QRect& area, int order,
                  bool showArrow=true, bool showScrollArrows=false);
    ~UIListBtnType();

    void  SetParentListTree(UIListTreeType* parent) { m_parentListTree = parent;}
    void  SetFontActive(fontProp *font);
    void  SetFontInactive(fontProp *font);
    void  SetSpacing(int spacing);
    void  SetMargin(int margin);
    void  SetItemRegColor(const QColor& beg, const QColor& end, uint alpha);
    void  SetItemSelColor(const QColor& beg, const QColor& end, uint alpha);
    
    void  Draw(QPainter *p, int order, int context);
    void  Draw(QPainter *p, int order, int context, bool active_on);
    void  SetActive(bool active);
    void  Reset();
    void  calculateScreenArea();
     
    void  SetItemCurrent(UIListBtnTypeItem* item);
    void  SetItemCurrent(int pos);
    UIListBtnTypeItem* GetItemCurrent();
    UIListBtnTypeItem* GetItemFirst();
    UIListBtnTypeItem* GetItemNext(UIListBtnTypeItem *item);
    UIListBtnTypeItem* GetItemAt(int pos);

    bool  MoveItemUpDown(UIListBtnTypeItem *item, bool flag);

    QPtrListIterator<UIListBtnTypeItem> GetIterator();

    int   GetItemPos(UIListBtnTypeItem* item);
    int   GetCount();

    enum MovementUnit { MoveItem, MovePage, MoveMax };
    void  MoveDown(MovementUnit unit=MoveItem);
    void  MoveDown(int count);
    void  MoveUp(MovementUnit unit=MoveItem);
    void  MoveUp(int count);    
    bool  MoveToNamedPosition(const QString &position_name);

    bool  incSearchStart();
    bool  incSearchNext();

    bool  IsVisible() { return m_visible; }
    void  SetVisible(bool vis) { m_visible = vis; }

    void  SetDrawOffset(int x) { m_xdrawoffset = x; }
    int   GetDrawOffset(void) { return m_xdrawoffset; }

    QRect GetArea(void) { return m_rect; }
    uint   GetNumbItemsVisible(){ return m_itemsVisible; }
  
  public slots:
    bool takeFocus();
    void looseFocus();

  private:
    void  Init();
    void  LoadPixmap(QPixmap& pix, const QString& fileName);

    void  InsertItem(UIListBtnTypeItem *item);
    void  RemoveItem(UIListBtnTypeItem *item);

    UIListTreeType *m_parentListTree;

    QRect m_rect;
    QRect m_contentsRect;
    QRect m_arrowsRect;

    int   m_itemHeight;
    int   m_itemSpacing;
    int   m_itemMargin;
    uint  m_itemsVisible;

    bool  m_active;
    bool  m_visible;
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

    QPtrListIterator<UIListBtnTypeItem> *m_topIterator;
    QPtrListIterator<UIListBtnTypeItem> *m_selIterator;

    int       m_selPosition;
    int       m_topPosition;
    int       m_itemCount;

    QPtrList<UIListBtnTypeItem> m_itemList;

    friend class UIListBtnTypeItem;
  
    int m_xdrawoffset;

    QString  m_incSearch;
    bool     m_bIncSearchContains;

  signals:

    void itemSelected(UIListBtnTypeItem* item);
};

class UIListBtnTypeItem
{

  public:

    enum CheckState {
        CantCheck=-1,
        NotChecked=0,
        HalfChecked,
        FullChecked
    };

    UIListBtnTypeItem(UIListBtnType* lbtype, const QString& text,
                      QPixmap *pixmap = 0, bool checkable = false,
                      CheckState state = CantCheck, bool showArrow = false);
    ~UIListBtnTypeItem();

    UIListBtnType *parent() const;

    void setText(const QString &text);
    QString text() const;

    void setPixmap(QPixmap *pixmap);
    const QPixmap *pixmap() const;

    bool checkable() const;
    void setCheckable(bool flag);

    CheckState state() const;
    void setChecked(CheckState state);

    void setDrawArrow(bool flag);
    bool getDrawArrow(void) { return m_showArrow; }
     
    void setData(void *data);
    void *getData();
    
    void setOverrideInactive(bool flag);
    bool getOverrideInactive(void);

    bool moveUpDown(bool flag);
    
    void paint(QPainter *p, fontProp *font, int x, int y, bool active_on);
    
  protected:
    void  CalcDimensions(void);

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
    
    bool           m_showArrow;

    bool           m_overrideInactive;

    friend class UIListBtnType;
};



#endif /* UILISTBTNTYPE_H */
