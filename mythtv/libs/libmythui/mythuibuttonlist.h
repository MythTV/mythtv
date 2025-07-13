#ifndef MYTHUIBUTTONLIST_H_
#define MYTHUIBUTTONLIST_H_

#include <utility>

// Qt headers
#include <QHash>
#include <QList>
#include <QString>
#include <QVariant>
#include <optional>

// MythTV headers
#include "mythuitype.h"
#include "mythscreentype.h"
#include "mythimage.h"

class MythUIButtonList;
class MythUIScrollBar;
class MythUIStateType;
class MythUIGroup;
class MythUIProgressBar;

struct TextProperties {
    QString text;
    QString state;
};

struct ProgressInfo {
    int8_t start {0}; // All in the range [0-100]
    int8_t total {0};
    int8_t used  {0};
};

using muibCbFn = QString (*)(const QString &name, void *data);
struct muibCbInfo
{
    muibCbFn fn   {nullptr};
    void*    data {nullptr};
};

class MUI_PUBLIC MythUIButtonListItem
{
  public:
    enum CheckState : std::int8_t {
        CantCheck = -1,
        NotChecked = 0,
        HalfChecked = 1,
        FullChecked = 2
    };

    MythUIButtonListItem(MythUIButtonList *lbtype, QString text,
                         QString image = "", bool checkable = false,
                         CheckState state = CantCheck, bool showArrow = false,
                         int listPosition = -1);
    MythUIButtonListItem(MythUIButtonList *lbtype, const QString& text,
                         QVariant data, int listPosition = -1);
    template <typename SLOT>
    MythUIButtonListItem(std::enable_if_t<FunctionPointerTest<SLOT>::MemberFunction, MythUIButtonList *>lbtype,
                         const QString& text, SLOT slot, int listPosition = -1)
        : MythUIButtonListItem(lbtype, text, QVariant::fromValue(static_cast<MythUICallbackMF>(slot)), listPosition) { }
    template <typename SLOT>
    MythUIButtonListItem(std::enable_if_t<FunctionPointerTest<SLOT>::MemberConstFunction, MythUIButtonList *>lbtype,
                         const QString& text, SLOT slot, int listPosition = -1)
        : MythUIButtonListItem(lbtype, text, QVariant::fromValue(static_cast<MythUICallbackMFc>(slot)), listPosition) { }
    virtual ~MythUIButtonListItem();

    MythUIButtonList *parent() const;

    void SetText(const QString &text, const QString &name="",
                 const QString &state="");
    void SetTextFromMap(const InfoMap &infoMap, const QString &state="");
    void SetTextFromMap(const QMap<QString, TextProperties> &stringMap);
    void SetTextCb(muibCbFn fn, void *data);
    QString GetText(const QString &name="") const;
    TextProperties GetTextProp(const QString &name = "") const;

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
    void SetImageCb(muibCbFn fn, void *data);
    QString GetImageFilename(const QString &name="") const;

    void SetProgress1(int start, int total, int used);
    void SetProgress2(int start, int total, int used);

    void DisplayState(const QString &state, const QString &name);
    void SetStatesFromMap(const InfoMap &stateMap);
    void SetStateCb(muibCbFn fn, void *data);
    QString GetState(const QString &name);

    bool isVisible() const { return m_isVisible; }
    void setVisible(bool flag) { m_isVisible = flag; }

    bool checkable() const;
    void setCheckable(bool flag);

    bool isEnabled() const;
    void setEnabled(bool flag);

    CheckState state() const;
    void setChecked(CheckState state);

    void setDrawArrow(bool flag);

    void SetData(QVariant data);
    QVariant GetData();

    bool MoveUpDown(bool flag);

    virtual void SetToRealButton(MythUIStateType *button, bool selected);

