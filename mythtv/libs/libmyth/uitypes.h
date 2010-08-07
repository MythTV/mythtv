#ifndef UIDATATYPES_H_
#define UIDATATYPES_H_

#include <vector>
using namespace std;

#include <QStringList>
#include <QKeyEvent>
#include <QObject>
#include <QPixmap>
#include <QVector>
#include <QList>
#include <QRect>
#include <QMap>
#include <QHash>

#include "mythdeque.h"
#include "mythwidgets.h"
#include "util.h"
#include "mythdialogs.h"
#include "generictree.h"
#include "mythwidgets.h"
#include "mythfontproperties.h"

#ifdef USING_MINGW
#undef LoadImage
#endif

class QFont;
class QPainter;
class UIType;
class MythDialog;
class MythThemedDialog;

class MPUBLIC LayerSet
{
  public:
    LayerSet(const QString &name);
   ~LayerSet();

    void    Draw(QPainter *, int, int);
    void    DrawRegion(QPainter *, QRect &, int, int);

    QString GetName() { return m_name; }
    void    SetName(const QString &name) { m_name = name; }

    void    SetDrawOrder(int order) { m_order = order; }
    int     GetDrawOrder() const { return m_order; }

    void    SetAreaRect(QRect area) { m_area = area; }
    QRect   GetAreaRect() { return m_area; }

    void    SetContext(int con) { m_context = con; }
    int     GetContext(void) { return m_context; }

    void    SetDebug(bool db) { m_debug = db; }
    void    bumpUpLayers(int a_number);
    int     getLayers(){return numb_layers;}

    void    AddType(UIType *);
    UIType  *GetType(const QString &name);
    vector<UIType *> *getAllTypes(){return allTypes;}

    void    ClearAllText(void);
    void    SetText(QHash<QString, QString> &infoMap);

    void    SetDrawFontShadow(bool state);

    void    UseAlternateArea(bool useAlt);

  private:
    bool    m_debug;
    int     m_context;
    int     m_order;
    QString m_name;
    QRect   m_area;
    int     numb_layers;

    QMap<QString, UIType *> typeList;
    vector<UIType *> *allTypes;
};

class MPUBLIC UIType : public QObject
{
  Q_OBJECT

  public:
    UIType(const QString &name);
    virtual ~UIType();

    void SetOrder(int order);
    void SetParent(LayerSet *);
    void SetScreen(double wmult, double hmult) { m_wmult = wmult; m_hmult = hmult; }
    void SetContext(int con) { m_context = con;}
    int  GetContext(){return m_context;}
    void SetDebug(bool db) { m_debug = db; }
    void allowFocus(bool yes_or_no){takes_focus = yes_or_no;}
    void SetDrawFontShadow(bool state) { drawFontShadow = state; }
    QString Name();


    bool    canTakeFocus(){ return takes_focus;}
    int     getOrder(){return m_order;}
    virtual void Draw(QPainter *, int, int);
    virtual void DrawRegion(QPainter *, QRect &, int, int);
    virtual void calculateScreenArea();
    QRect   getScreenArea(){return screen_area;}
    QString cutDown(const QString &data, QFont *font, bool multiline = false,
                    int overload_width = -1, int overload_height = -1);
    QString getName(){return m_name;}

    bool    isShown(){return !hidden;}
    bool    isHidden(){return hidden;}
    bool    isFocused(){return has_focus;}

  public slots:

    virtual bool takeFocus();
    virtual void looseFocus();
    virtual void activate(){}
    virtual void refresh();
    virtual void show();
    virtual void hide();
    virtual bool toggleShow();


  signals:

    //
    //  Ask my container to ask Qt to repaint
    //

    void requestUpdate();
    void requestUpdate(const QRect &);
    void requestRegionUpdate(const QRect &);
    void takingFocus();
    void loosingFocus();

  protected:

    double   m_wmult;
    double   m_hmult;
    int      m_context;
    int      m_order;
    bool     m_debug;
    QString  m_name;
    LayerSet *m_parent;
    bool     has_focus;
    bool     takes_focus;
    QRect    screen_area;   // The area, in real screen coordinates
    bool     drawFontShadow;
    bool     hidden;        // Is this "widget" seen or hidden ?
};

