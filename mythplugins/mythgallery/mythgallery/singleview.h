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
               MythMainWindow *parent, const char *name = 0);
   ~SingleView();

    void startShow(void);

  protected slots:
    void advanceFrame(bool doUpdate = true);
    void retreatFrame(bool doUpdate = true);

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
    int origWidth, origHeight;
    int newzoom, zoomfactor;
    int sx, sy;
    int zoomed_w, zoomed_h;
    bool redraw;
    QSqlDatabase *m_db;
};

#endif
