#ifndef THEMEDMENU_H_
#define THEMEDMENU_H_

#include <qvaluelist.h>
#include <qdom.h>
#include <qmap.h>

#include "mythdialogs.h"

#include <vector>

using namespace std;

class QPixmap;
class QImage;
class QWidget;

struct ButtonIcon
{
    QString name;
    QImage *icon;
    QImage *activeicon;
    QImage *watermark;
    QPoint offset;
};

struct ThemedButton
{
    QPoint pos;
    QRect  posRect;
    
    ButtonIcon *buttonicon;
    QPoint iconPos;
    QRect iconRect;

    QString text;
    QString altText;
    QString action;

    int row;
    int col;

    int status;
    bool visible;
};

struct MenuRow
{
    int numitems;
    bool visible;
    vector<ThemedButton *> buttons;
};

struct MenuState
{
    QString name;
    int row;
    int col;
};

class ThemedMenu : public MythDialog
{
    Q_OBJECT
  public:
    ThemedMenu(const char *cdir, const char *menufile, 
               MythMainWindow *parent, const char *name = 0);
   ~ThemedMenu();

    bool foundTheme() { return foundtheme; }

    void setCallback(void (*lcallback)(void *, QString &), void *data) 
                                        { callback = lcallback;
                                          callbackdata = data;
                                        }
    void setKillable(void) { killable = true; }

    QString getSelection() { return selection; }

    void ReloadTheme(void);
    void ReloadExitKey(void);

  protected:
    void paintEvent(QPaintEvent *e);
    void keyPressEvent(QKeyEvent *e);

  private:
    void parseMenu(QString menuname, int row = -1, int col = -1);

    void parseSettings(QString dir, QString menuname);

    void parseBackground(QString dir, QDomElement &element);
    void parseLogo(QString dir, QDomElement &element);
    void parseArrow(QString dir, QDomElement &element, bool up);
    void parseTitle(QString dir, QDomElement &element);
    void parseButtonDefinition(QString dir, QDomElement &element);
    void parseButton(QString dir, QDomElement &element);
    void parseThemeButton(QDomElement &element);
    
    void parseText(QDomElement &element);
    void parseOutline(QDomElement &element);
    void parseShadow(QDomElement &element);

    void setDefaults(void);

    void addButton(QString &type, QString &text, QString &alttext,
                   QString &action);
    void layoutButtons(void);
    void positionButtons(bool resetpos);   
    bool makeRowVisible(int newrow, int oldrow);

    void handleAction(QString &action);
    bool findDepends(QString file);
    QString findMenuFile(QString menuname);
    
    QString getFirstText(QDomElement &element);
    QPoint parsePoint(QString text);
    QRect parseRect(QString text);

    QRect menuRect() const;

    void paintLogo(QPainter *p);
    void paintTitle(QPainter *p);
    void paintWatermark(QPainter *p);
    void paintButton(unsigned int button, QPainter *p, bool erased,
                     bool drawinactive = false);

    void drawText(QPainter *p, QRect &rect, int textflags, QString text);

    void clearToBackground(void);
    void drawInactiveButtons(void);
    void drawScrollArrows(QPainter *p);

    QString prefix;
    
    QRect buttonArea;    
    
    QRect logoRect;
    QPixmap *logo;

    QImage *buttonnormal;
    QImage *buttonactive;

    QMap<QString, ButtonIcon> allButtonIcons;

    vector<ThemedButton> buttonList;
    ThemedButton *activebutton;
    int currentrow;
    int currentcolumn;

    vector<MenuRow> buttonRows;
   
    QRect textRect;
    QColor textColor;
    QFont font;
    int textflags;

    bool hasshadow;
    QColor shadowColor;
    QPoint shadowOffset;
    int shadowalpha;

    bool hasoutline;
    QColor outlineColor;
    int outlinesize;

    QString selection;
    bool foundtheme;

    int menulevel;
    vector<MenuState> menufiles;

    void (*callback)(void *, QString &);
    void *callbackdata;

    bool killable;
    int exitModifier;

    bool spreadbuttons;

    QMap<QString, QPixmap> titleIcons;
    QPixmap *curTitle;
    QString titleText;
    QPoint titlePos;
    QRect titleRect;
    bool drawTitle;

    QPixmap backgroundPixmap;

    bool ignorekeys;

    int maxrows;
    int visiblerows;

    QPixmap *uparrow;
    QRect uparrowRect;
    QPixmap *downarrow;
    QRect downarrowRect;

    QPoint watermarkPos;
    QRect watermarkRect;
};

#endif