class ImageGridItem
{
    public:
        ImageGridItem(const QString &text, QPixmap *pixmap, bool selected, void *data)
        {
            this->text = text;
            this->pixmap = pixmap;
            this->selected = selected;
            this->data = data;
        }

        ImageGridItem(const ImageGridItem &o)
        {
            text = o.text;
            pixmap = o.pixmap;
            selected = o.selected;
            data = o.data;
        }

        ~ImageGridItem(void)
        {
            if (pixmap)
            {
                delete pixmap;
                pixmap = NULL;
            }
        }

        ImageGridItem& operator=(ImageGridItem *rhs)
        {
            text = rhs->text;
            pixmap = rhs->pixmap;
            selected = rhs->selected;
            data = rhs->data;
            return *this;
        }

        QString text;
        QPixmap *pixmap;
        bool selected;
        void *data;
};

class MPUBLIC UIImageGridType : public UIType
{
    Q_OBJECT

    public:

        UIImageGridType(const QString &name, int order);
        ~UIImageGridType(void);

        void setColumnCount(int count) { columnCount = count; }
        void setRowCount(int count) { rowCount = count; }
        void setItemCount(int count) { itemCount = count; }
        void setBorderWidth(int width){ borderWidth = width; }
        void setPadding(int value){ padding = value; }
        void setArea(QRect area){ displayRect = area; }
        void appendItem(ImageGridItem *item);
        void updateItem(int itemNo, ImageGridItem *item);
        void updateItem(ImageGridItem *item);
        void removeItem(int itemNo);
        void removeItem(ImageGridItem *item);
        void reset(void);
        void setJustification(int jst);
        void setCutDown(bool state) { cutdown = state; }
        void setActiveFont(fontProp *font) { this->activeFont = font; }
        void setInactiveFont(fontProp *font) { this->inactiveFont = font; }
        void setSelectedFont(fontProp *font) { this->selectedFont = font; }
        void setWindow(MythDialog *win) { window = win; }
        void setNormalImage(QString filename) { normalImage = filename; }
        void setSelectedImage(QString filename) { selectedImage = filename; }
        void setHighlightedImage(QString filename) { highlightedImage = filename; }
        void setDefaultImage(QString filename) { defaultImage = filename; }
        void setShowChecks(bool value) { showCheck = value; }
        void setShowScrollArrows(bool value) { showScrollArrows = value; }
        void setShowSelected(bool value) { showSelected = value; }

        QSize getImageItemSize();
        void  setCurrentPos(int pos);
        void  setCurrentPos(QString value);
        int   getCurrentPos() { return currentItem; }
        ImageGridItem *getCurrentItem(void);
        ImageGridItem *getItemAt(int pos);
        ImageGridItem *getTopItem(void) { return getItemAt(topRow * columnCount); }
        int   getItemCount(void) { return itemCount; }
        int   getVisibleCount(void) { return columnCount * rowCount; }
        int   getTopItemPos(void) { return topRow * columnCount; }

        void calculateScreenArea();

        enum textPosition
        {
            textPosTop,
            textPosBottom,
        };
        void setTextPosition(textPosition position) { textPos = position; }
        void setTextHeight(int height) { textHeight = height; }

        void recalculateLayout(void);

        void Draw(QPainter *p, int drawlayer, int context);
        bool handleKeyPress(QString action);


    signals:
        void itemChanged(ImageGridItem *item);
        void selectionChanged(ImageGridItem *item);

    private:
        void drawCell(QPainter *p, int curPos, int xpos, int ypos);
        void drawText(QPainter *p, int curPos, int xpos, int ypos);
        void loadImages(void);
        void loadCellImages(void);
        QPixmap *createScaledPixmap(QString filename, int width, int height,
                                    Qt::AspectRatioMode mode);
        MythDialog *window;
        int rowCount;
        int columnCount;
        int itemCount;
        int currentItem;
        int borderWidth;
        int padding;
        int cellWidth;
        int cellHeight;
        int topRow;
        int lastRow;
        int lastColumn;
        int curColumn;
        int curRow;

        QRect displayRect; // bounding rect of all imagegrid
        QRect imageRect;   // bounding rect of image item relative to cell rect
        QRect checkRect;   // bounding rect of check pixmap

