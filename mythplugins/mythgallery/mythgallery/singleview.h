#ifndef SINGLEVIEW_H_
#define SINGLEVIEW_H_

#include <qwidget.h>
#include <qdialog.h>
#include <qstring.h>

class QImage;
class QFont;
class QTimer;

#include "iconview.h"

class SingleView : public QDialog
{
    Q_OBJECT
  public:
    SingleView(vector<Thumbnail> *imagelist, int pos, QWidget *parent = 0,
               const char *name = 0);
   ~SingleView();

  protected slots:
    void advanceFrame(void);

  protected:
    void paintEvent(QPaintEvent *e);
    void keyPressEvent(QKeyEvent *e);

  private:

    QImage *image;

    QColor bgcolor, fgcolor;
    int screenwidth, screenheight;

    float wmult, hmult;

    QFont *m_font;

    vector<Thumbnail> *images;
    int imagepos;
    int displaypos;

    QTimer *timer;
    int timersecs;

    bool timerrunning;
};

#endif
