#ifndef UIDATATYPES_H_
#define UIDATATYPES_H_

#include <qobject.h>
#include <qstring.h>
#include <qregexp.h>
#include <qstringlist.h>
#include <qrect.h>
#include <qfile.h>
#include <qmap.h>
#include <vector>
#include <qvaluevector.h>
#include <qfont.h>
#include <qpixmap.h>
#include <qpainter.h>
#include <qptrlist.h>

#include "mythwidgets.h"
#include "util.h"
#include "mythdialogs.h"

using namespace std;

class UIType;
class MythDialog;

struct fontProp {
    QFont face;
    QPoint shadowOffset;
    QColor color;
    QColor dropColor;
};

class LayerSet
{
  public:
    LayerSet(const QString &name);
   ~LayerSet();
 
    void    Draw(QPainter *, int, int);

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
    void    SetTextByRegexp(QMap<QString, QString> &regexpMap);

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

class UIType : public QObject
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
    virtual void calculateScreenArea();
    QRect   getScreenArea(){return screen_area;}
    QString cutDown(const QString &data, QFont *font, bool multiline = false, 
                    int overload_width = -1, int overload_height = -1);
    QString getName(){return m_name;}

    
  public slots:
  
    virtual bool takeFocus();
    virtual void looseFocus();
    virtual void activate(){}
    virtual void refresh();

  signals:
  
    //
    //  Ask my container to ask Qt to repaint
    //

    void requestUpdate();
    void requestUpdate(const QRect &);
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
};

class UIBarType : public UIType
{
  public:
    UIBarType(const QString &name, QString, int, QRect);
    ~UIBarType();

    void SetIcon(int, QPixmap);
    void SetText(int, QString);
    void SetIcon(int, QString);

    void Draw(QPainter *, int, int);

    void SetJustification(int jst) { m_justification = jst; }
    void SetSize(int size) { m_size = size; LoadImage(); }
    void SetScreen(double w, double h) { m_wmult = w; m_hmult = h; }
    void SetFont(fontProp *font) { m_font = font; }
    void SetOrientation(int ori) { m_orientation = ori; }
    void SetTextOffset(QPoint to) { m_textoffset = to; }
    void SetIconOffset(QPoint ic) { m_iconoffset = ic; }
    void SetIconSize(QPoint is) { m_iconsize = is; }
    void ResetImage(int loc) { iconData[loc].resize(0, 0); }
    int GetSize() { return m_iconsize.x(); }

  private:
    void LoadImage(int loc = -1, QString dat = "");
    QRect m_displaysize;
    QPoint m_iconsize;
    QPoint m_textoffset;
    QPoint m_iconoffset;
    int m_justification;
    int m_orientation;
    int m_size;
    fontProp *m_font;
    QString m_filename;
    QPixmap m_image;
    QMap<int, QString> textData;
    QMap<int, QPixmap> iconData;

};

class AlphaTable
{
  public:
    AlphaTable();
    ~AlphaTable();
    AlphaTable(const QColor &color, int alpha);
   
    unsigned char r[256], g[256], b[256];
  
  private:  
    void maketable(unsigned char* data, int channel, int alpha);
};

class AlphaBlender
{
  public:
    AlphaBlender();
    ~AlphaBlender();
    void init(int alpha = 96, int cacheSize = 30);
    void addColor(const QColor &color);
    void blendImage(const QImage &image, const QColor &color);
  
  private:
    QDict<AlphaTable> alphaTables;
    int alpha;
};

class UIGuideType : public UIType
{
  public:
    UIGuideType(const QString &name, int order);
    ~UIGuideType();

    enum FillType { Alpha = 10, Dense, Eco, Solid };

    void Draw(QPainter *dr, int drawlayer, int context);

    void SetJustification(int jst);
    void SetCutDown(bool state) { cutdown = state; }
    void SetFont(fontProp *font) { this->font = font; }
    void SetFillType(int type) { filltype = type; }
    void SetShowCategoryColors(bool state) { drawCategoryColors = state; }
    void SetShowCategoryText(bool state) { drawCategoryText = state; }
    void SetSelectorType(int type) { seltype = type; }
    void SetNumRows(int numRows) { this->numRows = numRows; }
    void SetWindow(MythDialog *win) { window = win; }
    