        textPosition textPos;
        int textHeight;
        int justification;
        bool multilineText;
        fontProp *activeFont, *inactiveFont, *selectedFont;
        bool cutdown;
        bool showCheck;
        bool showScrollArrows;
        bool showSelected;
        bool showUpArrow;
        bool showDnArrow;
        QString normalImage;
        QString highlightedImage;
        QString selectedImage;
        QString defaultImage;

        QPixmap *normalPixmap;
        QPixmap *highlightedPixmap;
        QPixmap *selectedPixmap;
        QPixmap *defaultPixmap;

        QPixmap *checkNonPixmap;
        QPixmap *checkHalfPixmap;
        QPixmap *checkFullPixmap;

        QPixmap *upArrowRegPixmap;
        QPixmap *upArrowActPixmap;
        QPixmap *dnArrowRegPixmap;
        QPixmap *dnArrowActPixmap;

        QList<ImageGridItem*> *allData;
};

class MPUBLIC UIListType : public UIType
{
    Q_OBJECT

    public:
    UIListType(const QString &, QRect, int);
    ~UIListType();

    enum ItemArrows  { ARROW_NONE, ARROW_LEFT, ARROW_RIGHT, ARROW_BOTH };

    void SetCount(int cnt) { m_count = cnt;
                             if (m_count)
                                m_selheight = (int)(m_area.height() / m_count);
                             else
                                m_selheight = 0;
                           }


    void SetItemText(int, int, QString);
    void SetItemText(int, QString);
    void SetItemArrow(int, int);

    void SetActive(bool act) { m_active = act; }
    void SetItemCurrent(int cur) { m_current = cur; }

    void SetColumnWidth(int col, int width) { columnWidth[col] = width; }
    void SetColumnContext(int col, int context) { columnContext[col] = context; }
    void SetColumnPad(int pad) { m_pad = pad; }
    void SetImageLeftArrow(QPixmap img, QPoint loc) {
                           m_leftarrow = img; m_leftarrow_loc = loc; }
    void SetImageRightArrow(QPixmap img, QPoint loc) {
                           m_rightarrow = img; m_rightarrow_loc = loc;}
    void SetImageUpArrow(QPixmap img, QPoint loc) {
                         m_uparrow = img; m_uparrow_loc = loc; }
    void SetImageDownArrow(QPixmap img, QPoint loc) {
                           m_downarrow = img; m_downarrow_loc = loc; }
    void SetImageSelection(QPixmap img, QPoint loc) {
                           m_selection = img; m_selection_loc = loc; }

    void SetUpArrow(bool arrow) { m_uarrow = arrow; }
    void SetDownArrow(bool arrow) { m_darrow = arrow; }

    void SetFill(QRect area, QColor color, int type) {
                          m_fill_area = area; m_fill_color = color;
                          m_fill_type = type; }

    void SetFonts(QMap<QString, QString> fonts, QMap<QString, fontProp> fontfcn) {
                          m_fonts = fonts; m_fontfcns = fontfcn; }
    void EnableForcedFont(int num, QString func) { forceFonts[num] = m_fonts[func]; }

    int GetCurrentItem() { return m_current; }
    int GetItems() { return m_count; }
    QString GetItemText(int, int col = 1);
    void ResetList() { listData.clear(); forceFonts.clear(); listArrows.clear();
                       m_current = -1; m_columns = 0; }
    void ResetArrows() { listArrows.clear(); }
    void Draw(QPainter *, int drawlayer, int);
    bool ShowSelAlways() const { return m_showSelAlways; }
    void ShowSelAlways(bool bnew) { m_showSelAlways = bnew; }
    void calculateScreenArea();

  public slots:
    bool takeFocus();
    void looseFocus();

