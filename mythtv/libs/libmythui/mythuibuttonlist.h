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
#include "mythscreentype.h"

class MythUIButtonList;
class MythUIScrollBar;
class MythUIStateType;

struct TextProperties {
    QString text;
    QString state;
};

class MUI_PUBLIC MythUIButtonListItem
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
                         CheckState state = CantCheck, bool showArrow = false,
                         int listPosition = -1);
    MythUIButtonListItem(MythUIButtonList *lbtype, const QString& text,
                         QVariant data, int listPosition = -1);
    virtual ~MythUIButtonListItem();

    MythUIButtonList *parent() const;

    void SetText(const QString &text, const QString &name="",
                 const QString &state="");
    void SetTextFromMap(const InfoMap &infoMap, const QString &state="");
    void SetTextFromMap(const QMap<QString, TextProperties> &stringMap);
    QString GetText(const QString &name="") const;

    bool FindText(const QString &searchStr, const QString &fieldList = "**ALL**",
                  bool startsWith = false) const;

    void SetFontState(const QString &state, const QString &name="");

    /** Sets an image directly, should only be used in special circumstances
     *  since it bypasses the cache.
     */
    void SetImage(MythImage *image, const QString &name="");

    /** Gets a MythImage which has been assigned to this button item,
     *  as with SetImage() it should only be used in special circumstances
     *  since it bypasses the cache.
     *  \note The reference count is set for one use, call DecrRef() to delete.
     */
    MythImage *GetImage(const QString &name="");

    /// Returns true when the image exists.
    bool HasImage(const QString &name="")
    {
        MythImage *img = GetImage(name);
        if (img)
        {
            img->DecrRef();
            return true;
        }

        return false;
    }

    void SetImage(const QString &filename, const QString &name="",
                  bool force_reload = false);
    void SetImageFromMap(const InfoMap &imageMap);
    QString GetImageFilename(const QString &name="") const;

    void DisplayState(const QString &state, const QString &name);
    void SetStatesFromMap(const InfoMap &stateMap);

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
    InfoMap m_imageFilenames;
    InfoMap m_states;

    friend class MythUIButtonList;
    friend class MythGenericTree;
};

/**
 * \class MythUIButtonList
 *
 * \brief List widget, displays list items in a variety of themeable
 *        arrangements and can trigger signals when items are highlighted or
 *        clicked
 *
 * \ingroup MythUI_Widgets
 */
class MUI_PUBLIC MythUIButtonList : public MythUIType
{
    Q_OBJECT
  public:
    MythUIButtonList(MythUIType *parent, const QString &name);
    MythUIButtonList(MythUIType *parent, const QString &name,
                   const QRect &area, bool showArrow = true,
                   bool showScrollArrows = false,
                   bool showScrollBar = false);
    ~MythUIButtonList();

    virtual bool keyPressEvent(QKeyEvent *);
    virtual bool gestureEvent(MythGestureEvent *event);
    virtual void customEvent(QEvent *);

    enum MovementUnit { MoveItem, MoveColumn, MoveRow, MovePage, MoveMax,
                        MoveMid, MoveByAmount };
    enum LayoutType  { LayoutVertical, LayoutHorizontal, LayoutGrid };

    void SetDrawFromBottom(bool draw);

    void Reset();
    void Update();

    virtual void SetValue(int value) { MoveToNamedPosition(QString::number(value)); }
    virtual void SetValue(const QString &value) { MoveToNamedPosition(value); }
    void SetValueByData(QVariant data);
    virtual int  GetIntValue() const;
    virtual QString  GetValue() const;
    QVariant GetDataValue() const;
    MythRect GetButtonArea(void) const;

    void SetItemCurrent(MythUIButtonListItem* item);
    void SetItemCurrent(int pos, int topPos = -1);
    MythUIButtonListItem* GetItemCurrent() const;
    MythUIButtonListItem* GetItemFirst() const;
    MythUIButtonListItem* GetItemNext(MythUIButtonListItem *item) const;
    MythUIButtonListItem* GetItemAt(int pos) const;
    MythUIButtonListItem* GetItemByData(QVariant data);

    uint ItemWidth(void);
    uint ItemHeight(void);
    LayoutType GetLayout() const { return m_layout; }

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
    void updateLCD(void);

    void SetSearchFields(const QString &fields) { m_searchFields = fields; }
    void ShowSearchDialog(void);
    bool Find(const QString &searchStr, bool startsWith = false);
    bool FindNext(void);
    bool FindPrev(void);

