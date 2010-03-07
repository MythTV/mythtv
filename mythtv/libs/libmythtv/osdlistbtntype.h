// -*- Mode: c++ -*-
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

#include <vector>
using namespace std;

// Qt headers
#include <QColor>
#include <QMutex>
#include <QKeyEvent>

// MythTV headers
#include "osdtypes.h"
#include "ttfont.h"
#include "generictree.h"

class OSDListBtnType;
class OSDListBtnTypeItem;
typedef vector<OSDListBtnType*> OSDListBtnList;
typedef vector<OSDListBtnTypeItem*> OSDListBtnItemList;

/**
 *  \class OSDListTreeItemEnteredEvent
 *
 *  \brief Event dispatched from OSDListTreeType when an item is focused
 *
 *   name is the name of the item selected
 *   action is the action associated with the item
 */

class MPUBLIC OSDListTreeItemEnteredEvent : public QEvent
{
  public:
    OSDListTreeItemEnteredEvent(const QString &name, const QString &action) :
        QEvent(kEventType), m_name(name), m_action(action) { }

    QString GetName() { return m_name; }
    QString GetAction() { return m_action; }

    static Type kEventType;

  private:
    QString m_name;
    QString m_action;
};

/**
 *  \class OSDListTreeItemSelectedEvent
 *
 *  \brief Event dispatched from OSDListTreeType when an item is clicked
 *
 *   name is the name of the item selected
 *   action is the action associated with the item
 */

class MPUBLIC OSDListTreeItemSelectedEvent : public QEvent
{
  public:
    OSDListTreeItemSelectedEvent(const QString &name, const QString &action) :
        QEvent(kEventType), m_name(name), m_action(action) { }

    QString GetName() { return m_name; }
    QString GetAction() { return m_action; }

    static Type kEventType;

  private:
    QString m_name;
    QString m_action;
};

/**
 *  \class OSDListBtnItemSelectedEvent
 *
 *  \brief Event dispatched from OSDListBtnType when an item is clicked
 *
 *   name is the name of the item selected
 *   action is the action associated with the item
 */
class MPUBLIC OSDListBtnItemSelectedEvent : public QEvent
{
  public:
    OSDListBtnItemSelectedEvent(const QString &name) :
        QEvent(kEventType), m_name(name) { }

    QString GetName() { return m_name; }

    static Type kEventType;

  private:
    QString m_name;
};

class OSDGenericTree : public GenericTree
{
  public:
    // This class will _not_ delete the image it's given, if any.
    OSDGenericTree(OSDGenericTree *parent,        const QString &name,
                   const QString  &action = "",   int            check = -1,
                   OSDTypeImage   *image  = NULL, QString        group = "") :
        GenericTree(name), m_image(image),     m_action(action),
        m_group(group),    m_checkable(check), m_parentButton(NULL)
    {
        m_group = (m_group.isEmpty()) ? action : m_group;
        setSelectable(!action.isEmpty());
        if (parent)
            parent->addNode(this);
    }

    QString getAction(void)    const { return m_action;    }
    QString getGroup(void)     const { return m_group;     }
    int     getCheckable(void) const { return m_checkable; }

    OSDTypeImage       *getImage(void)        { return m_image;        }
    OSDListBtnTypeItem *getParentButton(void) { return m_parentButton; }

    void setParentButton(OSDListBtnTypeItem *button)
        { m_parentButton = button; }

  private:
    OSDTypeImage       *m_image;
    QString             m_action;
    QString             m_group;
    int                 m_checkable;
    OSDListBtnTypeItem *m_parentButton;
};

// Will _not_ delete the GenericTree that it's given.
class OSDListTreeType : public OSDType
{
  public:
    OSDListTreeType(const QString &name,      const QRect &area,
                    const QRect   &levelsize, int          levelspacing,
                    float          wmult,     float        hmult);
    ~OSDListTreeType();

    void SetListener(QObject *listener) { m_listener = listener; }

    bool IsVisible(void)          const { return m_visible;  }

