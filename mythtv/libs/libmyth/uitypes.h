#ifndef UIDATATYPES_H_
#define UIDATATYPES_H_

#include <qobject.h>
#include <qstring.h>
#include <qstringlist.h>
#include <qrect.h>
#include <qfile.h>
#include <qmap.h>
#include <vector>
#include <qfont.h>
#include <qpixmap.h>
#include <qpainter.h>
using namespace std;

class UIType;

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
 
    void Draw(QPainter *, int, int);

    QString GetName() { return m_name; }
    void SetName(const QString &name) { m_name = name; }

    void SetDrawOrder(int order) { m_order = order; }
    int GetDrawOrder() const { return m_order; }

    void SetAreaRect(QRect area) { m_area = area; }
    QRect GetAreaRect() { return m_area; }

    void SetContext(int con) { m_context = con; }
    int GetContext(void) { return m_context; }

    void SetDebug(bool db) { m_debug = db; }

    void AddType(UIType *);
    UIType *GetType(const QString &name);

  private:
    bool m_debug;
    int m_context;
    int m_order;
    QString m_name;
    QRect m_area;

    QMap<QString, UIType *> typeList;
    vector<UIType *> *allTypes;
};

class UIType : public QObject
{
  public:
    UIType(const QString &name);
    virtual ~UIType();

    void SetParent(LayerSet *);
    void SetScreen(double wmult, double hmult) { m_wmult = wmult; m_hmult = hmult; }
    void SetContext(int con) { m_context = con; }
    void SetDebug(bool db) { m_debug = db; }

    virtual void Draw(QPainter *, int, int);

    QString Name();

  protected:
    double m_wmult;
    double m_hmult;
    int m_context;
    int m_order;
    bool m_debug;
    QString m_name;
    LayerSet *m_parent;
};

class UIFontPairType
{
  public:
    UIFontPairType(fontProp *, fontProp *);
    ~UIFontPairType();
 
    fontProp *getActive() { return m_activeFont; }
    fontProp *getInactive() { return m_inactiveFont; }

  private:
    fontProp *m_activeFont;
    fontProp *m_inactiveFont;
};

class UIListType : public UIType
{
  public:
    UIListType(const QString &, QRect, int);
    ~UIListType();
  
    void SetOrder(int order) { m_order = order; }
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
    QString cutDown(QString, QFont *, int);
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
    QString m_name;
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
  public:
    UIImageType(const QString &, const QString &, int, QPoint);
    ~UIImageType();

    QPoint DisplayPos() { return m_displaypos; }
    void SetPosition(QPoint pos) { m_displaypos = pos; }
  
    void SetOrder(int order) { m_order = order; }
    void SetFlex(bool flex) { m_flex = flex; }
    void SetImage(QString file) { m_filename = file; }
    void SetSize(int x, int y) { m_force_x = x; m_force_y = y; }
    void SetSkip(int x, int y) { m_drop_x = x; m_drop_y = y; }

    void LoadImage();
    QPixmap GetImage() { return img; }
  
    void Draw(QPainter *, int, int);

  protected:
    QPoint m_displaypos;
    QString m_filename;
    bool m_isvalid;
    bool m_flex;
    int m_drop_x;
    int m_drop_y;
    int m_force_x;
    int m_force_y;
    QPixmap img;

};

class UITextType : public UIType
{
  public:
    UITextType(const QString &, fontProp *, const QString &, int,
                QRect displayrect);
    ~UITextType();

    void SetText(const QString &text);
    QString GetText() { return m_message; }

    void SetJustification(int jst) { m_justification = jst; }
    int GetJustification() { return m_justification; }

    QRect DisplayArea() { return m_displaysize; }

    void Draw(QPainter *, int, int);

  private:
    QString cutDown(QString, QFont *, int);
    int m_justification;
    QRect m_displaysize;
    QString m_message;

    fontProp *m_font;

    bool m_centered;
    bool m_right;

};

class UIStatusBarType : public UIType
{
  public:
    UIStatusBarType(QString &, QPoint, int);
    ~UIStatusBarType();

    void SetUsed(int used) { m_used = used; }
    void SetTotal(int total) { m_total = total; }

    void SetLocation(QPoint loc) { m_location = loc; }
    void SetContainerImage(QPixmap img) { m_container = img; }
    void SetFillerImage(QPixmap img) { m_filler = img; }
    void SetFiller(int fill) { m_fillerSpace = fill; }

    void Draw(QPainter *, int drawlayer, int);

  private:
    int m_order;
    int m_used;
    int m_total;
    int m_fillerSpace;

    QPixmap m_container;
    QPixmap m_filler;
    QPoint m_location;
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

  public:
  
    GenericTree();
    GenericTree(const QString a_string);
    GenericTree(int an_int);
    ~GenericTree();
    
    GenericTree* addNode(QString a_string);
    GenericTree* addNode(QString a_string, int an_int);
    void         init();
    void         setInt(int an_int){my_int = an_int;}
    void         setParent(GenericTree* a_parent){my_parent = a_parent;}
    QString      getString(){return my_string;}

  private:
  
    QString                my_string;
    QStringList            my_stringlist;
    int                    my_int;
    QPtrList <GenericTree> my_subnodes;
    GenericTree*           my_parent;
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

    

  public:

    UIManagedTreeListType(const QString &name);
    ~UIManagedTreeListType();

    void setArea(QRect an_area){area = an_area;}
    void setBins(int l_bins){bins = l_bins;}
    void setBinAreas(CornerMap some_bin_corners){bin_corners = some_bin_corners;}
    void Draw(QPainter *, int drawlayer, int context);
    void assignTreeData(GenericTree *a_tree);

  public slots:
  
    void sayHelloWorld(); // debugging;

  signals:
  
    void leafNodeSelected(QString node_string, int node_int);

  private:
    
    QRect area;
    int   bins;
    
    CornerMap   bin_corners;
    GenericTree *my_tree_data;
};

#endif