    void LoadInBackground(int start = 0, int pageSize = 20);
    int  StopLoad(void);

  public slots:
    void Select();
    void Deselect();
    void ToggleEnabled();

  signals:
    void itemSelected(MythUIButtonListItem* item);
    void itemClicked(MythUIButtonListItem* item);
    void itemVisible(MythUIButtonListItem* item);
    void itemLoaded(MythUIButtonListItem* item);

  protected:
    enum ScrollStyle { ScrollFree, ScrollCenter, ScrollGroupCenter };
    enum ArrangeType { ArrangeFixed, ArrangeFill, ArrangeSpread, ArrangeStack };
    enum WrapStyle   { WrapCaptive = -1, WrapNone = 0, WrapSelect, WrapItems,
                       WrapFlowing };

    virtual void DrawSelf(MythPainter *p, int xoffset, int yoffset,
                          int alphaMod, QRect clipRect);
    void Const();
    virtual void Init();

    void InsertItem(MythUIButtonListItem *item, int listPosition = -1);

    int minButtonWidth(const MythRect & area);
    int minButtonHeight(const MythRect & area);
    void InitButton(int itemIdx, MythUIStateType* & realButton,
                    MythUIButtonListItem* & buttonItem);
    MythUIGroup *PrepareButton(int buttonIdx, int itemIdx,
                               int & selectedIdx, int & button_shift);
    bool DistributeRow(int & first_button, int & last_button,
                       int & first_item, int & last_item,
                       int & selected_column, int & skip_cols,
                       bool grow_left, bool grow_right,
                       int ** col_widths, int & row_height,
                       int total_height, int split_height,
                       int & col_cnt, bool & wrapped);
    bool DistributeCols(int & first_button, int & last_button,
                        int & first_item, int & last_item,
                        int & selected_column, int & selected_row,
                        int & skip_cols, int ** col_widths,
                        QList<int> & row_heights,
                        int & top_height, int & bottom_height,
                        bool & wrapped);
    bool DistributeButtons(void);
    void CalculateButtonPositions(void);
    void CalculateArrowStates(void);
    void SetScrollBarPosition(void);
    void ItemVisible(MythUIButtonListItem *item);

    void SetActive(bool active);

    int PageUp(void);
    int PageDown(void);

    bool DoFind(bool doMove, bool searchForward);

    /* methods for subclasses to override */
    virtual void CalculateVisibleItems(void);
    virtual QPoint GetButtonPosition(int column, int row) const;

    void SetButtonArea(const MythRect &rect);
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

    MythPoint m_searchPosition;
    QString   m_searchFields;
    QString   m_searchStr;
    bool      m_searchStartsWith;

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
    bool m_showScrollBar;

    MythUIScrollBar *m_scrollBar;
    MythUIStateType *m_upArrow;
    MythUIStateType *m_downArrow;

    MythUIStateType *m_buttontemplate;

    QVector<MythUIStateType *> m_ButtonList;
    QMap<int, MythUIButtonListItem *> m_ButtonToItem;
    QHash<QString, QString> m_actionRemap;

    bool m_initialized;
    bool m_needsUpdate;
    bool m_clearing;

    int m_selPosition;
    int m_topPosition;
    int m_itemCount;
    bool m_keepSelAtBottom;

    QList<MythUIButtonListItem*> m_itemList;
    int m_nextItemLoaded;

    bool m_drawFromBottom;

    QString     m_lcdTitle;
    QStringList m_lcdColumns;

    friend class MythUIButtonListItem;
    friend class MythUIButtonTree;
};

class MUI_PUBLIC SearchButtonListDialog : public MythScreenType
{
    Q_OBJECT
  public:
    SearchButtonListDialog(MythScreenStack *parent, const char *name,
                           MythUIButtonList *parentList, QString searchText);
    ~SearchButtonListDialog(void);

    bool Create(void);
    bool keyPressEvent(QKeyEvent *event);

  protected slots:
    void searchChanged(void);
    void prevClicked(void);
    void nextClicked(void);

  protected:
    bool               m_startsWith;

    MythUIButtonList  *m_parentList;
    QString            m_searchText;

    MythUITextEdit    *m_searchEdit;
    MythUIButton      *m_prevButton;
    MythUIButton      *m_nextButton;
    MythUIStateType   *m_searchState;
};

Q_DECLARE_METATYPE(MythUIButtonListItem *)

#endif
