#ifndef THEMEDMENU_H_
#define THEMEDMENU_H_

#include <qwidget.h>
#include <qdialog.h>
#include <qvaluevector.h>
#include <qvaluelist.h>

class QPixmap;

struct ThemedButton
{
    QPoint pos;
    QRect  posRect;
    QPixmap *icon; 
    QPoint iconPos;
    QRect iconRect;
    QString text;
    QString selectioninfo;
};

class ThemedMenu : public QDialog
{
    Q_OBJECT
  public:
    ThemedMenu(const char *cdir, const char *menufile, 
            QWidget *parent = 0, const char *name = 0);
   ~ThemedMenu();

    bool foundTheme() { return foundtheme; }

    void Show();

    QString getSelection() { return selection; }

  protected:
    void paintEvent(QPaintEvent *e);
    void keyPressEvent(QKeyEvent *e);

  private:
    void parseSettings(QString dir, QString menuname);

    QPoint parsePoint(QString text);
    QRect parseRect(QString text);
    QValueList<int> parseOrder(QString text);

    QRect menuRect() const;

    void paintLogo(QPainter *p);
    void paintButton(unsigned int button, QPainter *p);

    void drawText(QPainter *p, QRect &rect, int textflags, QString text);

    QPixmap *scalePixmap(QString filename);
    float wmult, hmult;

    int screenwidth, screenheight;

    QPoint logopos;
    QRect logoRect;
    QPixmap *logo;

    QPixmap *buttonnormal;
    QPixmap *buttonactive;
    QValueVector<ThemedButton> buttonList;
    QValueList<int> buttonUpDown;
    QValueList<int> buttonLeftRight;
    unsigned int activebutton;

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
};

QString findThemeDir(QString themename, QString prefix);

#endif
