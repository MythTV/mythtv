#include "extendedlistview.h"

void ExtendedListView::keyPressEvent( QKeyEvent *e )
{
  if(e && Key_Right == e->key())
  {
    emit RightKeyPressed( currentItem() );
  }
  QListView::keyPressEvent(e);
}
