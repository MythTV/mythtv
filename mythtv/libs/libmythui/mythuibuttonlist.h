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

class MPUBLIC MythUIButtonListItem
{
  public:
    enum CheckState {
        CantCheck = -1,
        NotChecked = 0,
        HalfChecked,
        FullChecked
    };

    struct TextProperties {
        QString text;
        QString state;
    };

    MythUIButtonListItem(MythUIButtonList *lbtype, const QString& text,
                         const QString& image = "", bool checkable = false,
                         CheckState state = CantCheck, bool showArrow = false);
    MythUIButtonListItem(MythUIButtonList *lbtype, const QString& text,
                         QVariant data);
    virtual ~MythUIButtonListItem();

    MythUIButtonList *parent() const;

    /** Deprecated in favour of SetText(const QString &, const QString &,
     *  const QString &)
     */
    void setText(const QString &text, const QString &name="",
                 const QString &state="") __attribute__ ((deprecated));

    /** Deprecated in favour of GetText(void)
     */
    QString text() const __attribute__ ((deprecated));

    void SetText(const QString &text, const QString &name="",
                 const QString &state="");
    QString GetText(void) const;

    void SetFontState(const QString &state, const QString &name="");

    /** Sets an image directly, should only be used in special circumstances
     *  since it bypasses the cache
     */
    void setImage(MythImage *image, const QString &name="");

    /** Gets a MythImage which has been assigned to this button item,
     *  as with setImage() it should only be used in special circumstances
     *  since it bypasses the cache
     */
    MythImage *getImage(const QString &name="");

    void SetImage(const QString &filename, const QString &name="");
    const QString Image() const;

    void DisplayState(const QString &state, const QString &name);

    bool checkable() const;
    void setCheckable(bool flag);

    CheckState state() const;
    void setChecked(CheckState state);

    void setDrawArrow(bool flag);

    void SetData(QVariant data);
    QVariant GetData();

    bool MoveUpDown(bool flag);

    virtual void SetToRealButton(MythUIStateType *button, bool selected);

  protected:
    MythUIButtonList *m_parent;
    QString         m_text;
    QString         m_fontState;
    MythImage      *m_image;
    QString         m_imageFilename;
    bool            m_checkable;
    CheckState      m_state;
    QVariant        m_data;
    bool            m_showArrow;

    QMap<QString, TextProperties> m_strings;
    QMap<QString, MythImage*> m_images;
    QMap<QString, QString> m_imageFilenames;
    QMap<QString, QString> m_states;

    friend class MythUIButtonList;
};

class MPUBLIC MythUIButtonList : public MythUIType
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

    void SetDrawFromBottom(bool draw);

    void SetActive(bool active);
    bool IsActive() { return m_active; }
    void Reset();
    void Update();

    void SetValue(int value) { MoveToNamedPosition(QString::number(value)); }
    void SetValue(QString value) { MoveToNamedPosition(value); }
    void SetValueByData(QVariant data);
    int  GetIntValue();
    QString  GetValue();

    void SetItemCurrent(MythUIButtonListItem* item);
    void SetItemCurrent(int pos);
    MythUIButtonListItem* GetItemCurrent() const;
    MythUIButtonListItem* GetItemFirst() const;
    MythUIButtonListItem* GetItemNext(MythUIButtonListItem *item) const;
    MythUIButtonListItem* GetItemAt(int pos) const;
    MythUIButtonListItem* GetItemByData(QVariant data);

    uint ItemWidth(void) const { return m_itemWidth; }
    uint ItemHeight(void) const { return m_itemHeight; }

    bool MoveItemUpDown(MythUIButtonListItem *item, bool flag);

    void SetAllChecked(MythUIButtonListItem::CheckState state);

    int GetCurrentPos() const { return m_selPosition; }
    int GetItemPos(MythUIButtonListItem* item) const;
    int GetTopItemPos(void) const { return m_topPosition; }
    int GetCount() const;
    uint GetVisibleCount() const { return m_itemsVisible; }
    bool IsEmpty() const;

    enum ScrollStyle  { ScrollFree, ScrollCenter };
    enum LayoutType   { LayoutVertical, LayoutHorizontal, LayoutGrid };
    enum MovementUnit { MoveItem, MoveColumn, MoveRow, MovePage, MoveMax };
    enum WrapStyle    { WrapNone, WrapSelect, WrapItems };

    bool MoveDown(MovementUnit unit = MoveItem);
    bool MoveUp(MovementUnit unit = MoveItem);
    bool MoveToNamedPosition(const QString &position_name);

    void RemoveItem(MythUIButtonListItem *item);

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

Q_DECLARE_METATYPE(MythUIButtonListItem *);

#endif
