#ifndef EXTENDEDLISTVIEW_H_
#define EXTENDEDLISTVIEW_H_

#include <qlistview.h>
#include <qheader.h>

class ExtendedListView : public QListView
{
    Q_OBJECT
  public:
    ExtendedListView(QWidget * parent = 0, const char * name = 0, WFlags f = 0):
                QListView(parent, name, f) 
    {
        viewport()->setPalette(palette());
        horizontalScrollBar()->setPalette(palette());
        verticalScrollBar()->setPalette(palette());
        header()->setPalette(palette());
        header()->setFont(font());
    }

  signals:
    void KeyPressed( QListViewItem *, int key);

  protected:
    void keyPressEvent( QKeyEvent *e );
};

#endif