    void SetRecordingColors(const QString &reccol, const QString &concol) 
                  { reccolor = QColor(reccol); concolor = QColor(concol); }
    void SetSelectorColor(const QString &col) { selcolor = QColor(col); }
    void SetSolidColor(const QString &col) { solidcolor = QColor(col); }
    void SetCategoryColors(const QMap<QString, QString> &catColors);
    
    void SetArea(const QRect &area) { this->area = area; }
    void SetScreenLocation(const QPoint &sl) { screenloc = sl; }
    void SetTextOffset(const QPoint &to) { textoffset = to; }
    void SetArrow(int, const QString &file);
    void LoadImage(int, const QString &file);
    void SetProgramInfo(int row, int col, const QRect &area,
                        const QString &title, const QString &category,
                        int arrow, int recType, int recStat, bool selected);
    void ResetData();
    void ResetRow(int row);

  private:

    class UIGTCon
    {
      public:
        UIGTCon() { arrow = recType = recStat = 0; };
        UIGTCon(const QRect &drawArea, const QString &title, 
                const QString &category, int arrow, int recType, int recStat)
        {
            this->drawArea = drawArea;
            this->title = title;
            this->category = category.stripWhiteSpace();
            this->arrow = arrow;
            this->recType = recType;
            this->recStat = recStat;
        }

        UIGTCon(const UIGTCon &o)
        {
            drawArea = o.drawArea;
            title = o.title;
            category = o.category;
            categoryColor = o.categoryColor;
            arrow = o.arrow;
            recType = o.recType;
            recStat = o.recStat;
        }        
        
        QRect drawArea;
        QString title;
        QString category;
        QColor categoryColor;
        int arrow;
        int recType;
        int recStat;
    };
    
    void drawBackground(QPainter *dr, UIGTCon *data); 
    void drawBox(QPainter *dr, UIGTCon *data, const QColor &color);
    void drawText(QPainter *dr, UIGTCon *data);
    void drawRecType(QPainter *dr, UIGTCon *data);
    void drawCurrent(QPainter *dr, UIGTCon *data);
    
    QPtrList<UIGTCon> *allData;
    UIGTCon selectedItem;
    
    QPixmap recImages[15];
    QPixmap arrowImages[15];
    
    int maxRows;
    int numRows;
    
    MythDialog *window;
    QRect area;
    QPoint textoffset;
    QPoint screenloc;

    int justification;
    bool multilineText;
    fontProp *font;
    QColor solidcolor;
    
    QColor selcolor;
    int seltype;
    
    QColor reccolor;
    QColor concolor;
    
    int filltype;
    bool cutdown;
    bool drawCategoryColors;
    bool drawCategoryText;

    QMap<QString, QColor> categoryColors;
    AlphaBlender alphaBlender;
};

class UIListType : public UIType
{
  public:
    UIListType(const QString &, QRect, int);
    ~UIListType();
  
    void SetCount(int cnt) { m_count = cnt; 
                             m_selheight = (int)(m_area.height() / m_count); }

    void SetItemText(int, int, QString);
    void SetItemText(int, QString);
 
    void SetActive(bool act) { m_active = act; }
    void SetItemCurrent(int cur) { m_current = cur; }

    void SetColumnWidth(int col, int width) { columnWidth[col] = width; }
    void SetColumnContext(int col, int context) { columnContext[col] = context; }
    void SetColumnPad(int pad) { m_pad = pad; }

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

    int GetItems() { return m_count; }
    QString GetItemText(int, int col = 1);
    void ResetList() { listData.clear(); forceFonts.clear(); 
                       m_current = -1; m_columns = 0; }
 
