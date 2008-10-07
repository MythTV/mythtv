#ifndef _MYTHLISTVIEW_H_
#define _MYTHLISTVIEW_H_

#include <q3listview.h>

class Q3MythListView : public Q3ListView
{
    Q_OBJECT
  public:
    Q3MythListView(QWidget *parent);

    void ensureItemVCentered (const Q3ListViewItem *i);

  protected:
    void keyPressEvent(QKeyEvent *e);
    void focusInEvent(QFocusEvent *e);
};

#endif // _MYTHLISTVIEW_H_
