#ifndef THEMESETUP_H_
#define THEMESETUP_H_

#include <qwidget.h>
#include <qstring.h>
#include <qpixmap.h>
#include <pthread.h>
#include <vector>

#include "libmyth/mythwidgets.h"

using namespace std;

class QSqlDatabase;
class QFont;
class MythContext;

#define THUMBS_W 3
#define THUMBS_H 3

class Thumbnail
{
  public:
    Thumbnail() { pixmap = NULL; filename = ""; name = ""; isdir = false; }
    Thumbnail(const Thumbnail &other) { pixmap = other.pixmap; 
                                        filename = other.filename;
                                        isdir = other.isdir;
                                        name = other.name;
                                      }

    QPixmap *pixmap;
    QString filename;
    QString name;
    bool isdir;
};

class ThemeSetup : public MyDialog
{
    Q_OBJECT
  public:
    ThemeSetup(MythContext *context,  QSqlDatabase *ldb, QWidget *parent = 0,
             const char *name = 0);
   ~ThemeSetup();

    void Show(void);

  protected:
    void paintEvent(QPaintEvent *e);
    void keyPressEvent(QKeyEvent *e);
   
  private:
    void fillList(const QString &dir);
    void loadThumbPixmap(Thumbnail *thumb);
    
    QFont *m_font;

    QColor fgcolor;
    QColor highlightcolor;

    int thumbw, thumbh;
    int spacew, spaceh;

    vector<Thumbnail> thumbs;
    unsigned int screenposition;

    int currow, curcol;

    QSqlDatabase *db;
};

#endif