    void Draw(QPainter *, int drawlayer, int);

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
    QRect m_fill_area;
    QColor m_fill_color;
    int m_fill_type;
    QPixmap m_selection;
    QPixmap m_downarrow;
    QPixmap m_uparrow;
    QPoint m_selection_loc;
    QPoint m_downarrow_loc;
    QPoint m_uparrow_loc;
    QRect m_area;
    QMap<QString, QString> m_fonts;
    QMap<QString, fontProp> m_fontfcns;
    QMap<int, QString> forceFonts;
    QMap<int, QString> listData;
    QMap<int, int> columnWidth;
    QMap<int, int> columnContext;
};

class UIImageType : public UIType
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
    void SetImage(QPixmap imgdata) { img = imgdata; }
    void SetSize(int x, int y) { m_force_x = x; m_force_y = y; }
    void SetSkip(int x, int y) { m_drop_x = x; m_drop_y = y; }

    void ResetImage() { img = QPixmap(); }
    void LoadImage();
    QPixmap GetImage() { return img; }
    int GetSize() { return m_force_x; }
    virtual void Draw(QPainter *, int, int);

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

class UIRepeatedImageType : public UIImageType
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


class UITextType : public UIType
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
    virtual void calculateScreenArea();

    void Draw(QPainter *, int, int);

  private:
    //QString cutDown(QString, QFont *, int, int);
    int m_justification;
    QRect m_displaysize;
    QRect m_origdisplaysize;
    QRect m_altdisplaysize;
    QString m_message;
    QString m_default_msg;
    QString m_name;

    fontProp *m_font;

    bool m_cutdown;

};

class UIStatusBarType : public UIType
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

class GenericTree
{
    //
    //  This is used by the UIType below
    //  (UIManagedTreeListType) to fill it
    //  with arbitrary simple data types and
    //  allow that object to fire SIGNALS
    //  when nodes, leafnodes, etc. are reached
    //  by the user
    //
    typedef QValueVector<int> IntVector;

  public:

    GenericTree();
    GenericTree(const QString a_string);
    GenericTree(QString a_string, int an_int);
    GenericTree(QString a_string, int an_int, bool selectable_flag);
    ~GenericTree();

    GenericTree*  addNode(QString a_string);
    GenericTree*  addNode(QString a_string, int an_int);
    GenericTree*  addNode(QString a_string, int an_int, bool selectable_flag);
    GenericTree*  findLeaf();
    GenericTree*  findLeaf(int ordering_index);
    GenericTree*  findNode(QValueList<int> route_of_branches);
    GenericTree*  recursiveNodeFinder(QValueList<int> route_of_branches);
    bool          checkNode(QValueList<int> route_of_branches);
    GenericTree*  nextSibling(int number_down);
    GenericTree*  nextSibling(int number_down, int ordering_index);
    GenericTree*  prevSibling(int number_up);
    GenericTree*  prevSibling(int number_up, int ordering_index);
    GenericTree*  getParent();
    GenericTree*  getChildAt(uint reference);
    GenericTree*  getSelectedChild(int ordering_index);
    GenericTree*  getChildAt(uint reference, int ordering_index);
    int           getChildPosition(GenericTree *which_child);
    int           getChildPosition(GenericTree *which_child, int ordering_index);
    int           getPosition();
    int           getPosition(int ordering_index);
    void          init();
    void          setInt(int an_int){my_int = an_int;}
    int           getInt(){return my_int;}
    void          setParent(GenericTree* a_parent){my_parent = a_parent;}
    const QString getString(){return my_string;}
    void          printTree(int margin);    // debugging
    void          printTree(){printTree(0);}// debugging
    int           calculateDepth(int start);
    int           childCount(){return my_subnodes.count();}
    int           siblingCount();
    void          setSelectable(bool flag){selectable = flag;}
    bool          isSelectable(){return selectable;}
    void          setAttribute(uint attribute_position, int value_of_attribute);
    int           getAttribute(uint which_one);
    IntVector*    getAttributes(){return my_attributes;}
    void          reorderSubnodes(int ordering_index);
    void          setOrderingIndex(int ordering_index){current_ordering_index = ordering_index;}
    int           getOrderingIndex(){return current_ordering_index;}
    void          becomeSelectedChild();
    void          setSelectedChild(GenericTree* a_node){my_selected_subnode = a_node;}
    void          addYourselfIfSelectable(QPtrList<GenericTree> *flat_list);
    void          buildFlatListOfSubnodes(int ordering_index, bool scrambled_parents);
    GenericTree*  nextPrevFromFlatList(bool forward_or_back, bool wrap_around, GenericTree *active);
    GenericTree*  getChildByName(QString a_name);
    void          sortByString();
    void          sortBySelectable();
    void          deleteAllChildren();