  private:
    //QString cutDown(QString, QFont *, int);
    int m_selheight;
    int m_justification;
    int m_columns;
    int m_current;
    bool m_active;
    int m_pad;
    int m_count;
    bool m_darrow;
    bool m_uarrow;
    bool m_showSelAlways;
    QRect m_fill_area;
    QColor m_fill_color;
    int m_fill_type;
    QPixmap m_selection;
    QPixmap m_downarrow;
    QPixmap m_uparrow;
    QPixmap m_leftarrow;
    QPixmap m_rightarrow;
    QPoint m_selection_loc;
    QPoint m_downarrow_loc;
    QPoint m_uparrow_loc;
    QPoint m_rightarrow_loc;
    QPoint m_leftarrow_loc;
    QRect m_area;
    QMap<QString, QString> m_fonts;
    QMap<QString, fontProp> m_fontfcns;
    QMap<int, QString> forceFonts;
    QMap<int, QString> listData;
    QMap<int, int> listArrows;
    QMap<int, int> columnWidth;
    QMap<int, int> columnContext;
};

class MPUBLIC UIImageType : public UIType
{
    Q_OBJECT

  public:
    UIImageType(const QString &, const QString &, int, QPoint);
    ~UIImageType();

    QPoint DisplayPos() { return m_displaypos; }
    void SetPosition(QPoint pos) { m_displaypos = pos; }

    void SetFlex(bool flex) { m_flex = flex; }
    void ResetFilename() { m_filename = orig_filename; };
    void SetImage(QString file) { m_filename = file; }
    void SetImage(const QPixmap &imgdata) { img = imgdata; m_show = true; }
    void SetSize(int x, int y) { m_force_x = x; m_force_y = y; }
    void SetSkip(int x, int y) { m_drop_x = x; m_drop_y = y; }

    void ResetImage() { img = QPixmap(); }
    void LoadImage();
    const QPixmap &GetImage() { return img; }
    QSize GetSize(bool scaled = false)
    {
        return scaled ?
                QSize(int(m_force_x * m_wmult), int(m_force_y * m_hmult)) :
                QSize(m_force_x, m_force_y);
    }

    virtual void Draw(QPainter *, int, int);

    const QString& GetImageFilename() const { return m_filename; }

  public slots:

    //
    //  We have to redefine this, as pixmaps
    //  are not of a fixed size. And it has to
    //  be virtual because RepeatedImage does
    //  it differently.
    //

    virtual void refresh();

  protected:

    QPoint  m_displaypos;
    QString orig_filename;
    QString m_filename;
    bool    m_isvalid;
    bool    m_flex;
    bool    m_show;
    int     m_drop_x;
    int     m_drop_y;
    int     m_force_x;
    int     m_force_y;
    QPixmap img;

};

class MPUBLIC UIRepeatedImageType : public UIImageType
{
    Q_OBJECT

  public:

    UIRepeatedImageType(const QString &, const QString &, int, QPoint);
    void setRepeat(int how_many);
    void Draw(QPainter *, int, int);
    void setOrientation(int x);

  public slots:

    void refresh();

  protected:

    int m_repeat;
    int m_highest_repeat;

  private:

    //  0 = left to right
    //  1 = right to left
    //  2 = bottom to top
    //  3 = top to bottom

    int m_orientation;

};


class MPUBLIC UITextType : public UIType
{
  public:
    UITextType(const QString &, fontProp *, const QString &, int,
                QRect displayrect, QRect altdisplayrect);
    ~UITextType();

    QString Name() { return m_name; }

    void UseAlternateArea(bool useAlt);

    void SetText(const QString &text);
    QString GetText() { return m_message; }
    QString GetDefaultText() { return m_default_msg; }

    void SetJustification(int jst) { m_justification = jst; }
    int GetJustification() { return m_justification; }
    void SetCutDown(bool cut) { m_cutdown = cut; }

    QRect DisplayArea() { return m_displaysize; }
    void SetDisplayArea(const QRect &rect) { m_displaysize = rect; }
    virtual void calculateScreenArea();

    virtual void Draw(QPainter *, int, int);

    fontProp *GetFont(void) { return m_font; }
    void SetFont(fontProp *font);

  protected:

    //QString cutDown(QString, QFont *, int, int);
    int m_justification;
    QRect m_displaysize;
    QRect m_origdisplaysize;
    QRect m_altdisplaysize;
    QString m_message;
    QString m_default_msg;

    fontProp *m_font;

    bool m_cutdown;

};

class MPUBLIC UIRemoteEditType : public UIType
{
    Q_OBJECT

  public:

    UIRemoteEditType(const QString &name, fontProp *font, const QString &text,
                     int dorder, QRect displayrect);
    ~UIRemoteEditType();