  private:
    void DoButtonText(MythUIText *buttontext);
    void DoButtonImage(MythUIImage *buttonimage);
    void DoButtonArrow(MythUIImage *buttonarrow) const;
    void DoButtonCheck(MythUIStateType *buttoncheck);
    void DoButtonProgress1(MythUIProgressBar *buttonprogress) const;
    void DoButtonProgress2(MythUIProgressBar *buttonprogress) const;
    void DoButtonLookupText(MythUIText *text, const TextProperties& textprop);
    static void DoButtonLookupFilename(MythUIImage *image, const QString& filename);
    static void DoButtonLookupImage(MythUIImage *uiimage, MythImage *image);
    static void DoButtonLookupState(MythUIStateType *statetype, const QString& name);

  protected:
    MythUIButtonList *m_parent      {nullptr};
    QString         m_text;
    QString         m_fontState;
    MythImage      *m_image         {nullptr};
    QString         m_imageFilename;
    bool            m_checkable     {false};
    CheckState      m_state         {CantCheck};
    QVariant        m_data          {0};
    bool            m_showArrow     {false};
    bool            m_isVisible     {false};
    bool            m_enabled       {true};
    bool            m_debugme       {false};
    ProgressInfo    m_progress1      {0,0,0};
    ProgressInfo    m_progress2      {0,0,0};

    QMap<QString, TextProperties> m_strings;
    QMap<QString, MythImage*> m_images;
    InfoMap m_imageFilenames;
    InfoMap m_states;
    muibCbInfo m_textCb;
    muibCbInfo m_imageCb;
    muibCbInfo m_stateCb;

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
    MythUIButtonList(MythUIType *parent, const QString &name,
                     const QString &shadow = "");
    MythUIButtonList(MythUIType *parent, const QString &name,
                   QRect area, bool showArrow = true,
                   bool showScrollBar = false);
    ~MythUIButtonList() override;

    bool keyPressEvent(QKeyEvent *event) override; // MythUIType
    bool gestureEvent(MythGestureEvent *event) override; // MythUIType
    void customEvent(QEvent *event) override; // MythUIType

    enum MovementUnit : std::uint8_t
                      { MoveItem, MoveColumn, MoveRow, MovePage, MoveMax,
                        MoveMid, MoveByAmount };
    enum LayoutType : std::uint8_t
                     { LayoutVertical, LayoutHorizontal, LayoutGrid };

#if 0
    void SetDrawFromBottom(bool draw);
#endif

    void Reset() override; // MythUIType
    void Update();

    virtual void SetValue(int value) { MoveToNamedPosition(QString::number(value)); }
    virtual void SetValue(const QString &value) { MoveToNamedPosition(value); }
    void SetValueByData(const QVariant& data);
    virtual int  GetIntValue() const;
    virtual QString  GetValue() const;
    QVariant GetDataValue() const;
    MythRect GetButtonArea(void) const;

    void SetItemCurrent(MythUIButtonListItem* item);
    void SetItemCurrent(int current, int topPos = -1);
    MythUIButtonListItem* GetItemCurrent() const;
    MythUIButtonListItem* GetItemFirst() const;
    MythUIButtonListItem* GetItemNext(MythUIButtonListItem *item) const;
    MythUIButtonListItem* GetItemAt(int pos) const;
    MythUIButtonListItem* GetItemByData(const QVariant& data);

    uint ItemWidth(void);
    uint ItemHeight(void);
    LayoutType GetLayout() const { return m_layout; }

    bool MoveItemUpDown(MythUIButtonListItem *item, bool up);

    void SetAllChecked(MythUIButtonListItem::CheckState state);

    int GetCurrentPos() const { return m_selPosition; }
    int GetItemPos(MythUIButtonListItem* item) const;
    int GetTopItemPos(void) const { return m_topPosition; }
    int GetCount() const;
    int GetVisibleCount();
    bool IsEmpty() const;

    virtual bool MoveDown(MovementUnit unit = MoveItem, uint amount = 0);
    virtual bool MoveUp(MovementUnit unit = MoveItem, uint amount = 0);
    bool MoveToNamedPosition(const QString &position_name);

    void RemoveItem(MythUIButtonListItem *item);

