#ifndef MYTHUIBUTTONLIST_H_
#define MYTHUIBUTTONLIST_H_

#include <QList>
#include <QString>
#include <QVariant>

#include "mythuitype.h"
#include "mythuiimage.h"
#include "mythuitext.h"
#include "mythgesture.h"

class MythUIButtonList;
class MythFontProperties;
class MythUIStateType;

class MythUIButtonListItem
{
  public:
    enum CheckState {
        CantCheck = -1,
        NotChecked = 0,
        HalfChecked,
        FullChecked
    };

    MythUIButtonListItem(MythUIButtonList *lbtype, const QString& text,
                         const QString& image = "", bool checkable = false,
                         CheckState state = CantCheck, bool showArrow = false);
    MythUIButtonListItem(MythUIButtonList *lbtype, const QString& text,
                         QVariant data);
    ~MythUIButtonListItem();

    MythUIButtonList *parent() const;

    void setText(const QString &text, const QString &name="");
    QString text() const;

    void setImage(MythImage *image, const QString &name="") __attribute__ ((deprecated));

    void SetImage(const QString &filename, const QString &name="");
    const QString Image() const;

    void DisplayState(const QString &state, const QString &name);

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

    void SetToRealButton(MythUIStateType *button, bool active_on);

  protected:
    MythUIButtonList *m_parent;
    QString         m_text;
    MythImage      *m_image;
    QString         m_imageFilename;
    bool            m_checkable;
    CheckState      m_state;
    QVariant        m_data;
    bool            m_showArrow;
    bool            m_overrideInactive;

    QMap<QString, QString> m_strings;
    QMap<QString, MythImage*> m_images;
    QMap<QString, QString> m_imageFilenames;
    QMap<QString, QString> m_states;

    friend class MythUIButtonList;
};

class MythUIButtonList : public MythUIType
{
    Q_OBJECT
  public:
    MythUIButtonList(MythUIType *parent, const QString &name);
    MythUIButtonList(MythUIType *parent, const QString &name,
                   const QRect &area, bool showArrow = true,
                   bool showScrollArrows = false);
    ~MythUIButtonList();

    virtual bool keyPressEvent(QKeyEvent *);
    virtual void gestureEvent(MythUIType *uitype, MythGestureEvent *event);

    void SetFontActive(const MythFontProperties &font);
    void SetFontInactive(const MythFontProperties &font);

    void SetTextFlags(int flags);

    void SetSpacing(int spacing);
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

    void SetItemCurrent(MythUIButtonListItem* item);
    void SetItemCurrent(int pos);
    MythUIButtonListItem* GetItemCurrent() const;
    MythUIButtonListItem* GetItemFirst() const;
    MythUIButtonListItem* GetItemNext(MythUIButtonListItem *item) const;
    MythUIButtonListItem* GetItemAt(int pos) const;

    uint ItemWidth(void) const { return m_itemWidth; }
    uint ItemHeight(void) const { return m_itemHeight; }

    bool MoveItemUpDown(MythUIButtonListItem *item, bool flag);

    void SetAllChecked(MythUIButtonListItem::CheckState state);

    int GetCurrentPos() const { return m_selPosition; }
    int GetItemPos(MythUIButtonListItem* item) const;
    int GetCount() const;
    bool IsEmpty() const;

    enum ScrollStyle  { ScrollFree, ScrollCenter };
    enum LayoutType   { LayoutVertical, LayoutHorizontal, LayoutGrid };
    enum MovementUnit { MoveItem, MoveColumn, MoveRow, MovePage, MoveMax };
    enum WrapStyle    { WrapNone, WrapSelect, WrapItems };

    void MoveDown(MovementUnit unit = MoveItem);
    void MoveUp(MovementUnit unit = MoveItem);
    bool MoveToNamedPosition(const QString &position_name);

  public slots:
    void Select();
    void Deselect();

  signals:
    void itemSelected(MythUIButtonListItem* item);
    void itemClicked(MythUIButtonListItem* item);

  protected:
    virtual void DrawSelf(MythPainter *p, int xoffset, int yoffset,
                          int alphaMod, QRect clipRect);
    void Const();
    virtual void Init();

    void InsertItem(MythUIButtonListItem *item);
    void RemoveItem(MythUIButtonListItem *item);

    void SetPositionArrowStates(void);

    /* methods for subclasses to override */
    virtual void CalculateVisibleItems(void);
    virtual QPoint GetButtonPosition(int column, int row) const;

    virtual bool ParseElement(QDomElement &element);
    virtual void CopyFrom(MythUIType *base);
    virtual void CreateCopy(MythUIType *parent);
    virtual void Finalize(void);

    /**/

    LayoutType m_layout;
    ScrollStyle m_scrollStyle;
    WrapStyle m_wrapStyle;

    MythRect m_contentsRect;

    int m_itemWidth;
    int m_itemHeight;
    int m_itemHorizSpacing;
    int m_itemVertSpacing;
    uint m_itemsVisible;
    int m_rows;
    int m_columns;

    bool m_active;
    bool m_showArrow;

    MythUIStateType *m_upArrow;
    MythUIStateType *m_downArrow;

    QVector<MythUIStateType *> m_ButtonList;

    bool m_initialized;
    bool m_needsUpdate;
    bool m_clearing;

    MythUIButtonListItem* m_topItem;
    MythUIButtonListItem* m_selItem;

    int m_selPosition;
    int m_topPosition;
    int m_itemCount;

    QList<MythUIButtonListItem*> m_itemList;

    bool m_drawFromBottom;

    friend class MythUIButtonListItem;
    friend class MythUIButtonTree;
};

#endif
