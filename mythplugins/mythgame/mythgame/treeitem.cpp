#include <qapplication.h>
#include <qstring.h>
#include <qpixmap.h>
#include <qimage.h>

#include "treeitem.h"
#include <mythtv/settings.h>

extern Settings *globalsettings;

#include "res/system_pix.xpm"
#include "res/genre_pix.xpm"
#include "res/game_pix.xpm"

QPixmap *TreeItem::system = NULL;
QPixmap *TreeItem::game = NULL;
QPixmap *TreeItem::genre = NULL;
bool TreeItem::pixmapsSet = false;

TreeItem::TreeItem(QListView *parent, QString &ltext,
                             const QString &llevel, RomInfo *rinfo)
             : QListViewItem(parent, ltext.prepend(" "))
{
    level = llevel;
    rominfo = rinfo;

    pickPixmap();
}

TreeItem::TreeItem(TreeItem *parent, QString &ltext,
                             const QString &llevel, RomInfo *rinfo)
             : QListViewItem(parent, ltext.prepend(" "))
{
    level = llevel;
    rominfo = rinfo;

    pickPixmap();
}

void TreeItem::pickPixmap(void)
{
    if (!pixmapsSet)
        setupPixmaps();

    if (level == "system")
        setPixmap(0, *system);
    else if (level == "gamename")
    {
        //QPixmap test;
        //test.load("/screens/005.png");
        //setPixmap(0,test);
        setPixmap(0, *game);
        //Cabinet.load(picfile);
        //ScaleImageLabel(Cabinet,CabinetPic);
        //CabinetPic->setPixmap(Cabinet);
    }
    else if (level == "genre")
        setPixmap(0, *genre);
}

void TreeItem::setupPixmaps(void)
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
        system = scalePixmap((const char **)system_pix, wmult, hmult);
        game = scalePixmap((const char **)game_pix, wmult, hmult);
        genre = scalePixmap((const char **)genre_pix, wmult, hmult);
    }
    else
    {
        system = new QPixmap((const char **)system_pix);
        game = new QPixmap((const char **)game_pix);
        genre = new QPixmap((const char **)genre_pix);
    }

    pixmapsSet = true;
}

QPixmap *TreeItem::scalePixmap(const char **xpmdata, float wmult,
                                    float hmult)
{
    QImage tmpimage(xpmdata);
    QImage tmp2 = tmpimage.smoothScale(tmpimage.width() * wmult, 
                                       tmpimage.height() * hmult);
    QPixmap *ret = new QPixmap();
    ret->convertFromImage(tmp2);

    return ret;
}
