#ifndef UIDATATYPES_H_
#define UIDATATYPES_H_

#include <qstring.h>
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

class UIType
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
    UIListType(const QString &, QRect, int, UIFontPairType *);
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
 
    void SetForceColor(QString color) { m_forcecolor = color; }
    void EnableForcedColor(int num) { forceColors[num] = m_forcecolor; }
    void EnableForcedColor(int num, QString color) { forceColors[num] = color; }

    int GetItems() { return m_count; }
    void ResetList() { listData.clear(); forceColors.clear(); 
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
    QPixmap m_selection;
    QPixmap m_downarrow;
    QPixmap m_uparrow;
    QPoint m_selection_loc;
    QPoint m_downarrow_loc;
    QPoint m_uparrow_loc;
    QString m_name;
    QRect m_area;
    UIFontPairType *m_text;
    QString m_forcecolor;
    QMap<int, QString> forceColors;
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

    void LoadImage();
    QPixmap GetImage() { return img; }
  
    void Draw(QPainter *, int, int);

  protected:
    QPoint m_displaypos;
    QString m_filename;
    bool m_isvalid;
    bool m_flex;
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

#endif
