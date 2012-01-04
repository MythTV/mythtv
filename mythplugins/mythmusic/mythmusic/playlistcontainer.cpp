#include <mythcontext.h>
#include <mythdb.h>
#include <compat.h>

#include "playlistcontainer.h"
#include "mythlogging.h"

PlaylistLoadingThread::PlaylistLoadingThread(PlaylistContainer *parent_ptr,
                                             AllMusic *all_music_ptr) :
    MThread("PlaylistLoading"), parent(parent_ptr), all_music(all_music_ptr)
{
}

void PlaylistLoadingThread::run()
{
    RunProlog();
    while (!all_music->doneLoading())
    {
        msleep(250);
    }
    parent->load();
    RunEpilog();
}

#define LOC      QString("PlaylistContainer: ")
#define LOC_WARN QString("PlaylistContainer, Warning: ")
#define LOC_ERR  QString("PlaylistContainer, Error: ")

void PlaylistContainer::clearCDList()
{
    cd_playlist.clear();
}

void PlaylistContainer::addCDTrack(int track)
{
    cd_playlist.push_back(track);
}

void PlaylistContainer::removeCDTrack(int track)
{
    cd_playlist.removeAll(track);
}

bool PlaylistContainer::checkCDTrack(int track)
{
    return cd_playlist.contains(track);
}

PlaylistContainer::PlaylistContainer(
    AllMusic *all_music, const QString &host_name) :
    active_playlist(NULL),      backup_playlist(NULL),
    all_other_playlists(NULL),  all_available_music(all_music),
    pending_writeback_index(-1),

    playlists_loader(new PlaylistLoadingThread(this, all_music)),
    done_loading(false),        my_host(host_name),

    RatingWeight(   gCoreContext->GetNumSetting("IntelliRatingWeight",    2)),
    PlayCountWeight(gCoreContext->GetNumSetting("IntelliPlayCountWeight", 2)),
    LastPlayWeight( gCoreContext->GetNumSetting("IntelliLastPlayWeight",  2)),
    RandomWeight(   gCoreContext->GetNumSetting("IntelliRandomWeight",    2))
{
    playlists_loader->start();
}

PlaylistContainer::~PlaylistContainer()
{
    playlists_loader->wait();
    delete playlists_loader;
    playlists_loader = NULL;

    if (active_playlist)
        delete active_playlist;
    if (backup_playlist)
        delete backup_playlist;
    if (stream_playlist)
        delete stream_playlist;
    if (all_other_playlists)
    {
        while (!all_other_playlists->empty())
        {
            delete all_other_playlists->front();
            all_other_playlists->pop_front();
        }
        delete all_other_playlists;
    }
}

void PlaylistContainer::FillIntelliWeights(int &rating, int &playcount,
                                            int &lastplay, int &random)
{
    rating = RatingWeight;
    playcount = PlayCountWeight;
    lastplay = LastPlayWeight;
    random = RandomWeight;
}

void PlaylistContainer::load()
{
    done_loading = false;
    active_playlist = new Playlist();
    active_playlist->setParent(this);

    backup_playlist = new Playlist();
    backup_playlist->setParent(this);

    stream_playlist = new Playlist();
    stream_playlist->setParent(this);

    all_other_playlists = new QList<Playlist*>;

    cd_playlist.clear();

    active_playlist->loadPlaylist("default_playlist_storage", my_host);

    backup_playlist->loadPlaylist("backup_playlist_storage", my_host);

    stream_playlist->loadPlaylist("stream_playlist", my_host);

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT playlist_id FROM music_playlists "
                  "WHERE playlist_name != :DEFAULT"
                  " AND playlist_name != :BACKUP "
                  " AND (hostname = '' OR hostname = :HOST) "
                  "ORDER BY playlist_name;");
    query.bindValue(":DEFAULT", "default_playlist_storage");
    query.bindValue(":BACKUP", "backup_playlist_storage");
    query.bindValue(":HOST", my_host);

    if (!query.exec())
    {
        MythDB::DBError("Querying playlists", query);
    }
    else
    {
        while (query.next())
        {
            Playlist *temp_playlist = new Playlist();
            //  No, we don't destruct this ...
            temp_playlist->setParent(this);
            temp_playlist->loadPlaylistByID(query.value(0).toInt(), my_host);
            all_other_playlists->push_back(temp_playlist);
            //  ... cause it's sitting on this PtrList
        }
    }
    postLoad();

    pending_writeback_index = 0;

    int x = gCoreContext->GetNumSetting("LastMusicPlaylistPush");
    setPending(x);
    done_loading = true;
}