    bool IsShadowing(void) { return m_shadowListName == GetFocusedName(); }
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
    enum ScrollStyle : std::uint8_t
                     { ScrollFree, ScrollCenter, ScrollGroupCenter };
    enum ArrangeType : std::uint8_t
                     { ArrangeFixed, ArrangeFill, ArrangeSpread, ArrangeStack };
    enum WrapStyle   : std::int8_t
                     { WrapCaptive = -1, WrapNone = 0, WrapSelect = 1,
                       WrapItems = 2, WrapFlowing = 3 };

    void DrawSelf(MythPainter *p, int xoffset, int yoffset,
                  int alphaMod, QRect clipRect) override; // MythUIType
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

    void FindEnabledUp(MovementUnit unit);
    void FindEnabledDown(MovementUnit unit);

    /* methods for subclasses to override */
    virtual void CalculateVisibleItems(void);
    virtual QPoint GetButtonPosition(int column, int row) const;

    void SetButtonArea(const MythRect &rect);
    bool ParseElement(const QString &filename, QDomElement &element,
                      bool showWarnings) override; // MythUIType
    void CopyFrom(MythUIType *base) override; // MythUIType
    void CreateCopy(MythUIType *parent) override; // MythUIType
    void Finalize(void) override; // MythUIType

    void SanitizePosition(void);

    /**/

    LayoutType  m_layout              {LayoutVertical};
    ArrangeType m_arrange             {ArrangeFixed};
    ScrollStyle m_scrollStyle         {ScrollFree};
    WrapStyle   m_wrapStyle           {WrapNone};
    int         m_defaultAlignment    {Qt::AlignLeft | Qt::AlignTop};
    std::optional<int> m_shadowAlignment {std::nullopt};
    MythRect    m_contentsRect        {0, 0, 0, 0};

    MythPoint   m_searchPosition      {-2,-2};
    QString     m_searchFields        {"**ALL**"};
    QString     m_searchStr;
    bool        m_searchStartsWith    {false};

    int m_itemWidth                   {0};
    int m_itemHeight                  {0};
    int m_itemHorizSpacing            {0};
    int m_itemVertSpacing             {0};
    int m_itemsVisible                {0};
    int m_maxVisible                  {0};
    int m_rows                        {0};
    int m_columns                     {0};
    int m_leftColumns                 {0};
    int m_rightColumns                {0};
    int m_topRows                     {0};
    int m_bottomRows                  {0};

    QString m_shadowListName;

    bool m_active                     {false};
    bool m_showArrow                  {true};
    bool m_showScrollBar              {true};

    MythUIScrollBar *m_scrollBar      {nullptr};
    MythUIStateType *m_upArrow        {nullptr};
    MythUIStateType *m_downArrow      {nullptr};

    MythUIStateType *m_buttontemplate {nullptr};

    QVector<MythUIStateType *> m_buttonList;
    QMap<int, MythUIButtonListItem *> m_buttonToItem;
    QHash<QString, QString> m_actionRemap;

    bool m_initialized                {false};
    bool m_needsUpdate                {false};
    bool m_clearing                   {false};

    int m_selPosition                 {0};
    int m_topPosition                 {0};
    int m_itemCount                   {0};
    bool m_keepSelAtBottom            {false};

    QList<MythUIButtonListItem*> m_itemList;
    int m_nextItemLoaded              {0};

    bool m_defaultDrawFromBottom      {false};
    std::optional<bool> m_shadowDrawFromBottom {std::nullopt};

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
                           MythUIButtonList *parentList, QString searchText)
        : MythScreenType(parent, name, false),
          m_parentList(parentList), m_searchText(std::move(searchText)) {}
    ~SearchButtonListDialog(void) override = default;

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType

  protected slots:
    void searchChanged(void);
    void prevClicked(void);
    void nextClicked(void);

  protected:
    bool               m_startsWith  {false};

    MythUIButtonList  *m_parentList  {nullptr};
    QString            m_searchText;

    MythUITextEdit    *m_searchEdit  {nullptr};
    MythUIButton      *m_prevButton  {nullptr};
    MythUIButton      *m_nextButton  {nullptr};
    MythUIStateType   *m_searchState {nullptr};
};

Q_DECLARE_METATYPE(MythUIButtonListItem *)

#endif
