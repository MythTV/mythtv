#include <qapplication.h>
#include <qstring.h>
#include <qpixmap.h>
#include <qimage.h>

#include "treecheckitem.h"

#include <mythtv/settings.h>

extern Settings *globalsettings;

#include "res/album_pix.xpm"
#include "res/artist_pix.xpm"
#include "res/catalog_pix.xpm" 
#include "res/track_pix.xpm"
#include "res/cd_pix.xpm"

QPixmap *TreeCheckItem::artist = NULL;
QPixmap *TreeCheckItem::album = NULL;
QPixmap *TreeCheckItem::track = NULL;
QPixmap *TreeCheckItem::catalog = NULL;
QPixmap *TreeCheckItem::cd = NULL;
bool TreeCheckItem::pixmapsSet = false;

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
    if (!pixmapsSet)
        setupPixmaps();

    if (level == "artist")
        setPixmap(0, *artist);
    else if (level == "album")
        setPixmap(0, *album);
    else if (level == "title")
        setPixmap(0, *track);
    else if (level == "genre")
        setPixmap(0, *catalog);
    else if (level == "cd")
        setPixmap(0, *cd);
}

void TreeCheckItem::setupPixmaps(void)
{
    int screenheight = QApplication::desktop()->height();
    int screenwidth = QApplication::desktop()->width();

    if (globalsettings->GetNumSetting("GuiWidth") > 0)
        screenwidth = globalsettings->GetNumSetting("GuiWidth");
    if (globalsettings->GetNumSetting("GuiHeight") > 0)
        screenheight = globalsettings->GetNumSetting("GuiHeight");

    float wmult = screenwidth / 800.0;
    float hmult = screenheight / 600.0;

    if (screenheight != 600 || screenwidth != 800) 
    {   
        artist = scalePixmap((const char **)artist_pix, wmult, hmult);
        album = scalePixmap((const char **)album_pix, wmult, hmult);
        track = scalePixmap((const char **)track_pix, wmult, hmult);
        catalog = scalePixmap((const char **)catalog_pix, wmult, hmult);
        cd = scalePixmap((const char **)cd_pix, wmult, hmult);
    }
    else
    {
        artist = new QPixmap((const char **)artist_pix);
        album = new QPixmap((const char **)album_pix);
        track = new QPixmap((const char **)track_pix);
        catalog = new QPixmap((const char **)catalog_pix);
        cd = new QPixmap((const char **)cd_pix);
    }

    pixmapsSet = true;
}

QPixmap *TreeCheckItem::scalePixmap(const char **xpmdata, float wmult, 
                                    float hmult)
{
    QImage tmpimage(xpmdata);
    QImage tmp2 = tmpimage.smoothScale((int)(tmpimage.width() * wmult), 
                                       (int)(tmpimage.height() * hmult));
    QPixmap *ret = new QPixmap();
    ret->convertFromImage(tmp2);

    return ret;
}