    void    createEdit(MythThemedDialog* parent);
    QWidget *getEdit(void) { return (QWidget*) edit; };
    void    Draw(QPainter *, int drawlayer, int context);
    void    setArea(QRect area){m_displaysize = area;}
    void    setText(const QString some_text);
    QString getText();
    void    setFont(fontProp *font);
    void    setCharacterColors(QColor unselected, QColor selected, QColor special);
    void    calculateScreenArea();

  public slots:
    void takeFocusAwayFromEditor(bool up_or_down);
    void editorChanged(QString value);
    virtual bool takeFocus();
    virtual void looseFocus();
    virtual void show();
    virtual void hide();

  signals:
    void    textChanged(QString value);

  private:
    MythRemoteLineEdit *edit;
    QRect    m_displaysize;
    QString  m_text;
    fontProp *m_font;
    QColor   m_unselected;
    QColor   m_selected;
    QColor   m_special;

    MythThemedDialog* m_parentDialog;
};

class MPUBLIC UIStatusBarType : public UIType
{
  public:
    UIStatusBarType(QString &, QPoint, int);
    ~UIStatusBarType();

    void SetUsed(int used) { m_used = used; refresh();}
    void SetTotal(int total) { m_total = total; }

    void SetLocation(QPoint loc) { m_location = loc; }
    void SetContainerImage(QPixmap img) { m_container = img; }
    void SetFillerImage(QPixmap img) { m_filler = img; }
    void SetFiller(int fill) { m_fillerSpace = fill; }
    void calculateScreenArea();
    void setOrientation(int x);

    void Draw(QPainter *, int drawlayer, int);

  private:

    int m_used;
    int m_total;
    int m_fillerSpace;

    QPixmap m_container;
    QPixmap m_filler;
    QPoint m_location;

    // 0 = left to right
    // 1 = left to right
    // 2 = left to right
    // 3 = left to right

    int m_orientation;

};

class MPUBLIC UIManagedTreeListType : public UIType
{
    Q_OBJECT

    //
    //  The idea here is a generalized notion of a list that
    //  shows underlying tree data with theme-ui configurable
    //  control over how much to display, where to display it,
    //  etc.
    //
    //  If you just want a straight list of data, you probably
    //  don't need this
    //
    typedef QMap <int, QRect> CornerMap;
    typedef QVector<int> IntVector;

  public:

    UIManagedTreeListType(const QString &name);
    ~UIManagedTreeListType();
    void    setUpArrowOffset(QPoint& pt) { upArrowOffset = pt;}
    void    setDownArrowOffset(QPoint& pt) { downArrowOffset = pt;}
    void    setLeftArrowOffset(QPoint& pt) {leftArrowOffset = pt;}
    void    setRightArrowOffset(QPoint& pt) {rightArrowOffset = pt;}
    void    setSelectPoint(QPoint& pt) { selectPoint = pt;}
    void    setSelectPadding(int pad) {selectPadding = pad;}
    void    setSelectScale(bool scale) {selectScale = scale;}
    void    setArea(QRect an_area) { area = an_area; }
    void    setBins(int l_bins) { bins = l_bins; }
    void    setBinAreas(CornerMap some_bin_corners) { bin_corners = some_bin_corners; }
    void    Draw(QPainter *, int drawlayer, int context);
    void    assignTreeData(GenericTree *a_tree);
    void    moveToNode(QList<int> route_of_branches);
    void    moveToNodesFirstChild(QList<int> route_of_branchs);
    QList<int>* getRouteToActive();
    QStringList getRouteToCurrent();
    bool    tryToSetActive(QList<int> route);
    bool    tryToSetCurrent(QStringList route);
    void    setHighlightImage(QPixmap an_image){highlight_image = an_image;}
    void    setArrowImages(QPixmap up, QPixmap down, QPixmap left, QPixmap right)
                          {
                            up_arrow_image = up;
                            down_arrow_image = down;
                            left_arrow_image = left;
                            right_arrow_image = right;
                          }
    void    addIcon(int i, QPixmap *img) { iconMap[i] = img; }
    void    setFonts(QMap<QString, QString> fonts, QMap<QString, fontProp> fontfcn) {
                          m_fonts = fonts; m_fontfcns = fontfcn; }
    void    drawText(QPainter *p, QString the_text, QString font_name, int x, int y, int bin_number, int icon_number);
    void    setJustification(int jst) { m_justification = jst; }
    int     getJustification() { return m_justification; }
    void    makeHighlights();
    void    syncCurrentWithActive();
    void    forceLastBin(){active_bin = bins; refresh();}
    void    calculateScreenArea();
    void    setTreeOrdering(int an_int){tree_order = an_int;}
    void    setVisualOrdering(int an_int){visual_order = an_int;}
    void    setIconSelector(int an_int){iconAttr = an_int;}
    void    showWholeTree(bool yes_or_no){ show_whole_tree = yes_or_no; }
    void    scrambleParents(bool yes_or_no){ scrambled_parents = yes_or_no; }
    void    colorSelectables(bool yes_or_no){color_selectables = yes_or_no; }
    void    sortTreeByString(){if (my_tree_data) my_tree_data->sortByString(); }
    void    sortTreeBySelectable(){if (my_tree_data) my_tree_data->sortBySelectable();}
    GenericTree *getCurrentNode() { return current_node; }
    GenericTree *getActiveNode() { return active_node; }
    void    setCurrentNode(GenericTree *a_node);
    int     getActiveBin();
    void    setActiveBin(int a_bin);

