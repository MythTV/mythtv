#ifndef MYTHLISTBUTTON_H_
#define MYTHLISTBUTTON_H_

#include "mythuitype.h"
#include "mythuiimage.h"
#include "mythuitext.h"
#include "mythuibutton.h"
#include "mythgesture.h"

class MythListButtonItem;
class MythFontProperties;
class MythUIStateType;

class MythListButton : public MythUIType
{
    Q_OBJECT
  public:
    MythListButton(MythUIType *parent, const char *name);
    MythListButton(MythUIType *parent, const char *name, 
                   const QRect &area, bool showArrow = true, 
                   bool showScrollArrows = false);
    ~MythListButton();

    virtual bool keyPressEvent(QKeyEvent *);
    virtual void gestureEvent(MythUIType *uitype, MythGestureEvent *event);
    virtual MythUIType *GetChildAt(const QPoint &p);

    void SetFontActive(const MythFontProperties &font);
    void SetFontInactive(const MythFontProperties &font);

    void SetTextFlags(int flags);

    void SetSpacing(int spacing);
    void SetMargin(int margin);
    void SetDrawFromBottom(bool draw);

    void SetActive(bool active);
    bool isActive() { return m_active; }
    void Reset();

    void SetItemCurrent(MythListButtonItem* item);
    void SetItemCurrent(int pos);
    MythListButtonItem* GetItemCurrent();
    MythListButtonItem* GetItemFirst();
    MythListButtonItem* GetItemNext(MythListButtonItem *item);
    MythListButtonItem* GetItemAt(int pos);

    bool MoveItemUpDown(MythListButtonItem *item, bool flag);

    QPtrListIterator<MythListButtonItem> GetIterator();

    int GetCurrentPos() { return m_selPosition; }
    int GetItemPos(MythListButtonItem* item);
    int GetCount();
    bool IsEmpty();

    enum MovementUnit { MoveItem, MoveColumn, MoveRow, MovePage, MoveMax };
    enum LayoutType { LayoutVertical, LayoutHorizontal, LayoutGrid };
    void MoveDown(MovementUnit unit = MoveItem);
    void MoveUp(MovementUnit unit = MoveItem);
    bool MoveToNamedPosition(const QString &position_name);

    void SetDrawOffset(QPoint off) { m_drawoffset = off; }
    QPoint GetDrawOffset(void) { return m_drawoffset; }

  public slots:
    void Select() { SetActive(true); }
    void Deselect() { SetActive(false); }

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
    virtual uint ItemWidth(void) const { return m_itemWidth; }
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

    int m_order;
    QRect m_rect;
    QRect m_contentsRect;

    int m_itemHeight;
    int m_itemHorizSpacing;
    int m_itemVertSpacing;
    int m_itemMargin;
    uint m_itemsVisible;
    int m_rows;
    int m_columns;
    int m_itemWidth;

    bool m_active;
    bool m_showScrollArrows;
    bool m_showArrow;

    MythUIStateType *m_upArrow;
    MythUIStateType *m_downArrow;

    QValueVector<MythUIButton *> m_ButtonList;

    MythFontProperties *m_fontActive;
    MythFontProperties *m_fontInactive;

    bool m_initialized;
    bool m_clearing;

    MythListButtonItem* m_topItem;
    MythListButtonItem* m_selItem;

    QPtrListIterator<MythListButtonItem> *m_topIterator;
    QPtrListIterator<MythListButtonItem> *m_selIterator;

    int m_selPosition;
    int m_topPosition;
    int m_itemCount;

    QPtrList<MythListButtonItem> m_itemList;

    QPoint m_drawoffset;
    bool m_drawFromBottom;

    int m_textFlags;

    MythImage *arrowPix, *checkNonePix, *checkHalfPix, *checkFullPix;
    MythImage *itemRegPix, *itemSelActPix, *itemSelInactPix;

    friend class MythListButtonItem;
};

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

    void setData(void *data);
    void *getData();

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
    void           *m_data;
    bool            m_showArrow;
    bool            m_overrideInactive;

    friend class MythListButton;
};

#endif