    void SetFontActive(TTFFont *font)   { m_active   = font; }
    void SetFontInactive(TTFFont *font) { m_inactive = font; }

    void SetGroupCheckState(QString group, int newState = 0);
    void SetSpacing(uint spacing)
        { m_unbiasedspacing = (m_spacing = spacing) / m_wmult; }
    void SetMargin(uint margin)
        { m_unbiasedmargin  = (m_margin  = margin)  / m_wmult; }
    void SetItemRegColor(const QColor& beg, const QColor& end, uint alpha)
        { m_itemRegBeg = beg; m_itemRegEnd = end; m_itemRegAlpha = alpha; }
    void SetItemSelColor(const QColor& beg, const QColor& end, uint alpha)
        { m_itemSelBeg = beg; m_itemSelEnd = end; m_itemSelAlpha = alpha; }
    void SetVisible(bool visible) { m_visible = visible; }
    void SetAsTree(OSDGenericTree *toplevel, vector<uint> *select = NULL);

    void Reinit(float wmult, float hmult);
    bool HandleKeypress(QKeyEvent *e);
    void Draw(OSDSurface *surface, int fade, int maxfade, int xoff, int yoff);

    void SendItemSelected(OSDListTreeType *parent, OSDGenericTree *item);
    void SendItemEntered(OSDListTreeType *parent, OSDGenericTree *item);

  private:
    void FillLevelFromTree(OSDGenericTree *item, uint levelnum);
    OSDListBtnType *GetLevel(uint levelnum);
    void EnterItem(void);
    void SelectItem(void);

    OSDGenericTree *treetop;
    OSDGenericTree *currentpos;
    TTFFont        *m_active;
    TTFFont        *m_inactive;

    OSDListBtnList  listLevels;

    QColor    m_itemRegBeg;
    QColor    m_itemRegEnd;
    QColor    m_itemSelBeg;
    QColor    m_itemSelEnd;
    uint      m_itemRegAlpha;
    uint      m_itemSelAlpha;

    uint      m_spacing;
    uint      m_margin;
    int       m_levelspacing;

    QRect     m_totalarea;
    QRect     m_levelsize;

    float     m_unbiasedspacing;
    float     m_unbiasedmargin;
    QRect     m_unbiasedarea;
    QRect     m_unbiasedsize;

    float     m_wmult;
    float     m_hmult;

    int       m_depth;
    int       m_levelnum;
    bool      m_visible;
    bool      m_arrowAccel;

    QObject  *m_listener;
};

class OSDListBtnType : public OSDType
{
    friend class OSDListBtnTypeItem;

  public:
    OSDListBtnType(const QString &name, const QRect& area,
                   float wmult, float hmult,
                   bool showScrollArrows = false);
    ~OSDListBtnType();

    void SetListener(QObject *listener) { m_listener = listener; }

    // General Gets
    bool  IsVisible() const { return m_visible; }
    int   GetShiftLeftOffset() const { return -m_contentsRect.width() - 10; }

    // General Sets
    void  SetFontActive(TTFFont *font)   { m_fontActive   = font;    }
    void  SetFontInactive(TTFFont *font) { m_fontInactive = font;    }
    void  SetSpacing(int spacing)        { m_itemSpacing  = spacing; }
    void  SetMargin(int margin)          { m_itemMargin   = margin;  }
    void  SetActive(bool active)         { m_active       = active;  }
    void  SetVisible(bool vis)           { m_visible = vis; }
    void  SetItemRegColor(const QColor& beg, const QColor& end, uint alpha)
        { m_itemRegBeg = beg; m_itemRegEnd = end; m_itemRegAlpha = alpha; }
    void  SetItemSelColor(const QColor& beg, const QColor& end, uint alpha)
        { m_itemSelBeg = beg; m_itemSelEnd = end; m_itemSelAlpha = alpha; }
    void  SetGroupCheckState(QString group, int newState = 0);
    void  SetItemCurrent(const OSDListBtnTypeItem* item);
    void  SetItemCurrent(uint pos);

