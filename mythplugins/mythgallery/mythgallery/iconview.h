#ifndef ICONVIEW_H_
#define ICONVIEW_H_

#include <qwidget.h>
#include <qstring.h>
#include <qsqldatabase.h>
#include <qpixmap.h>

#include <vector>
using namespace std;

#include <mythtv/mythdialogs.h>

class QFont;

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

class IconView : public MythDialog
{
    Q_OBJECT
  public:
    IconView(QSqlDatabase *db, const QString &startdir, 
             MythMainWindow *parent, const char *name = 0);
   ~IconView();

  protected:
    void paintEvent(QPaintEvent *e);
    void keyPressEvent(QKeyEvent *e);

  private:
    void fillList(const QString &dir);
    void loadThumbPixmap(Thumbnail *thumb);

    bool moveDown();
    bool moveUp();
    bool moveLeft();
    bool moveRight();
    void importPics();
    void importFromDir(const QString &fromDir, const QString &toDir);

    QFont *m_font;

    QColor fgcolor;
    QColor highlightcolor;

    int thumbw, thumbh;
    int spacew, spaceh;

    vector<Thumbnail> thumbs;
    unsigned int screenposition;

    int currow, curcol;

    QPixmap *foldericon;

    QSqlDatabase *m_db;
    QString curdir;
    MythProgressDialog *cacheprogress;
};

#endif
