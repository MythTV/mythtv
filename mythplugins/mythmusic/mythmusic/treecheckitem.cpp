#include <qapplication.h>
#include <qstring.h>
#include <qpixmap.h>
#include <qimage.h>
#include <qpainter.h>
#include <iostream>
using namespace std;

#include "treecheckitem.h"
#include "playlist.h"

#include <mythtv/mythcontext.h>

#include "res/album_pix.xpm"
#include "res/artist_pix.xpm"
#include "res/catalog_pix.xpm" 
#include "res/track_pix.xpm"
#include "res/cd_pix.xpm"
#include "res/track_up_down_pix.xpm"
#include "res/favorites_pix.xpm"
#include "res/playlist_pix.xpm"
#include "res/streams_pix.xpm"
#include "res/uncategorized_pix.xpm"

static bool pixmapsSet = false;
static QPixmap *pixartist = NULL;
static QPixmap *pixalbum = NULL;
static QPixmap *pixtrack = NULL;
static QPixmap *pixcatalog = NULL;
static QPixmap *pixcd = NULL;
static QPixmap *pixtrack_up_down = NULL;
static QPixmap *pixfavorites = NULL;
static QPixmap *pixplaylist = NULL;
static QPixmap *pixstreams = NULL;
static QPixmap *pixuncat = NULL;

static QPixmap *scalePixmap(const char **xpmdata, float wmult, float hmult)
{
    QImage tmpimage(xpmdata);
    QImage tmp2 = tmpimage.smoothScale((int)(tmpimage.width() * wmult),
                                       (int)(tmpimage.height() * hmult));
    QPixmap *ret = new QPixmap();
    ret->convertFromImage(tmp2);

    return ret;
}

static void setupPixmaps(void)
{
    int screenheight = 0, screenwidth = 0;
    float wmult = 0, hmult = 0;

    gContext->GetScreenSettings(screenwidth, wmult, screenheight, hmult);

    if (screenheight != 600 || screenwidth != 800)
    {
        pixartist = scalePixmap((const char **)artist_pix, wmult, hmult);
        pixalbum = scalePixmap((const char **)album_pix, wmult, hmult);
        pixtrack = scalePixmap((const char **)track_pix, wmult, hmult);
        pixcatalog = scalePixmap((const char **)catalog_pix, wmult, hmult);
        pixcd = scalePixmap((const char **)cd_pix, wmult, hmult);
        pixfavorites = scalePixmap((const char **)favorites_pix, wmult, hmult);
        pixplaylist = scalePixmap((const char **)playlist_pix, wmult, hmult);
        pixstreams = scalePixmap((const char **)streams_pix, wmult, hmult);
        pixuncat = scalePixmap((const char **)uncategorized_pix, wmult, hmult);
        pixtrack_up_down = scalePixmap((const char **)track_up_down_pix_xpm,
                                       wmult, hmult);
    }
    else
    {
        pixartist = new QPixmap((const char **)artist_pix);
        pixalbum = new QPixmap((const char **)album_pix);
        pixtrack = new QPixmap((const char **)track_pix);
        pixcatalog = new QPixmap((const char **)catalog_pix);
        pixcd = new QPixmap((const char **)cd_pix);
        pixfavorites = new QPixmap((const char **)favorites_pix);
        pixplaylist = new QPixmap((const char **)playlist_pix);
        pixstreams = new QPixmap((const char **)streams_pix);
        pixuncat = new QPixmap((const char **)uncategorized_pix);
        pixtrack_up_down = new QPixmap((const char **)track_up_down_pix_xpm);
    }

    pixmapsSet = true;
}

static QPixmap *getPixmap(const QString &level)
{
    if (level == "artist")
        return pixartist;
    else if (level == "album")
        return pixalbum;
    else if (level == "title")
        return pixtrack;
    else if (level == "genre")
        return pixcatalog;
    else if (level == "cd")
        return pixcd;
    else if (level == "playlist")
        return pixplaylist;
    else if (level == "favorite")
        return pixfavorites;
    else if (level == "stream")
        return pixstreams;
    else if (level == "uncategorized")
        return pixuncat;

    return NULL;
}

TreeCheckItem::TreeCheckItem(QListView *parent, QString &ltext, 
                             const QString &llevel, int l_id)
             : QCheckListItem(parent, ltext, 
                              QCheckListItem::CheckBox)
{
    checkable = true;
    level = llevel;
    id = l_id;

    pickPixmap();
}

TreeCheckItem::TreeCheckItem(TreeCheckItem *parent, QString &ltext, 
                             const QString &llevel, int l_id)
             : QCheckListItem(parent, ltext, 
                              QCheckListItem::CheckBox)
{
    checkable = true;
    level = llevel;
    id = l_id;
    
    pickPixmap();
}

TreeCheckItem::TreeCheckItem(TreeCheckItem *parent, 
                             TreeCheckItem *after,
                             QString &ltext, 
                             const QString &llevel, int l_id)
#if (QT_VERSION >= 0x030100)
             : QCheckListItem(parent, after, ltext, 
                              QCheckListItem::CheckBox)
#else
#warning
#warning ***   You should think seriously about upgrading your Qt to 3.1.0 or higher   ***
#warning
             : QCheckListItem(parent, ltext, 
                              QCheckListItem::CheckBox)
#endif
{
    checkable = true;
    level = llevel;
    id = l_id;
    after = after;  // -Wall for Qt < 3.1.x
    
    pickPixmap();
}

void TreeCheckItem::setCheckable(bool flag)
{
    if(flag == false)
    {
        QCheckListItem::setOn(false);
    }
    checkable = flag;
}

void TreeCheckItem::setOn(bool flag)
{
    if(checkable)
    {
        QCheckListItem::setOn(flag);
    }
}

