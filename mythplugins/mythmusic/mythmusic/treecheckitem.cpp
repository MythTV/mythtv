#include <qstring.h>
#include <qpixmap.h>

#include "treecheckitem.h"

#include "res/album_pix.xpm"
#include "res/artist_pix.xpm"
#include "res/catalog_pix.xpm" 
#include "res/track_pix.xpm"

TreeCheckItem::TreeCheckItem(QListView *parent, QString &ltext, 
                             const QString &llevel, Metadata *mdata)
             : QCheckListItem(parent, ltext.prepend(" "), 
                              QCheckListItem::CheckBox)
{
    level = llevel;
    metadata = mdata;

    pickPixmap();
}

TreeCheckItem::TreeCheckItem(TreeCheckItem *parent, QString &ltext,
                             const QString &llevel, Metadata *mdata)
             : QCheckListItem(parent, ltext.prepend(" "), 
                              QCheckListItem::CheckBox)
{
    level = llevel;
    metadata = mdata;

    pickPixmap();
}

void TreeCheckItem::pickPixmap(void)
{
    if (level == "artist")
        setPixmap(0, QPixmap((const char **)artist_pix));
    else if (level == "album")
        setPixmap(0, QPixmap((const char **)album_pix));
    else if (level == "title")
        setPixmap(0, QPixmap((const char **)track_pix));
    else if (level == "genre")
        setPixmap(0, QPixmap((const char **)catalog_pix));
}