    // Item Gets
    int   GetCount(void) const;
    int   GetItemPos(const OSDListBtnTypeItem* item) const;
    int   GetItemCurrentPos() const;
    OSDListBtnTypeItem* GetItemCurrent(void);
    OSDListBtnTypeItem* GetItemFirst(void);
    OSDListBtnTypeItem* GetItemNext(const OSDListBtnTypeItem *item);
    OSDListBtnTypeItem* GetItemAt(int pos);

    // Item Sets/Commands
    void  MoveDown(void);
    void  MoveUp(void);
    void  MovePageDown(void);
    void  MovePageUp(void);

    // General Commands
    void  Draw(OSDSurface *surface, int fade, int maxfade, int xoff, int yoff);
    void  Reinit(float,float) {}
    void  Reset(void);

  private:
    void  Init(void);
    void  InitItem(OSDTypeImage &osdImg, uint width, uint height,
                   QColor beg, QColor end, int alpha);
    void  LoadPixmap(OSDTypeImage& pix, const QString& fileName);

    void  InsertItem(OSDListBtnTypeItem *item);
    void  RemoveItem(OSDListBtnTypeItem *item);

  private:
    int            m_order;
    QRect          m_rect;
    QRect          m_contentsRect;
    QRect          m_arrowsRect;

    float          m_wmult;
    float          m_hmult;

    int            m_itemHeight;
    int            m_itemSpacing;
    int            m_itemMargin;
    uint           m_itemsVisible;

    bool           m_active;
    bool           m_showScrollArrows;
    bool           m_showUpArrow;
    bool           m_showDnArrow;
    bool           m_initialized;
    bool           m_clearing;
    bool           m_visible;

    QColor         m_itemRegBeg;
    QColor         m_itemRegEnd;
    QColor         m_itemSelBeg;
    QColor         m_itemSelEnd;
    uint           m_itemRegAlpha;
    uint           m_itemSelAlpha;

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

    TTFFont       *m_fontActive;
    TTFFont       *m_fontInactive;

    int            m_topIndx;
    int            m_selIndx;

    OSDListBtnItemList m_itemList;

    mutable QMutex m_update;

    void SendItemSelected(OSDListBtnTypeItem* item);
    QObject * m_listener;
};

class OSDListBtnTypeItem
{
    friend class OSDListBtnType;

  public:
    enum CheckState
    {
        NotChecked  = 0,
        HalfChecked,
        FullChecked,
    };

    OSDListBtnTypeItem(OSDListBtnType* lbtype, const QString& text,
                       OSDTypeImage *pixmap = 0, bool checkable = false,
                       bool showArrow = false, CheckState state = NotChecked);
    ~OSDListBtnTypeItem();

    OSDListBtnType*     parent(void)    const { return m_parent;    }
    QString             text(void)      const { return m_text;      }
    const OSDTypeImage* pixmap(void)    const { return m_pixmap;    }
    bool                checkable(void) const { return m_checkable; }
    CheckState          state(void)     const { return m_state;     }
    QString             getGroup(void)  const { return m_group;     }
    void               *getData(void)         { return m_data;      }

    void setData(void *data)          { m_data = data; }
    void setGroup(QString grp)        { m_group = grp; }
    void setChecked(CheckState state)
        { m_state = (m_checkable) ? state : m_state; }

    void  Reinit(float,float) {}
    void  paint(OSDSurface *surface, TTFFont *font, int fade, int maxfade,
                int x, int y);

  protected:
    OSDListBtnType *m_parent;
    OSDTypeImage   *m_pixmap;
    void           *m_data;
    QString         m_text;
    QString         m_group;
    CheckState      m_state;
    bool            m_showArrow;
    bool            m_checkable;
    QRect           m_checkRect;
    QRect           m_arrowRect;
    QRect           m_pixmapRect;
    QRect           m_textRect;
};


#endif /* OSDLISTBTNTYPE_H */