void TreeCheckItem::paintCell(QPainter * p, const QColorGroup & cg, 
                              int column, int width, int align)
{
    if(!checkable)
    {
        QColorGroup *newcg = new QColorGroup();
        *newcg = cg;
        newcg->setColor(QColorGroup::Text, QColor(150, 150, 150));
        newcg->setColor(QColorGroup::HighlightedText, QColor(150, 150, 150));
        QCheckListItem::paintCell(p, *newcg, column, width, align);
        delete newcg;
    }
    else
    {
        QCheckListItem::paintCell(p, cg, column, width, align);
    }
}

CDCheckItem::CDCheckItem(QListView *parent, QString &ltext, 
                             const QString &llevel, int l_track)
             : TreeCheckItem(parent, ltext, llevel, l_track)
{
}

CDCheckItem::CDCheckItem(CDCheckItem *parent, QString &ltext, 
                             const QString &llevel, int l_track)
             : TreeCheckItem(parent, ltext, llevel, l_track)
{
}

void TreeCheckItem::pickPixmap(void)
{
    if (!pixmapsSet)
        setupPixmaps();

    QPixmap *pix = getPixmap(level);
    if (pix)
        setPixmap(0, *pix);
}

PlaylistItem::PlaylistItem(QListView *parent, const QString &title)
    : QListViewItem(parent, title)
{
    
}

PlaylistItem::PlaylistItem(QListViewItem *parent, const QString &title)
    : QListViewItem(parent, title)
{
}

PlaylistItem::PlaylistItem(QListViewItem *parent, QListViewItem *after, const QString &title)
    : QListViewItem(parent, after, title)
{
}

bool PlaylistTitle::isDefault()
{
//    return ptr_to_owner->isDefault();
    cout << "Why are you asking " << endl;
    return false;
}

void PlaylistTrack::beMoving(bool flag)
{
    if(flag)
    {
        setPixmap(0, *pixtrack_up_down);    
    }
    else
    {
        if (pixmap)
            setPixmap(0, *pixmap);
        else
            setPixmap(0, *pixtrack);   
    }
}

void PlaylistTrack::moveUpDown(bool flag)
{
    if(flag == true)
    {
        //  Check that we are not already at the top
        if(PlaylistTrack *swapper = dynamic_cast<PlaylistTrack*>(this->itemAbove()))
        {
            swapper->moveItem(this);
            ptr_to_owner->moveUpDown(true);
        }        
        else
        {
        }
    }
    else
    {
        //  Are we at the bottom?
        if(!nextSibling())
        {
        }
        else
        {
            moveItem(nextSibling());
            ptr_to_owner->moveUpDown(false);
        }
    }
}

PlaylistTitle::PlaylistTitle(QListViewItem *parent, const QString &title)
    : PlaylistItem(parent, title)
{
    active = false;

    if (!pixmapsSet)
        setupPixmaps();

    QPixmap *pix = getPixmap("playlist");
    if (pix)
        setPixmap(0, *pix);
}

void PlaylistTitle::userSelectedMe()
{
//    ptr_to_owner->becomeActive();
}

void PlaylistTitle::setOwner(Playlist *owner)
{
    ptr_to_owner = owner;
}

PlaylistTrack::PlaylistTrack(QListViewItem *parent, const QString &title)
    : PlaylistItem(parent, title)
{
    held = false;

    QString level = "title";
    if (title.left(10).lower() == "playlist -")
        level = "playlist";

    if (!pixmapsSet)
        setupPixmaps();

    pixmap = getPixmap(level);
    if (pixmap)
        setPixmap(0, *pixmap);
}

PlaylistTrack::PlaylistTrack(QListViewItem *parent, QListViewItem *after, const QString &title)
    : PlaylistItem(parent, after, title)
{
    held = false;

    QString level = "title";
    if (title.left(10).lower() == "playlist -")
        level = "playlist";

    if (!pixmapsSet)
        setupPixmaps();

    pixmap = getPixmap(level);
    if (pixmap)
        setPixmap(0, *pixmap);
}

PlaylistTitle::PlaylistTitle(QListView *parent, const QString &title)    : PlaylistItem(parent, title)
{
    active = false;

    if (!pixmapsSet)
        setupPixmaps();

    QPixmap *pix = getPixmap("playlist");
    if (pix)
        setPixmap(0, *pix);
}

void PlaylistTrack::userSelectedMe()
{
    //ptr_to_owner->doSomething();
}

void PlaylistTrack::setOwner(Track *owner)
{
    ptr_to_owner = owner;
}

int PlaylistTrack::getID()
{
    return ptr_to_owner->getValue();
}

void PlaylistTitle::removeTrack(int x)
{
    ptr_to_owner->removeTrack(x, false);
}


PlaylistPlaylist::PlaylistPlaylist(QListViewItem *parent, const QString &title)
    : PlaylistTrack(parent, title)
{
    pixmap = getPixmap("playlist");
    if (pixmap)
        setPixmap(0, *pixmap);
}

PlaylistPlaylist::PlaylistPlaylist(QListViewItem *parent, QListViewItem *after, const QString &title)
    : PlaylistTrack(parent, after, title)
{
    pixmap = getPixmap("playlist");
    if (pixmap)
        setPixmap(0, *pixmap);
}

PlaylistCD::PlaylistCD(QListViewItem *parent, const QString &title)
    : PlaylistTrack(parent, title)
{
    pixmap = getPixmap("cd");
    if (pixmap)
        setPixmap(0, *pixmap);
}

PlaylistCD::PlaylistCD(QListViewItem *parent, QListViewItem *after, const QString &title)
    : PlaylistTrack(parent, after, title)
{

    pixmap = getPixmap("cd");
    if (pixmap)
        setPixmap(0, *pixmap);
}

