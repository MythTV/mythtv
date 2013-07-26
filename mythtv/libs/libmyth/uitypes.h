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
#include "mythdate.h"
#include "mythdialogs.h"
#include "mythwidgets.h"
#include "mythfontproperties.h"

#ifdef _WIN32
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

    void    SetDrawFontShadow(bool state);


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
