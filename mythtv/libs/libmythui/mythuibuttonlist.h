#ifndef MYTHUIBUTTONLIST_H_
#define MYTHUIBUTTONLIST_H_

#include <QList>
#include <QHash>
#include <QString>
#include <QVariant>

#include "mythuitype.h"
#include "mythuiimage.h"
#include "mythuitext.h"
#include "mythuigroup.h"

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

    void SetText(const QString &text, const QString &name="",
                 const QString &state="");
    void SetTextFromMap(QHash<QString, QString> &infoMap, const QString &state="");
    QString GetText(const QString &name="") const;

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

    void SetImage(const QString &filename, const QString &name="",
                  bool force_reload = false);
    QString GetImage(const QString &name="") const;

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
    virtual bool gestureEvent(MythGestureEvent *event);

    enum MovementUnit { MoveItem, MoveColumn, MoveRow, MovePage, MoveMax,
                        MoveMid, MoveByAmount };

    void SetDrawFromBottom(bool draw);

    void Reset();
    void Update();

    virtual void SetValue(int value) { MoveToNamedPosition(QString::number(value)); }
    virtual void SetValue(QString value) { MoveToNamedPosition(value); }
    void SetValueByData(QVariant data);
    virtual int  GetIntValue() const;
    virtual QString  GetValue() const;
    QVariant GetDataValue() const;

    void SetItemCurrent(MythUIButtonListItem* item);
    void SetItemCurrent(int pos, int topPos = -1);
    MythUIButtonListItem* GetItemCurrent() const;
    MythUIButtonListItem* GetItemFirst() const;
    MythUIButtonListItem* GetItemNext(MythUIButtonListItem *item) const;
    MythUIButtonListItem* GetItemAt(int pos) const;
    MythUIButtonListItem* GetItemByData(QVariant data);

    uint ItemWidth(void);
    uint ItemHeight(void);

    bool MoveItemUpDown(MythUIButtonListItem *item, bool up);

    void SetAllChecked(MythUIButtonListItem::CheckState state);

    int GetCurrentPos() const { return m_selPosition; }
    int GetItemPos(MythUIButtonListItem* item) const;
    int GetTopItemPos(void) const { return m_topPosition; }
    int GetCount() const;
    uint GetVisibleCount();
    bool IsEmpty() const;

    virtual bool MoveDown(MovementUnit unit = MoveItem, uint amount = 0);
    virtual bool MoveUp(MovementUnit unit = MoveItem, uint amount = 0);
    bool MoveToNamedPosition(const QString &position_name);

    void RemoveItem(MythUIButtonListItem *item);

    void SetLCDTitles(const QString &title, const QString &columnList = "");

  public slots:
    void Select();
    void Deselect();

  signals:
    void itemSelected(MythUIButtonListItem* item);
    void itemClicked(MythUIButtonListItem* item);
    void itemVisible(MythUIButtonListItem* item);

  protected:
    enum ScrollStyle { ScrollFree, ScrollCenter, ScrollGroupCenter };
    enum LayoutType  { LayoutVertical, LayoutHorizontal, LayoutGrid };
    enum ArrangeType { ArrangeFixed, ArrangeFill, ArrangeSpread, ArrangeStack };
    enum WrapStyle   { WrapCaptive = -1, WrapNone = 0, WrapSelect, WrapItems,
                       WrapFlowing };

    virtual void DrawSelf(MythPainter *p, int xoffset, int yoffset,
                          int alphaMod, QRect clipRect);
    void Const();
    virtual void Init();

    void InsertItem(MythUIButtonListItem *item);

    int minButtonWidth(const MythRect & area);
    int minButtonHeight(const MythRect & area);
    void InitButton(int itemIdx, MythUIStateType* & realButton,
                    MythUIButtonListItem* & buttonItem);
    MythUIGroup *PrepareButton(int buttonIdx, int itemIdx,
                               int & selectedIdx, int & button_shift);
    bool DistributeRow(int & first_button, int & last_button,
                       int & first_item, int & last_item,
                       int & selected_column,
                       bool grow_left, bool grow_right,
                       int ** col_widths, int & row_height,
                       int total_height, int split_height,
                       int & col_cnt, bool & wrapped);
    bool DistributeCols(int & first_button, int & last_button,
                        int & first_item, int & last_item,
                        int & selected_column, int & selected_row,
                        int ** col_widths, QList<int> & row_heights,
                        int & top_height, int & bottom_height,
                        bool & wrapped);
    bool DistributeButtons(void);
    void SetPosition(void);
    void SetPositionArrowStates(void);
    void ItemVisible(MythUIButtonListItem *item);

    void updateLCD(void);

    void SetActive(bool active);

    int PageUp(void);
    int PageDown(void);

    /* methods for subclasses to override */
    virtual void CalculateVisibleItems(void);
    virtual QPoint GetButtonPosition(int column, int row) const;

    virtual bool ParseElement(
        const QString &filename, QDomElement &element, bool showWarnings);
    virtual void CopyFrom(MythUIType *base);
    virtual void CreateCopy(MythUIType *parent);
    virtual void Finalize(void);

    void SanitizePosition(void);

    /**/

    LayoutType  m_layout;
    ArrangeType m_arrange;
    ScrollStyle m_scrollStyle;
    WrapStyle   m_wrapStyle;
    int         m_alignment;

    MythRect m_contentsRect;

    int m_itemWidth;
    int m_itemHeight;
    int m_itemHorizSpacing;
    int m_itemVertSpacing;
    uint m_itemsVisible;
    int m_maxVisible;
    int m_rows;
    int m_columns;
    int m_leftColumns, m_rightColumns;
    int m_topRows, m_bottomRows;

    bool m_active;
    bool m_showArrow;

    MythUIStateType *m_upArrow;
    MythUIStateType *m_downArrow;

    MythUIStateType *m_buttontemplate;

    QVector<MythUIStateType *> m_ButtonList;
    QMap<int, MythUIButtonListItem *> m_ButtonToItem;

    bool m_initialized;
    bool m_needsUpdate;
    bool m_clearing;

    int m_selPosition;
    int m_topPosition;
    int m_itemCount;
    bool m_keepSelAtBottom;

    QList<MythUIButtonListItem*> m_itemList;

    bool m_drawFromBottom;

    QString     m_lcdTitle;
    QStringList m_lcdColumns;

    friend class MythUIButtonListItem;
    friend class MythUIButtonTree;
};

Q_DECLARE_METATYPE(MythUIButtonListItem *)

#endif
