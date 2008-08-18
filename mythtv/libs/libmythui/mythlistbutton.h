#ifndef MYTHLISTBUTTON_H_
#define MYTHLISTBUTTON_H_

#include <QList>
#include <QString>
#include <QVariant>

#include "mythuitype.h"
#include "mythuiimage.h"
#include "mythuitext.h"
#include "mythuibutton.h"
#include "mythgesture.h"

class MythListButton;
class MythFontProperties;
class MythUIStateType;

class MythListButtonItem
{
  public:
    enum CheckState {
        CantCheck = -1,
        NotChecked = 0,
        HalfChecked,
        FullChecked
    };

    MythListButtonItem(MythListButton *lbtype, const QString& text,
                       MythImage *image = 0, bool checkable = false,
                       CheckState state = CantCheck, bool showArrow = false);
    MythListButtonItem(MythListButton *lbtype, const QString& text,
                       QVariant data);
    ~MythListButtonItem();

    MythListButton *parent() const;

    void setText(const QString &text);
    QString text() const;

    void setImage(MythImage *image);
    const MythImage *image() const;

    bool checkable() const;
    void setCheckable(bool flag);

    CheckState state() const;
    void setChecked(CheckState state);

    void setDrawArrow(bool flag);

    // Deprecated, use the QVariant based SetData & GetData instead
    void setData(void *data);
    void *getData();
    // ------------

    void SetData(QVariant data);
    QVariant GetData();

    void setOverrideInactive(bool flag);
    bool getOverrideInactive(void);

    bool moveUpDown(bool flag);

    void SetToRealButton(MythUIButton *button, bool active_on);

  protected:
    MythListButton *m_parent;
    QString         m_text;
    MythImage      *m_image;
    bool            m_checkable;
    CheckState      m_state;
    QVariant        m_data;
    bool            m_showArrow;
    bool            m_overrideInactive;

    friend class MythListButton;
};

class MythListButton : public MythUIType, public StorageUser
{
    Q_OBJECT
  public:
    MythListButton(MythUIType *parent, const QString &name);
    MythListButton(MythUIType *parent, const QString &name,
                   const QRect &area, bool showArrow = true,
                   bool showScrollArrows = false);
    ~MythListButton();

    virtual bool keyPressEvent(QKeyEvent *);
    virtual void gestureEvent(MythUIType *uitype, MythGestureEvent *event);

    void SetFontActive(const MythFontProperties &font);
    void SetFontInactive(const MythFontProperties &font);

    void SetTextFlags(int flags);

    void SetSpacing(int spacing);
    void SetMargin(int marginX, int marginY = 0);
    int  GetMarginX() { return m_itemMarginX; }
    int  GetMarginY() { return m_itemMarginY; }
    void SetDrawFromBottom(bool draw);

    void SetActive(bool active);
    bool isActive() { return m_active; }
    void Reset();
    void Update();

    void SetValue(int value) { MoveToNamedPosition(QString::number(value)); }
    void SetValue(QString value) { MoveToNamedPosition(value); }
    void SetValueByData(QVariant data);
    int  GetIntValue() { return GetItemCurrent()->text().toInt(); }
    QString  GetValue() { return GetItemCurrent()->text(); }

    void SetItemCurrent(MythListButtonItem* item);
    void SetItemCurrent(int pos);
    MythListButtonItem* GetItemCurrent();
    MythListButtonItem* GetItemFirst();
    MythListButtonItem* GetItemNext(MythListButtonItem *item);
    MythListButtonItem* GetItemAt(int pos);

    virtual uint ItemWidth(void) const { return m_itemWidth; }
    virtual uint ItemHeight(void) const { return m_itemHeight; }

    bool MoveItemUpDown(MythListButtonItem *item, bool flag);

    void SetAllChecked(MythListButtonItem::CheckState state);

    int GetCurrentPos() { return m_selPosition; }
    int GetItemPos(MythListButtonItem* item);
    int GetCount();
    bool IsEmpty();

    enum ScrollStyle { ScrollFree, ScrollCenter };
    enum LayoutType { LayoutVertical, LayoutHorizontal, LayoutGrid };
    enum MovementUnit { MoveItem, MoveColumn, MoveRow, MovePage, MoveMax };

    void MoveDown(MovementUnit unit = MoveItem);
    void MoveUp(MovementUnit unit = MoveItem);
    bool MoveToNamedPosition(const QString &position_name);

    void SetDrawOffset(QPoint off) { m_drawoffset = off; }
    QPoint GetDrawOffset(void) { return m_drawoffset; }

    // StorageUser
    void SetDBValue(const QString &value);
    QString GetDBValue(void) const;

  public slots:
    void Select();
    void Deselect();

  signals:
    void itemSelected(MythListButtonItem* item);
    void itemClicked(MythListButtonItem* item);

  protected:
    void Const();
    virtual void Init();
    void LoadPixmap(MythImage **pix, QDomElement &element);

    void InsertItem(MythListButtonItem *item);
    void RemoveItem(MythListButtonItem *item);

    void SetPositionArrowStates(void);

    /* methods for subclasses to override */
    virtual QRect CalculateContentsRect(const QRect &arrowsRect) const;
    virtual void CalculateVisibleItems(void);
    virtual const QRect PlaceArrows(const QSize &arrowSize);
    virtual QPoint GetButtonPosition(int column, int row) const;

    virtual bool ParseElement(QDomElement &element);
    virtual void CopyFrom(MythUIType *base);
    virtual void CreateCopy(MythUIType *parent);
    virtual void Finalize(void);

    /**/

    LayoutType m_layout;
    ScrollStyle m_scrollStyle;

    int m_order;
    QRect m_rect;
    QRect m_contentsRect;

    int m_itemWidth;
    int m_itemHeight;
    int m_itemHorizSpacing;
    int m_itemVertSpacing;
    int m_itemMarginX;
    int m_itemMarginY;
    uint m_itemsVisible;
    int m_rows;
    int m_columns;

    bool m_active;
    bool m_showArrow;

    MythUIStateType *m_upArrow;
    MythUIStateType *m_downArrow;

    QVector<MythUIButton *> m_ButtonList;

    MythFontProperties *m_fontActive;
    MythFontProperties *m_fontInactive;

    bool m_initialized;
    bool m_clearing;

    MythListButtonItem* m_topItem;
    MythListButtonItem* m_selItem;

    int m_selPosition;
    int m_topPosition;
    int m_itemCount;

    QList<MythListButtonItem*> m_itemList;

    QPoint m_drawoffset;
    bool m_drawFromBottom;

    int m_textFlags;
    int m_imageAlign;

    MythImage *arrowPix, *checkNonePix, *checkHalfPix, *checkFullPix;
    MythImage *itemRegPix, *itemSelActPix, *itemSelInactPix;

    friend class MythListButtonItem;
    friend class MythUIButtonTree;
};

#endif