void PlaylistContainer::describeYourself(void) const
{
    //    Debugging
    active_playlist->describeYourself();
    QList<Playlist*>::const_iterator it = all_other_playlists->begin();
    for (; it != all_other_playlists->end(); ++it)
        (*it)->describeYourself();
}

Playlist *PlaylistContainer::getPlaylist(int id)
{
    //  return a pointer to a playlist
    //  by id;

    if (active_playlist->getID() == id)
    {
        return active_playlist;
    }

    QList<Playlist*>::iterator it = all_other_playlists->begin();
    for (; it != all_other_playlists->end(); ++it)
    {
        if ((*it)->getID() == id)
            return *it;
    }

    LOG(VB_GENERAL, LOG_ERR,
        "getPlaylistName() called with unknown index number");
    return NULL;
}

Playlist *PlaylistContainer::getPlaylist(const QString &name)
{
    //  return a pointer to a playlist
    //  by name;

    QList<Playlist*>::iterator it = all_other_playlists->begin();
    for (; it != all_other_playlists->end(); ++it)
    {
        if ((*it)->getName() == name)
            return *it;
    }

    LOG(VB_GENERAL, LOG_ERR, QString("getPlaylistName() called with unknown name: %1").arg(name));
    return NULL;
}

void PlaylistContainer::save(void)
{
    QList<Playlist*>::const_iterator it = all_other_playlists->begin();
    for (; it != all_other_playlists->end(); ++it)
    {
        if ((*it)->hasChanged())
            (*it)->savePlaylist((*it)->getName(), my_host);
    }

    active_playlist->savePlaylist("default_playlist_storage", my_host);
    backup_playlist->savePlaylist("backup_playlist_storage", my_host);
}

void PlaylistContainer::createNewPlaylist(QString name)
{
    Playlist *new_list = new Playlist();
    new_list->setParent(this);

    //  Need to touch the database to get persistent ID
    new_list->savePlaylist(name, my_host);
    new_list->Changed();
    all_other_playlists->push_back(new_list);
}

void PlaylistContainer::copyNewPlaylist(QString name)
{
    Playlist *new_list = new Playlist();
    new_list->setParent(this);

    //  Need to touch the database to get persistent ID
    new_list->savePlaylist(name, my_host);
    new_list->Changed();
    all_other_playlists->push_back(new_list);
    active_playlist->copyTracks(new_list, false);
    pending_writeback_index = 0;
}

void PlaylistContainer::popBackPlaylist()
{
    Playlist *destination = getPlaylist(pending_writeback_index);
    if (!destination)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "popBackPlaylist() " +
            QString("Unknown playlist: %1") .arg(pending_writeback_index));
        return;
    }
    destination->removeAllTracks();
    destination->Changed();
    active_playlist->copyTracks(destination, false);
    active_playlist->removeAllTracks();
    backup_playlist->copyTracks(active_playlist, true);
    pending_writeback_index = 0;

    active_playlist->Changed();
    backup_playlist->Changed();
}

