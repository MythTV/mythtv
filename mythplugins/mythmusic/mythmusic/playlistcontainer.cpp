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
    m_cdPlaylist.clear();
}

void PlaylistContainer::addCDTrack(int track)
{
    m_cdPlaylist.push_back(track);
}

void PlaylistContainer::removeCDTrack(int track)
{
    m_cdPlaylist.removeAll(track);
}

bool PlaylistContainer::checkCDTrack(int track)
{
    return m_cdPlaylist.contains(track);
}

PlaylistContainer::PlaylistContainer(
    AllMusic *all_music, const QString &host_name) :
    m_activePlaylist(NULL),      m_backupPlaylist(NULL),
    m_streamPlaylist(NULL),
    m_allPlaylists(NULL),  m_allMusic(all_music),
    m_pendingWritebackIndex(-1),

    m_playlistsLoader(new PlaylistLoadingThread(this, all_music)),
    m_doneLoading(false),        m_myHost(host_name),

    m_ratingWeight(   gCoreContext->GetNumSetting("IntelliRatingWeight",    2)),
    m_playCountWeight(gCoreContext->GetNumSetting("IntelliPlayCountWeight", 2)),
    m_lastPlayWeight( gCoreContext->GetNumSetting("IntelliLastPlayWeight",  2)),
    m_randomWeight(   gCoreContext->GetNumSetting("IntelliRandomWeight",    2))
{
    m_playlistsLoader->start();
}

PlaylistContainer::~PlaylistContainer()
{
    m_playlistsLoader->wait();
    delete m_playlistsLoader;
    m_playlistsLoader = NULL;

    if (m_activePlaylist)
        delete m_activePlaylist;
    if (m_backupPlaylist)
        delete m_backupPlaylist;
    if (m_streamPlaylist)
        delete m_streamPlaylist;
    if (m_allPlaylists)
    {
        while (!m_allPlaylists->empty())
        {
            delete m_allPlaylists->front();
            m_allPlaylists->pop_front();
        }
        delete m_allPlaylists;
    }
}

void PlaylistContainer::FillIntelliWeights(int &rating, int &playcount,
                                            int &lastplay, int &random)
{
    rating = m_randomWeight;
    playcount = m_playCountWeight;
    lastplay = m_lastPlayWeight;
    random = m_randomWeight;
}

void PlaylistContainer::load()
{
    m_doneLoading = false;
    m_activePlaylist = new Playlist();
    m_activePlaylist->setParent(this);

    m_backupPlaylist = new Playlist();
    m_backupPlaylist->setParent(this);

    m_streamPlaylist = new Playlist();
    m_streamPlaylist->setParent(this);

    m_allPlaylists = new QList<Playlist*>;

    m_cdPlaylist.clear();

    m_activePlaylist->loadPlaylist("default_playlist_storage", m_myHost);

    m_backupPlaylist->loadPlaylist("backup_playlist_storage", m_myHost);

    m_streamPlaylist->loadPlaylist("stream_playlist", m_myHost);

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT playlist_id FROM music_playlists "
                  "WHERE playlist_name != :DEFAULT"
                  " AND playlist_name != :BACKUP "
                  " AND playlist_name != :STREAM "
                  " AND (hostname = '' OR hostname = :HOST) "
                  "ORDER BY playlist_name;");
    query.bindValue(":DEFAULT", "default_playlist_storage");
    query.bindValue(":BACKUP", "backup_playlist_storage");
    query.bindValue(":STREAM", "stream_playlist");
    query.bindValue(":HOST", m_myHost);

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
            temp_playlist->loadPlaylistByID(query.value(0).toInt(), m_myHost);
            m_allPlaylists->push_back(temp_playlist);
            //  ... cause it's sitting on this PtrList
        }
    }
    postLoad();

    m_pendingWritebackIndex = 0;

    int x = gCoreContext->GetNumSetting("LastMusicPlaylistPush");
    setPending(x);
    m_doneLoading = true;
}

void PlaylistContainer::describeYourself(void) const
{
    //    Debugging
    m_activePlaylist->describeYourself();
    QList<Playlist*>::const_iterator it = m_allPlaylists->begin();
    for (; it != m_allPlaylists->end(); ++it)
        (*it)->describeYourself();
}

Playlist *PlaylistContainer::getPlaylist(int id)
{
    //  return a pointer to a playlist
    //  by id;

    if (m_activePlaylist->getID() == id)
    {
        return m_activePlaylist;
    }

    QList<Playlist*>::iterator it = m_allPlaylists->begin();
    for (; it != m_allPlaylists->end(); ++it)
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

    QList<Playlist*>::iterator it = m_allPlaylists->begin();
    for (; it != m_allPlaylists->end(); ++it)
    {
        if ((*it)->getName() == name)
            return *it;
    }

    LOG(VB_GENERAL, LOG_ERR, QString("getPlaylistName() called with unknown name: %1").arg(name));
    return NULL;
}