  public slots:

    bool    popUp();
    bool    pushDown();
    bool    moveUp(bool do_refresh = true);
    bool    moveUpByAmount(int number_up = 1, bool do_refresh = true);
    bool    moveDown(bool do_refresh = true);
    bool    moveDownByAmount(int number_down = 1, bool do_refresh = true);
    bool    pageUp();
    bool    pageDown();
    bool    nextActive(bool wrap_around, bool traverse_up_down);
    bool    prevActive(bool wrap_around, bool traverse_up_down);
    void    select();
    void    activate();
    void    enter();
    void    deactivate(){active_node = NULL;}
    bool    incSearchStart();
    bool    incSearchNext();

  signals:

    void    nodeSelected(int, IntVector*); //  emit int and attributes when user selects a node
    void    nodeEntered(int, IntVector*);  //  emit int and attributes when user navigates to node

  private:

    int   calculateEntriesInBin(int bin_number);
    bool  complexInternalNextPrevActive(bool forward_or_reverse, bool wrap_around);
    QRect area;
    int   bins;
    int   active_bin;

    CornerMap   bin_corners;
    CornerMap   screen_corners;
    GenericTree *my_tree_data;
    GenericTree *current_node;
    GenericTree *active_parent;
    GenericTree *active_node;
    int         tree_order;
    int         visual_order;
    int         iconAttr;
    int         selectPadding;
    bool        selectScale;

    QMap<QString, QString>  m_fonts;
    QMap<QString, fontProp> m_fontfcns;
    int                     m_justification;
    QPixmap                 highlight_image;
    QPixmap                 up_arrow_image;
    QPixmap                 down_arrow_image;
    QPixmap                 left_arrow_image;
    QPixmap                 right_arrow_image;
    QList<QPixmap*>         resized_highlight_images;
    QMap<int, QPixmap*>     highlight_map;
    QList<int>              route_to_active;
    bool                    show_whole_tree;
    bool                    scrambled_parents;
    bool                    color_selectables;
    QMap<int, QPixmap*>     iconMap;
    QPoint                  selectPoint;
    QPoint                  upArrowOffset;
    QPoint                  downArrowOffset;
    QPoint                  leftArrowOffset;
    QPoint                  rightArrowOffset;
    QString                 incSearch;
    bool                    bIncSearchContains;
};

class MPUBLIC UIPushButtonType : public UIType
{
    Q_OBJECT

  public:

    UIPushButtonType(const QString &name, QPixmap on, QPixmap off, QPixmap pushed, QPixmap pushedon=QPixmap());

    virtual void Draw(QPainter *, int drawlayer, int context);
    void    setPosition(QPoint pos){m_displaypos = pos;}
    virtual void calculateScreenArea();
    void    setLockOn();

  public slots:

    virtual void push();
    virtual void unPush();
    virtual void activate(){push();}

  signals:

    void    pushed();
    void    pushed(QString);