  private:

    QString               my_string;
    QStringList           my_stringlist;
    int                   my_int;
    QPtrList<GenericTree> my_subnodes;
    QPtrList<GenericTree> my_ordered_subnodes;
    QPtrList<GenericTree> my_flatened_subnodes;
    GenericTree*          my_selected_subnode;
    IntVector             *my_attributes;
    GenericTree*          my_parent;
    bool                  selectable;
    int                   current_ordering_index;
};

class UIManagedTreeListType : public UIType
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
    typedef QValueVector<int> IntVector;

  public:

    UIManagedTreeListType(const QString &name);
    ~UIManagedTreeListType();

    void    setArea(QRect an_area) { area = an_area; }
    void    setBins(int l_bins) { bins = l_bins; }
    void    setBinAreas(CornerMap some_bin_corners) { bin_corners = some_bin_corners; }
    void    Draw(QPainter *, int drawlayer, int context);
    void    assignTreeData(GenericTree *a_tree);
    void    moveToNode(QValueList<int> route_of_branches);
    void    moveToNodesFirstChild(QValueList<int> route_of_branchs);
    QValueList <int> * getRouteToActive();
    bool    tryToSetActive(QValueList <int> route);
    void    setHighlightImage(QPixmap an_image){highlight_image = an_image;}
    void    setArrowImages(QPixmap up, QPixmap down, QPixmap left, QPixmap right)
                          {
                            up_arrow_image = up;
                            down_arrow_image = down;
                            left_arrow_image = left;
                            right_arrow_image = right;
                          }
    void    setFonts(QMap<QString, QString> fonts, QMap<QString, fontProp> fontfcn) { 
                          m_fonts = fonts; m_fontfcns = fontfcn; }
    void    drawText(QPainter *p, QString the_text, QString font_name, int x, int y, int bin_number);
    void    setJustification(int jst) { m_justification = jst; }
    int     getJustification() { return m_justification; }
    void    makeHighlights();
    void    syncCurrentWithActive();
    void    forceLastBin(){active_bin = bins; refresh();}
    void    calculateScreenArea();
    void    setTreeOrdering(int an_int){tree_order = an_int;}
    void    setVisualOrdering(int an_int){visual_order = an_int;}    
    void    showWholeTree(bool yes_or_no){ show_whole_tree = yes_or_no; }
    void    scrambleParents(bool yes_or_no){ scrambled_parents = yes_or_no; }
    void    colorSelectables(bool yes_or_no){color_selectables = yes_or_no; }
    void    sortTreeByString(){if(my_tree_data) my_tree_data->sortByString(); }
    void    sortTreeBySelectable(){if(my_tree_data) my_tree_data->sortBySelectable();}
    GenericTree *getCurrentNode() { return current_node; }
    void    setCurrentNode(GenericTree *a_node);

  public slots:

    bool    popUp();
    bool    pushDown();
    bool    moveUp(bool do_refresh = true);
    bool    moveDown(bool do_refresh = true);
    bool    pageUp();
    bool    pageDown();
    bool    nextActive(bool wrap_around, bool traverse_up_down);
    bool    prevActive(bool wrap_around, bool traverse_up_down);
    void    select();
    void    activate();
    void    enter();
    void    deactivate(){active_node = NULL;}
    
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

    QMap<QString, QString>  m_fonts;
    QMap<QString, fontProp> m_fontfcns;
    int                     m_justification;
    QPixmap                 highlight_image;
    QPixmap                 up_arrow_image;
    QPixmap                 down_arrow_image;
    QPixmap                 left_arrow_image;
    QPixmap                 right_arrow_image;
    QPtrList<QPixmap>       resized_highlight_images;
    QMap<int, QPixmap*>     highlight_map;
    QValueList <int>        route_to_active;
    bool                    show_whole_tree;
    bool                    scrambled_parents;
    bool                    color_selectables;

};

