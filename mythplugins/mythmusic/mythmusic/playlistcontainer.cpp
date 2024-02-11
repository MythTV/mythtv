// C/C++
#include <utility>

// MythTV
#include <libmyth/mythcontext.h>
#include <libmythbase/compat.h>
#include <libmythbase/mythdb.h>
#include <libmythbase/mythlogging.h>

// mythmusic
#include "playlist.h"
#include "playlistcontainer.h"

void PlaylistLoadingThread::run()
{
    RunProlog();
    while (!m_allMusic->doneLoading())
    {
        usleep(250ms); // cppcheck-suppress usleepCalled
    }
    m_parent->load();
    RunEpilog();
}

#define LOC      QString("PlaylistContainer: ")
#define LOC_WARN QString("PlaylistContainer, Warning: ")
#define LOC_ERR  QString("PlaylistContainer, Error: ")

PlaylistContainer::PlaylistContainer(AllMusic *all_music) :
    m_playlistsLoader(new PlaylistLoadingThread(this, all_music)),
    m_myHost(gCoreContext->GetHostName()),

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
    m_playlistsLoader = nullptr;

    delete m_activePlaylist;
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
                                            int &lastplay, int &random) const
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

    m_streamPlaylist = new Playlist();
    m_streamPlaylist->setParent(this);

    m_allPlaylists = new QList<Playlist*>;

    m_activePlaylist->loadPlaylist(DEFAULT_PLAYLIST_NAME, m_myHost);

    m_streamPlaylist->loadPlaylist(DEFAULT_STREAMLIST_NAME, m_myHost);

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT playlist_id FROM music_playlists "
                  "WHERE playlist_name != :DEFAULT"
                  " AND playlist_name != :BACKUP "
                  " AND playlist_name != :STREAM "
                  " AND (hostname = '' OR hostname = :HOST) "
                  "ORDER BY playlist_name;");
    query.bindValue(":DEFAULT", DEFAULT_PLAYLIST_NAME);
    query.bindValue(":BACKUP", "backup_playlist_storage");
    query.bindValue(":STREAM", DEFAULT_STREAMLIST_NAME);
    query.bindValue(":HOST", m_myHost);

    if (!query.exec())
    {
        MythDB::DBError("Querying playlists", query);
    }
    else
    {
        while (query.next())
        {
            auto *temp_playlist = new Playlist();
            //  No, we don't destruct this ...
            temp_playlist->setParent(this);
            temp_playlist->loadPlaylistByID(query.value(0).toInt(), m_myHost);
            m_allPlaylists->push_back(temp_playlist);
            //  ... cause it's sitting on this PtrList
        }
    }

    m_doneLoading = true;
}

// resync all the playlists after a rescan just in case some tracks were removed
void PlaylistContainer::resync(void)
{
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (auto it = m_allPlaylists->begin(); it != m_allPlaylists->end(); ++it)
        (*it)->resync();

    m_activePlaylist->resync();
}

void PlaylistContainer::describeYourself(void) const
{
    //    Debugging
    m_activePlaylist->describeYourself();
    for (const auto & playlist : std::as_const(*m_allPlaylists))
        playlist->describeYourself();
}

Playlist *PlaylistContainer::getPlaylist(int id)
{
    //  return a pointer to a playlist
    //  by id;

    if (m_activePlaylist->getID() == id)
    {
        return m_activePlaylist;
    }

    auto idmatch = [id](const auto & playlist)
        { return playlist->getID() == id; };
    auto it = std::find_if(m_allPlaylists->cbegin(), m_allPlaylists->cend(), idmatch);
    if (it != m_allPlaylists->cend())
        return *it;

    LOG(VB_GENERAL, LOG_ERR,
        "getPlaylistName() called with unknown index number");
    return nullptr;
}

Playlist *PlaylistContainer::getPlaylist(const QString &name)
{
    //  return a pointer to a playlist
    //  by name;
    auto namematch = [name](const auto & playlist)
        { return playlist->getName() == name; };
    auto it = std::find_if(m_allPlaylists->cbegin(), m_allPlaylists->cend(), namematch);
    if (it != m_allPlaylists->cend())
        return *it;

    LOG(VB_GENERAL, LOG_ERR, QString("getPlaylistName() called with unknown name: %1").arg(name));
    return nullptr;
}

void PlaylistContainer::save(void)
{
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (auto it = m_allPlaylists->begin(); it != m_allPlaylists->end(); ++it)
    {
        if ((*it)->hasChanged())
            (*it)->savePlaylist((*it)->getName(), m_myHost);
    }

    m_activePlaylist->savePlaylist(DEFAULT_PLAYLIST_NAME, m_myHost);
    m_streamPlaylist->savePlaylist(DEFAULT_STREAMLIST_NAME, m_myHost);
}

void PlaylistContainer::createNewPlaylist(const QString &name)
{
    auto *new_list = new Playlist();
    new_list->setParent(this);

    //  Need to touch the database to get persistent ID
    new_list->savePlaylist(name, m_myHost);

    m_allPlaylists->push_back(new_list);
}

void PlaylistContainer::copyNewPlaylist(const QString &name)
{
    auto *new_list = new Playlist();
    new_list->setParent(this);

    //  Need to touch the database to get persistent ID
    new_list->savePlaylist(name, m_myHost);

    m_allPlaylists->push_back(new_list);
    m_activePlaylist->copyTracks(new_list, false);
}

void PlaylistContainer::copyToActive(int index)
{
    m_activePlaylist->removeAllTracks();
    Playlist *copy_from = getPlaylist(index);
    if (!copy_from)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "copyToActive() " +
            QString("Unknown playlist: %1").arg(index));
        return;
    }
    copy_from->copyTracks(m_activePlaylist, true);
}

void PlaylistContainer::renamePlaylist(int index, const QString& new_name)
{
    Playlist *list_to_rename = getPlaylist(index);
    if (list_to_rename)
    {
        list_to_rename->setName(new_name);
        list_to_rename->changed();
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

    list_to_kill->removeAllTracks();
    m_allPlaylists->removeAll(list_to_kill);

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM music_playlists WHERE playlist_id = :ID ;");
    query.bindValue(":ID", kill_me);

    if (!query.exec() || query.numRowsAffected() < 1)
    {
        MythDB::DBError("playlist delete", query);
    }
}


QString PlaylistContainer::getPlaylistName(int index, bool &reference)
{
    if (m_activePlaylist)
    {
        if (m_activePlaylist->getID() == index)
        {
            return m_activePlaylist->getName();
        }

        auto indexmatch = [index](const auto & playlist)
            { return playlist->getID() == index; };
        auto it = std::find_if(m_allPlaylists->cbegin(), m_allPlaylists->cend(), indexmatch);
        if (it != m_allPlaylists->cend())
            return (*it)->getName();
    }

    LOG(VB_GENERAL, LOG_ERR, LOC +
        "getPlaylistName() called with unknown index number");

    reference = true;
    return tr("Something is Wrong");
}

bool PlaylistContainer::nameIsUnique(const QString& a_name, int which_id)
{
    if (a_name == DEFAULT_PLAYLIST_NAME)
        return false;

    auto itemfound = [a_name,which_id](const auto & playlist)
        { return playlist->getName() == a_name &&
                 playlist->getID() != which_id; };
    return std::none_of(m_allPlaylists->cbegin(), m_allPlaylists->cend(), itemfound);
}

QStringList PlaylistContainer::getPlaylistNames(void)
{
    QStringList res;

    for (const auto & playlist : std::as_const(*m_allPlaylists))
    {
        res.append(playlist->getName());
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
    m_activePlaylist->removeAllTracks();
}