  protected:

    QPoint  m_displaypos;
    QPixmap on_pixmap;
    QPixmap off_pixmap;
    QPixmap pushed_pixmap;
    QPixmap pushedon_pixmap;
    bool    currently_pushed;
    QTimer  push_timer;
    bool    m_lockOn;

};

class MPUBLIC UITextButtonType : public UIType
{
    Q_OBJECT

  public:

    UITextButtonType(const QString &name, QPixmap on, QPixmap off, QPixmap pushed);

    void    Draw(QPainter *, int drawlayer, int context);
    void    setPosition(QPoint pos){m_displaypos = pos;}
    void    setText(const QString some_text);
    void    setFont(fontProp *font) { m_font = font; }
    void    calculateScreenArea();

  public slots:

    void    push();
    void    unPush();
    void    activate(){push();}

  signals:

    void    pushed();

  private:

    QPoint   m_displaypos;
    QPixmap  on_pixmap;
    QPixmap  off_pixmap;
    QPixmap  pushed_pixmap;
    QString  m_text;
    fontProp *m_font;
    bool     currently_pushed;
    QTimer   push_timer;

};

class MPUBLIC UICheckBoxType : public UIType
{
    Q_OBJECT

    //
    //  A simple, theme-able check box
    //

  public:

    UICheckBoxType(const QString &name,
                   QPixmap checkedp,
                   QPixmap uncheckedp,
                   QPixmap checked_highp,
                   QPixmap unchecked_highp);

    void    Draw(QPainter *, int drawlayer, int context);
    void    setPosition(QPoint pos){m_displaypos = pos;}
    void    calculateScreenArea();
    bool    getState(){return checked;}

  public slots:

    void    push();
    void    setState(bool checked_or_not);
    void    toggle(){push();}
    void    activate(){push();}

  signals:

    void    pushed(bool state);

  private:

    QPoint  m_displaypos;
    QPixmap checked_pixmap;
    QPixmap unchecked_pixmap;
    QPixmap checked_pixmap_high;
    QPixmap unchecked_pixmap_high;
    bool    checked;
    QString label;
};

class IntStringPair
{
    //  Minuscule class for holding data
    //  in a UISelectorType (below)

  public:

    IntStringPair(int an_int, const QString a_string){my_int = an_int; my_string = a_string;}

    void    setString(const QString &a_string){my_string = a_string;}
    void    setInt(int an_int){my_int = an_int;}
    QString getString(){return my_string;}
    int     getInt(){return my_int;}


  private:

    int     my_int;
    QString my_string;

};


class MPUBLIC UISelectorType : public UIPushButtonType
{
    Q_OBJECT

    //
    //  A theme-able "thingy" (that's the
    //  official term) that can hold a list
    //  of strings and emit an int and/or
    //  the string when the user selects
    //  from among them.
    //

  public:

    UISelectorType(const QString &name, QPixmap on, QPixmap off, QPixmap pushed, QRect area);
    ~UISelectorType();

    void    Draw(QPainter *, int drawlayer, int context);
    void    calculateScreenArea();
    void    addItem(int an_int, const QString &a_string);
    void    setFont(fontProp *font) { m_font = font; }
    QString getCurrentString();
    int     getCurrentInt();

  public slots:

    void push(bool up_or_down);
    void unPush();
    void activate(){push(true);}
    void cleanOut(){current_data = NULL; my_data.clear();}
    void setToItem(int which_item);
    void setToItem(const QString &which_item);

  signals:

    void    pushed(int);

  private:

    QRect                   m_area;
    fontProp                *m_font;
    QList<IntStringPair*>    my_data;
    IntStringPair           *current_data;

};

class MPUBLIC UIBlackHoleType : public UIType
{
    Q_OBJECT

    //
    //  This just holds a blank area of the screen
    //  originally designed to "hold" the place where
    //  visualizers go in mythmusic playback
    //

  public:

    UIBlackHoleType(const QString &name);
    void calculateScreenArea();
    void setArea(QRect an_area) { area = an_area; }
    virtual void Draw(QPainter *, int, int){}

  protected:

    QRect area;
};

class MPUBLIC UIKeyType : public UIType
{
    Q_OBJECT

