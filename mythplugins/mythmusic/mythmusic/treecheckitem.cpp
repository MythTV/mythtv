#include <iostream>
using namespace std;

#include <QPixmap>
#include <QImage>

#include <mythcontext.h>
#include <mythuihelper.h>

#include "treecheckitem.h"
#include "playlist.h"

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
    QImage tmp2 = tmpimage.scaled((int)(tmpimage.width() * wmult),
                                       (int)(tmpimage.height() * hmult),
                                       Qt::IgnoreAspectRatio,
                                       Qt::SmoothTransformation);
    QPixmap *ret = new QPixmap();
    ret->convertFromImage(tmp2);

    return ret;
}

static void setupPixmaps(void)
{
    int screenheight = 0, screenwidth = 0;
    float wmult = 0, hmult = 0;

    GetMythUI()->GetScreenSettings(screenwidth, wmult, screenheight, hmult);

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

TreeCheckItem::TreeCheckItem(UIListGenericTree *parent, const QString &text, 
                             const QString &level, int id) :
    UIListGenericTree(parent, text, "TREECHECK", 0),
    m_id(id), m_level(level), m_checkable(true)
{
    pickPixmap();
}

void TreeCheckItem::setCheckable(bool flag)
{
    if (flag == false)
        setCheck(-1);
    m_checkable = flag;
}

void TreeCheckItem::setCheck(int flag)
{
    if (m_checkable)
        UIListGenericTree::setCheck(flag);
}

void TreeCheckItem::pickPixmap(void)
{
    if (!pixmapsSet)
        setupPixmaps();

    QPixmap *pix = getPixmap(m_level);
    if (pix)
        m_image = pix;
}

CDCheckItem::CDCheckItem(UIListGenericTree *parent, const QString &text, 
                         const QString &level, int track) :
    TreeCheckItem(parent, text, level, track)
{
}

PlaylistItem::PlaylistItem(UIListGenericTree *parent, const QString &title) :
    UIListGenericTree(parent, title, "PLAYLISTITEM", -1)
{
    text = title;
}

PlaylistTitle::PlaylistTitle(UIListGenericTree *parent, const QString &title) :
    PlaylistItem(parent, title), ptr_to_owner(NULL), active(false)
{
    if (!pixmapsSet)
        setupPixmaps();

    QPixmap *pix = getPixmap("playlist");
    if (pix)
        m_image = pix;
}

void PlaylistTitle::userSelectedMe(void)
{
}

void PlaylistTitle::setOwner(Playlist *owner)
{
    ptr_to_owner = owner;
}

PlaylistTrack::PlaylistTrack(UIListGenericTree *parent, const QString &title) :
    PlaylistItem(parent, title), pixmap(NULL), ptr_to_owner(NULL), held(false)
{
    QString level = "title";
    if (title.left(10).lower() == "playlist -")
        level = "playlist";

    if (!pixmapsSet)
        setupPixmaps();

    pixmap = getPixmap(level);
    if (pixmap)
        m_image = pixmap;
}

void PlaylistTrack::userSelectedMe(void)
{
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

void PlaylistTrack::beMoving(bool flag)
{
    if (flag)
        setPixmap(pixtrack_up_down);
    else if (pixmap)
        setPixmap(pixmap);
    else
        setPixmap(pixtrack);
}

void PlaylistTrack::moveUpDown(bool flag)
{
    if (movePositionUpDown(flag))
        ptr_to_owner->moveUpDown(flag);
}

PlaylistPlaylist::PlaylistPlaylist(UIListGenericTree *parent, 
                                   const QString &title) :
    PlaylistTrack(parent, title)
{
    pixmap = getPixmap("playlist");
    if (pixmap)
        m_image = pixmap;
}

PlaylistCD::PlaylistCD(UIListGenericTree *parent, const QString &title) :
    PlaylistTrack(parent, title)
{
    pixmap = getPixmap("cd");
    if (pixmap)
        m_image = pixmap;
}
