#include "extendedlistview.h"

void ExtendedListView::keyPressEvent( QKeyEvent *e )
{
  //if(e && Key_Right == e->key())
  //{
  if(e)
    emit KeyPressed( currentItem(), e->key());
  //}
  QListView::keyPressEvent(e);
}
