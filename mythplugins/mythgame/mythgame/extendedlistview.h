#ifndef EXTENDEDLISTVIEW_H_
#define EXTENDEDLISTVIEW_H_

#include <qlistview.h>

class ExtendedListView : public QListView
{
    Q_OBJECT
  public:
    ExtendedListView(QWidget * parent = 0, const char * name = 0, WFlags f = 0):
                QListView(parent, name, f) {}

  signals:
    void RightKeyPressed( QListViewItem * );

  protected:
    void keyPressEvent( QKeyEvent *e );
};

#endif
