#include <qapplication.h>
#include <qstring.h>
#include <qpixmap.h>
#include <qimage.h>

#include "treecheckitem.h"

#include <mythtv/mythcontext.h>

QPixmap *TreeCheckItem::artist = NULL;
QPixmap *TreeCheckItem::album = NULL;
QPixmap *TreeCheckItem::track = NULL;
QPixmap *TreeCheckItem::catalog = NULL;
QPixmap *TreeCheckItem::cd = NULL;
bool TreeCheckItem::pixmapsSet = false;

TreeCheckItem::TreeCheckItem(MythContext *context, QListView *parent, 
                             QString &ltext, const QString &llevel, 
                             Metadata *mdata)
  : QListViewItem(parent, ltext.prepend(" "))
{
    level = llevel;
    metadata = mdata;
    m_context = context;

}

TreeCheckItem::TreeCheckItem(MythContext *context, TreeCheckItem *parent, 
                             QString &ltext, const QString &llevel, 
                             Metadata *mdata)
             : QListViewItem(parent, ltext.prepend(" "))
{
    level = llevel;
    metadata = mdata;
    m_context = context;

}