class UIPushButtonType : public UIType
{
    Q_OBJECT
    
  public:
    
    UIPushButtonType(const QString &name, QPixmap on, QPixmap off, QPixmap pushed);

    virtual void Draw(QPainter *, int drawlayer, int context);
    void    setPosition(QPoint pos){m_displaypos = pos;}
    virtual void calculateScreenArea();
    
  public slots:  
  
    virtual void push();
    virtual void unPush();
    virtual void activate(){push();}
 
  signals:
  
    void    pushed();
  
  protected:
  
    QPoint  m_displaypos;
    QPixmap on_pixmap;
    QPixmap off_pixmap;
    QPixmap pushed_pixmap;
    bool    currently_pushed;
    QTimer  push_timer;
    
};

class UITextButtonType : public UIType
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



class UICheckBoxType : public UIType
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
    //  Miniscule class for holding data
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


class UISelectorType : public UIPushButtonType
{
    Q_OBJECT
    
    //
    //  A theme-able "thingy" (that's the 
    //  offical term) that can hold a list
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
    
  public slots:  
  
    void push(bool up_or_down);
    void unPush();
    void activate(){push(true);}
    void cleanOut(){current_data = NULL; my_data.clear();}
    void setToItem(int which_item);
 
  signals:
  
    void    pushed(int);
  
  private:
  
    QRect                   m_area;
    fontProp                *m_font;
    QPtrList<IntStringPair> my_data;
    IntStringPair           *current_data;

};




class UIBlackHoleType : public UIType
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

// ********************************************************************

class UIListBtnType : public UIType
{
    Q_OBJECT

  public:

    typedef struct {
        QString text;
        bool    checked;
    } UIListBtnTypeItem;
    
    UIListBtnType(const QString& name, const QRect& area, int order,
                        bool showArrow=true, bool showScrollArrows=false,
                        bool showChecked=false);
    ~UIListBtnType();

    void SetFontActive(fontProp *font);
    void SetFontInactive(fontProp *font);
    
    void SetSpacing(int spacing) {m_itemSpacing = spacing;}
    void SetMargin(int margin) {m_itemMargin = margin;}
    void SetItemRegColor(const QColor& beg, const QColor& end, uint alpha);
    void SetItemSelColor(const QColor& beg, const QColor& end, uint alpha);
    void Draw(QPainter *, int order, int);

    void SetActive(bool active) {m_active = active;}

    void Reset();
    void AddItem(const QString& text, bool checked = false);
    void SetItemCurrent(int current);
    int  GetItemCurrent();
    int  GetCount();
    const UIListBtnTypeItem* GetItem(int itemPos);
    void SetItemChecked(int itemPos, bool checked);

    void MoveDown();
    void MoveUp();
    void Toggle();

  private:

    void Init();
    void LoadPixmap(QPixmap& pix, const QString& fileName);

    int   m_order;
    QRect m_rect;
    QRect m_contentsRect;
    QRect m_arrowsRect;
    QRect m_itemTextRect;
    QRect m_itemCheckRect;
    QRect m_itemArrowRect;
    
    int   m_itemHeight;
    int   m_itemSpacing;
    int   m_itemMargin;
    uint  m_itemsVisible;

    bool  m_active;
    bool  m_showScrollArrows;
    bool  m_showArrow;
    bool  m_showChecked;
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
    QPixmap   m_checkPix;

    QColor    m_itemRegBeg;
    QColor    m_itemRegEnd;
    QColor    m_itemSelBeg;
    QColor    m_itemSelEnd;
    uint      m_itemRegAlpha;
    uint      m_itemSelAlpha;

    fontProp* m_fontActive;
    fontProp* m_fontInactive;

    bool      m_initialized;

    UIListBtnTypeItem* m_topItem;
    UIListBtnTypeItem* m_selItem;
    QPtrList<UIListBtnTypeItem> m_itemList;
    
  signals:

    void itemSelected(int itemPos);
    void itemChecked(int itemPos, bool checked);
};

#endif