void PlaylistContainer::copyToActive(int index)
{
    backup_playlist->removeAllTracks();
    active_playlist->copyTracks(backup_playlist, false);

    pending_writeback_index = index;

    active_playlist->removeAllTracks();
    Playlist *copy_from = getPlaylist(index);
    if (!copy_from)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "copyToActive() " +
            QString("Unknown playlist: %1").arg(index));
        return;
    }
    copy_from->copyTracks(active_playlist, true);

    active_playlist->Changed();
    backup_playlist->Changed();
}


void PlaylistContainer::renamePlaylist(int index, QString new_name)
{
    Playlist *list_to_rename = getPlaylist(index);
    if (list_to_rename)
    {
        list_to_rename->setName(new_name);
        list_to_rename->Changed();
    }
}

void PlaylistContainer::deletePlaylist(int kill_me)
{
    Playlist *list_to_kill = getPlaylist(kill_me);
    if (!list_to_kill)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "deletePlaylist() " +
            QString("Unknown playlist: %1").arg(kill_me));
        return;
    }
    //  First, we need to take out any **track** on any other
    //  playlist that is actually a reference to this
    //  playlist

    if (kill_me == pending_writeback_index)
        popBackPlaylist();

    active_playlist->removeTrack(kill_me * -1);

    QList<Playlist*>::iterator it = all_other_playlists->begin();
    for (; it != all_other_playlists->end(); ++it)
    {
        if ((*it) != list_to_kill)
            (*it)->removeTrack(kill_me * -1);
    }

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM music_playlists WHERE playlist_id = :ID ;");
    query.bindValue(":ID", kill_me);

    if (!query.exec() || query.numRowsAffected() < 1)
    {
        MythDB::DBError("playlist delete", query);
    }
    list_to_kill->removeAllTracks();
    all_other_playlists->removeAll(list_to_kill);
}


QString PlaylistContainer::getPlaylistName(int index, bool &reference)
{
    if (active_playlist)
    {
        if (active_playlist->getID() == index)
        {
            return active_playlist->getName();
        }

        QList<Playlist*>::iterator it = all_other_playlists->begin();
        for (; it != all_other_playlists->end(); ++it)
        {
            if ((*it)->getID() == index)
                return (*it)->getName();
        }
    }

    LOG(VB_GENERAL, LOG_ERR, LOC +
        "getPlaylistName() called with unknown index number");

    reference = true;
    return QObject::tr("Something is Wrong");
}


void PlaylistContainer::postLoad()
{
    //FIXME remove this and the other post load stuff?

    //  Now that everything is loaded, we need to recheck all
    //  tracks and update those that refer to a playlist
#if 0
    active_playlist->postLoad();
    backup_playlist->postLoad();

    QList<Playlist*>::iterator it = all_other_playlists->begin();
    for (; it != all_other_playlists->end(); ++it)
        (*it)->postLoad();
#endif
}

bool PlaylistContainer::pendingWriteback()
{
    if (pending_writeback_index > 0)
    {
        return true;
    }
    return false;
}

bool PlaylistContainer::nameIsUnique(QString a_name, int which_id)
{
    if (a_name == "default_playlist_storage")
        return false;

    if (a_name == "backup_playlist_storage")
        return false;

    QList<Playlist*>::iterator it = all_other_playlists->begin();
    for (; it != all_other_playlists->end(); ++it)
    {
        if ((*it)->getName() == a_name && (*it)->getID() != which_id)
            return false;
    }

    return true;
}

QStringList PlaylistContainer::getPlaylistNames(void)
{
    QStringList res;

    QList<Playlist*>::iterator it = all_other_playlists->begin();
    for (; it != all_other_playlists->end(); ++it)
    {
        res.append((*it)->getName());
    }

    return res;
}

bool PlaylistContainer::cleanOutThreads()
{
    if (playlists_loader->isFinished())
    {
        return true;
    }
    playlists_loader->wait();
    return false;
}

void PlaylistContainer::clearActive()
{
    backup_playlist->removeAllTracks();
    active_playlist->removeAllTracks();
    backup_playlist->Changed();
    active_playlist->Changed();
    pending_writeback_index = 0;
}