void PlaylistContainer::save(void)
{
    QList<Playlist*>::const_iterator it = m_allPlaylists->begin();
    for (; it != m_allPlaylists->end(); ++it)
    {
        if ((*it)->hasChanged())
            (*it)->savePlaylist((*it)->getName(), m_myHost);
    }

    m_activePlaylist->savePlaylist("default_playlist_storage", m_myHost);
    m_backupPlaylist->savePlaylist("backup_playlist_storage", m_myHost);
    m_streamPlaylist->savePlaylist("stream_playlist", m_myHost);
}

void PlaylistContainer::createNewPlaylist(QString name)
{
    Playlist *new_list = new Playlist();
    new_list->setParent(this);

    //  Need to touch the database to get persistent ID
    new_list->savePlaylist(name, m_myHost);
    new_list->Changed();
    m_allPlaylists->push_back(new_list);
}

void PlaylistContainer::copyNewPlaylist(QString name)
{
    Playlist *new_list = new Playlist();
    new_list->setParent(this);

    //  Need to touch the database to get persistent ID
    new_list->savePlaylist(name, m_myHost);
    new_list->Changed();
    m_allPlaylists->push_back(new_list);
    m_activePlaylist->copyTracks(new_list, false);
    m_pendingWritebackIndex = 0;
}

void PlaylistContainer::popBackPlaylist()
{
    Playlist *destination = getPlaylist(m_pendingWritebackIndex);
    if (!destination)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "popBackPlaylist() " +
            QString("Unknown playlist: %1") .arg(m_pendingWritebackIndex));
        return;
    }
    destination->removeAllTracks();
    destination->Changed();
    m_activePlaylist->copyTracks(destination, false);
    m_activePlaylist->removeAllTracks();
    m_backupPlaylist->copyTracks(m_activePlaylist, true);
    m_pendingWritebackIndex = 0;

    m_activePlaylist->Changed();
    m_backupPlaylist->Changed();
}

void PlaylistContainer::copyToActive(int index)
{
    m_backupPlaylist->removeAllTracks();
    m_activePlaylist->copyTracks(m_backupPlaylist, false);

    m_pendingWritebackIndex = index;

    m_activePlaylist->removeAllTracks();
    Playlist *copy_from = getPlaylist(index);
    if (!copy_from)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "copyToActive() " +
            QString("Unknown playlist: %1").arg(index));
        return;
    }
    copy_from->copyTracks(m_activePlaylist, true);

    m_activePlaylist->Changed();
    m_backupPlaylist->Changed();
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

    if (kill_me == m_pendingWritebackIndex)
        popBackPlaylist();

    m_activePlaylist->removeTrack(kill_me * -1);

    QList<Playlist*>::iterator it = m_allPlaylists->begin();
    for (; it != m_allPlaylists->end(); ++it)
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
    m_allPlaylists->removeAll(list_to_kill);
}


QString PlaylistContainer::getPlaylistName(int index, bool &reference)
{
    if (m_activePlaylist)
    {
        if (m_activePlaylist->getID() == index)
        {
            return m_activePlaylist->getName();
        }

        QList<Playlist*>::iterator it = m_allPlaylists->begin();
        for (; it != m_allPlaylists->end(); ++it)
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
    m_activePlaylist->postLoad();
    m_backupPlaylist->postLoad();

    QList<Playlist*>::iterator it = m_allPlaylists->begin();
    for (; it != m_allPlaylists->end(); ++it)
        (*it)->postLoad();
#endif
}

bool PlaylistContainer::pendingWriteback()
{
    if (m_pendingWritebackIndex > 0)
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

    QList<Playlist*>::iterator it = m_allPlaylists->begin();
    for (; it != m_allPlaylists->end(); ++it)
    {
        if ((*it)->getName() == a_name && (*it)->getID() != which_id)
            return false;
    }

    return true;
}

QStringList PlaylistContainer::getPlaylistNames(void)
{
    QStringList res;

    QList<Playlist*>::iterator it = m_allPlaylists->begin();
    for (; it != m_allPlaylists->end(); ++it)
    {
        res.append((*it)->getName());
    }

    return res;
}

bool PlaylistContainer::cleanOutThreads()
{
    if (m_playlistsLoader->isFinished())
    {
        return true;
    }
    m_playlistsLoader->wait();
    return false;
}

void PlaylistContainer::clearActive()
{
    m_backupPlaylist->removeAllTracks();
    m_activePlaylist->removeAllTracks();
    m_backupPlaylist->Changed();
    m_activePlaylist->Changed();
    m_pendingWritebackIndex = 0;
}

