#ifndef SINGLEVIEW_H_
#define SINGLEVIEW_H_

#include <qwidget.h>
#include <qdialog.h>
#include <qstring.h>

class QImage;
class QFont;

class SingleView : public QDialog
{
    Q_OBJECT
  public:
    SingleView(const QString &filename, QWidget *parent = 0,
               const char *name = 0);
   ~SingleView();

  protected:
    void paintEvent(QPaintEvent *e);
    void keyPressEvent(QKeyEvent *e);

  private:

    QImage *image;

    QColor bgcolor, fgcolor;
    int screenwidth, screenheight;

    float wmult, hmult;

    QFont *m_font;
};

#endif
