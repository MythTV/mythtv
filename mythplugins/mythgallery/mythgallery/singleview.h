#ifndef SINGLEVIEW_H_
#define SINGLEVIEW_H_

#include <qwidget.h>
#include <qstring.h>

#include <mythtv/mythdialogs.h>

class QImage;
class QFont;
class QTimer;

#include "iconview.h"

class SingleView : public MythDialog
{
    Q_OBJECT
  public:
    SingleView(QSqlDatabase *db, vector<Thumbnail> *imagelist, int pos, 
               QWidget *parent = 0, const char *name = 0);
   ~SingleView();

  protected slots:
    void advanceFrame(void);

  protected:
    void paintEvent(QPaintEvent *e);
    void keyPressEvent(QKeyEvent *e);

  private:

    QImage *image;

    QFont *m_font;

    vector<Thumbnail> *images;
    int imagepos;
    int displaypos;

    QTimer *timer;
    int timersecs;

    bool timerrunning;
    int rotateAngle, imageRotateAngle;
    bool redraw;

    QSqlDatabase *m_db;
};

#endif
