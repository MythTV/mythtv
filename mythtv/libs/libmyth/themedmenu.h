#ifndef THEMEDMENU_H_
#define THEMEDMENU_H_

#include <qwidget.h>
#include <qvaluelist.h>
#include <qdom.h>
#include <qmap.h>

#include "mythwidgets.h"

#include <vector>

using namespace std;

class QPixmap;

struct ButtonIcon
{
    QString name;
    QPixmap *icon;
    QPixmap *activeicon;
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

    int status;
};

struct MenuRow
{
    int numitems;
    vector<ThemedButton *> buttons;
};

class ThemedMenu : public MythDialog
{
    Q_OBJECT
  public:
    ThemedMenu(const char *cdir, const char *menufile, 
               QWidget *parent = 0, const char *name = 0);
   ~ThemedMenu();

    bool foundTheme() { return foundtheme; }

    void setCallback(void (*lcallback)(void *, QString &), void *data) 
                                        { callback = lcallback;
                                          callbackdata = data;
                                        }
    void setKillable(void) { killable = true; }

    QString getSelection() { return selection; }

    void ReloadTheme(void);

  protected:
    void paintEvent(QPaintEvent *e);
    void keyPressEvent(QKeyEvent *e);

  private:
    void parseMenu(QString menuname);

    void parseSettings(QString dir, QString menuname);

    void parseBackground(QString dir, QDomElement &element);
    void parseLogo(QString dir, QDomElement &element);
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
   
    void handleAction(QString &action);
    bool findDepends(QString file);
    QString findMenuFile(QString menuname);
    
    QString getFirstText(QDomElement &element);
    QPoint parsePoint(QString text);
    QRect parseRect(QString text);

    QRect menuRect() const;

    void paintLogo(QPainter *p);
    void paintTitle(QPainter *p);
    void paintButton(unsigned int button, QPainter *p, bool erased);

    void drawText(QPainter *p, QRect &rect, int textflags, QString text);

    QString prefix;
    
    QRect buttonArea;    
    
    QPoint logopos;
    QRect logoRect;
    QPixmap *logo;

    QPixmap *buttonnormal;
    QPixmap *buttonactive;

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
    vector<QString> menufiles;

    void (*callback)(void *, QString &);
    void *callbackdata;

    bool killable;
    int exitModifier;

    bool spreadbuttons;

    QMap<QString, QPixmap> titleIcons;
    QPixmap *curTitle;
    QPoint titlePos;
    QRect titleRect;
    bool drawTitle;
};

#endif