  public:
    UIKeyType(const QString &);
    ~UIKeyType();

    QPoint GetPosition() { return m_pos; }

    void SetArea(QRect &area) { m_area = area; }
    void SetPosition(QPoint pos) { m_pos = pos; }
    void SetImages(QPixmap *normal, QPixmap *focused, QPixmap *down,
                   QPixmap *downFocused);
    void SetDefaultImages(QPixmap *normal, QPixmap *focused, QPixmap *down,
                          QPixmap *downFocused);

    void SetFonts(fontProp *normal, fontProp *focused, fontProp *down,
                  fontProp *downFocused);
    void SetDefaultFonts(fontProp *normal, fontProp *focused, fontProp *down,
                         fontProp *downFocused);

    void    SetType(QString type) { m_type = type; }
    QString GetType(void) const { return m_type; }

    void    SetChars(QString normal, QString shift, QString alt, QString shiftAlt);
    QString GetChar();

    void    SetMoves(QString moveLeft, QString moveRight, QString moveUp,
                  QString moveDown);
    QString GetMove(QString direction);

    void SetShiftState(bool sh, bool ag);
    void SetOn(bool bOn) { m_bDown = bOn; refresh(); }
    bool IsOn(void) { return m_bDown; }

    void SetToggleKey(bool bOn) { m_bToggle = bOn; }
    bool IsToggleKey(void) { return m_bToggle; }

   virtual void Draw(QPainter *, int, int);
    virtual void calculateScreenArea();

  public slots:
    void    push();
    void    unPush();
    void    activate(){push();}

  signals:
    void    pushed();

  private:
    QString decodeChar(QString c);

    QRect   m_area;
    QString m_type;

    QPixmap *m_normalImg;
    QPixmap *m_focusedImg;
    QPixmap *m_downImg;
    QPixmap *m_downFocusedImg;

    fontProp *m_normalFont;
    fontProp *m_focusedFont;
    fontProp *m_downFont;
    fontProp *m_downFocusedFont;

    QPoint m_pos;

    QString m_normalChar;
    QString m_shiftChar;
    QString m_altChar;
    QString m_shiftAltChar;

    QString m_moveLeft;
    QString m_moveRight;
    QString m_moveUp;
    QString m_moveDown;

    bool    m_bShift;
    bool    m_bAlt;
    bool    m_bDown;
    bool    m_bToggle;

    bool    m_bPushed;
    QTimer  m_pushTimer;
};

class MPUBLIC UIKeyboardType : public UIType
{
    Q_OBJECT

  public:
    UIKeyboardType(const QString &, int);
    ~UIKeyboardType();

    typedef QList<UIKeyType*> KeyList;

    void SetContainer(LayerSet *container) { m_container = container; }
    void SetArea(QRect &area) { m_area = area; }
    void SetEdit(QWidget* edit) { m_parentEdit = edit; }
    void SetParentDialog(MythThemedDialog * parentDialog)
            { m_parentDialog = parentDialog; }

    KeyList GetKeys() { return m_keyList; }
    void AddKey(UIKeyType *key);

    virtual void Draw(QPainter *, int, int);
    virtual void calculateScreenArea();
    virtual void keyPressEvent(QKeyEvent *e);

  private slots:
    void charKey();
    void lockOnOff();
    void shiftLOnOff();
    void shiftROnOff();
    void shiftOff();
    void altGrOnOff();
    void compOnOff();
    void updateButtons();
    void leftCursor();
    void rightCursor();
    void backspaceKey();
    void delKey();
    void close();

  private:
    void init();
    void insertChar(QString c);
    void moveUp();
    void moveDown();
    void moveLeft();
    void moveRight();
    UIKeyType *findKey(QString keyName);

    QRect     m_area;

    bool m_bInitalized;

    bool      m_bCompTrap;
    QString   m_comp1;

    UIKeyType *m_altKey;
    UIKeyType *m_lockKey;
    UIKeyType *m_shiftLKey;
    UIKeyType *m_shiftRKey;
    UIKeyType *m_focusedKey;
    UIKeyType *m_doneKey;

    QWidget          *m_parentEdit;
    MythThemedDialog *m_parentDialog;

    LayerSet  *m_container;
    KeyList   m_keyList;
};

#endif
