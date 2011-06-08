#include <mythcontext.h>
#include <mythdb.h>
#include <compat.h>

#include "playlistcontainer.h"
#include "mythlogging.h"

PlaylistLoadingThread::PlaylistLoadingThread(PlaylistContainer *parent_ptr,
                                             AllMusic *all_music_ptr)
{
    parent = parent_ptr;
    all_music = all_music_ptr;
}

void PlaylistLoadingThread::run()
{
    threadRegister("PlaylistLoading");
    while(!all_music->doneLoading())
    {
        sleep(1);
    }
    parent->load();
    threadDeregister();
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
    cd_playlist.remove(track);
}

bool PlaylistContainer::checkCDTrack(int track)
{
    list<int>::const_iterator it =
        find(cd_playlist.begin(), cd_playlist.end(), track);
    return it != cd_playlist.end();
}

PlaylistContainer::PlaylistContainer(AllMusic *all_music, const QString &host_name) :
    active_playlist(NULL),      backup_playlist(NULL),
    all_other_playlists(NULL),  all_available_music(all_music),
    active_widget(NULL),

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
    playlists_loader->deleteLater();

    if (active_playlist)
        delete active_playlist;
    if (backup_playlist)
        delete backup_playlist;
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
    active_playlist = new Playlist(all_available_music);
    active_playlist->setParent(this);

    backup_playlist = new Playlist(all_available_music);
    backup_playlist->setParent(this);

    all_other_playlists = new list<Playlist*>;

    cd_playlist.clear();

    active_playlist->loadPlaylist("default_playlist_storage", my_host);
    active_playlist->fillSongsFromSonglist(false);

    backup_playlist->loadPlaylist("backup_playlist_storage", my_host);
    backup_playlist->fillSongsFromSonglist(false);

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
            Playlist *temp_playlist = new Playlist(all_available_music);
            //  No, we don't destruct this ...
            temp_playlist->setParent(this);
            temp_playlist->loadPlaylistByID(query.value(0).toInt(), my_host);
            temp_playlist->fillSongsFromSonglist(false);
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
    list<Playlist*>::const_iterator it = all_other_playlists->begin();
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

    list<Playlist*>::iterator it = all_other_playlists->begin();
    for (; it != all_other_playlists->end(); ++it)
    {
        if ((*it)->getID() == id)
            return *it;
    }

    VERBOSE(VB_IMPORTANT, "getPlaylistName() called with unknown index number");
    return NULL;
}

GenericTree* PlaylistContainer::writeTree(GenericTree *tree_to_write_to)
{
    all_available_music->writeTree(tree_to_write_to);

    GenericTree *sub_node
        = tree_to_write_to->addNode(QObject::tr("All My Playlists"), 1);
    sub_node->setAttribute(0, 1);
    sub_node->setAttribute(1, 1);
    sub_node->setAttribute(2, 1);
    sub_node->setAttribute(3, 1);

    GenericTree *subsub_node
        = sub_node->addNode(QObject::tr("Active Play Queue"), 0);
    subsub_node->setAttribute(0, 0);
    subsub_node->setAttribute(1, 0);
    subsub_node->setAttribute(2, rand());
    subsub_node->setAttribute(3, rand());

    active_playlist->writeTree(subsub_node, 0);

    int a_counter = 0;

    //
    //  Write the CD playlist (if there's anything in it)
    //

/*
    if (cd_playlist.count() > 0)
    {
        ++a_counter;
        QString a_string = QObject::tr("CD: ");
        a_string += all_available_music->getCDTitle();
        GenericTree *cd_node = sub_node->addNode(a_string, 0);
        cd_node->setAttribute(0, 0);
        cd_node->setAttribute(1, a_counter);
        cd_node->setAttribute(2, rand());
        cd_node->setAttribute(3, rand());
    }
*/

    //
    //  Write the other playlists
    //

    list<Playlist*>::const_iterator it = all_other_playlists->begin();
    for (; it != all_other_playlists->end(); ++it)
    {
        ++a_counter;
        GenericTree *new_node = sub_node->addNode((*it)->getName(), (*it)->getID());
        new_node->setAttribute(0, 0);
        new_node->setAttribute(1, a_counter);
        new_node->setAttribute(2, rand());
        new_node->setAttribute(3, rand());
        (*it)->writeTree(new_node, 0);
    }

    GenericTree* active_playlist_node = subsub_node->findLeaf();
    if (!active_playlist_node) active_playlist_node = subsub_node;
    return active_playlist_node;
}

void PlaylistContainer::save(void)
{
    list<Playlist*>::const_iterator it = all_other_playlists->begin();
    for (; it != all_other_playlists->end(); ++it)
    {
        if ((*it)->hasChanged())
        {
            (*it)->fillSonglistFromSongs();
            (*it)->savePlaylist((*it)->getName(), my_host);
        }
    }

    active_playlist->savePlaylist("default_playlist_storage", my_host);
    backup_playlist->savePlaylist("backup_playlist_storage", my_host);
}

void PlaylistContainer::createNewPlaylist(QString name)
{
    Playlist *new_list = new Playlist(all_available_music);
    new_list->setParent(this);

    //  Need to touch the database to get persistent ID
    new_list->savePlaylist(name, my_host);
    new_list->Changed();
    all_other_playlists->push_back(new_list);
    //if (my_widget)
    //{
    //    new_list->putYourselfOnTheListView(my_widget);
    //}
}

void PlaylistContainer::copyNewPlaylist(QString name)
{
    Playlist *new_list = new Playlist(all_available_music);
    new_list->setParent(this);

    //  Need to touch the database to get persistent ID
    new_list->savePlaylist(name, my_host);
    new_list->Changed();
    all_other_playlists->push_back(new_list);
    active_playlist->copyTracks(new_list, false);
    pending_writeback_index = 0;
    active_widget->setText(QObject::tr("Active Play Queue"));
    active_playlist->removeAllTracks();
    active_playlist->addTrack(new_list->getID() * -1, true, false);
}

void PlaylistContainer::setActiveWidget(PlaylistTitle *widget)
{
    active_widget = widget;
    if (active_widget && pending_writeback_index > 0)
    {
        bool bad = false;
        QString newlabel = QString(QObject::tr("Active Play Queue (%1)"))
                           .arg(getPlaylistName(pending_writeback_index, bad));
        active_widget->setText(newlabel);
    }
}

void PlaylistContainer::popBackPlaylist()
{
    Playlist *destination = getPlaylist(pending_writeback_index);
    if (!destination)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "popBackPlaylist() " +
                QString("Unknown playlist: %1")
                .arg(pending_writeback_index));
        return;
    }
    destination->removeAllTracks();
    destination->Changed();
    active_playlist->copyTracks(destination, false);
    active_playlist->removeAllTracks();
    backup_playlist->copyTracks(active_playlist, true);
    pending_writeback_index = 0;
    active_widget->setText(QObject::tr("Active Play Queue"));

    active_playlist->Changed();
    backup_playlist->Changed();
}

void PlaylistContainer::copyToActive(int index)
{
    backup_playlist->removeAllTracks();
    active_playlist->copyTracks(backup_playlist, false);

    pending_writeback_index = index;
    if (active_widget)
    {
        bool bad = false;
        QString newlabel = QString(QObject::tr("Active Play Queue (%1)"))
            .arg(getPlaylistName(index, bad));
        active_widget->setText(newlabel);
    }
    active_playlist->removeAllTracks();
    Playlist *copy_from = getPlaylist(index);
    if (!copy_from)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "copyToActive() " +
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
        if (list_to_rename->getID() == pending_writeback_index)
        {
            QString newlabel = QString(QObject::tr("Active Play Queue (%1)"))
                .arg(new_name);
            active_widget->setText(newlabel);
        }
    }
}

void PlaylistContainer::deletePlaylist(int kill_me)
{
    Playlist *list_to_kill = getPlaylist(kill_me);
    if (!list_to_kill)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "deletePlaylist() " +
                QString("Unknown playlist: %1").arg(kill_me));
        return;
    }
    //  First, we need to take out any **track** on any other
    //  playlist that is actually a reference to this
    //  playlist

    if (kill_me == pending_writeback_index)
        popBackPlaylist();

    active_playlist->removeTrack(kill_me * -1, false);

    list<Playlist*>::iterator it = all_other_playlists->begin();
    for (; it != all_other_playlists->end(); ++it)
    {
        if ((*it) != list_to_kill)
            (*it)->removeTrack(kill_me * -1, false);
    }

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM music_playlists WHERE playlist_id = :ID ;");
    query.bindValue(":ID", kill_me);

    if (!query.exec() || query.numRowsAffected() < 1)
    {
        MythDB::DBError("playlist delete", query);
    }
    list_to_kill->removeAllTracks();
    all_other_playlists->remove(list_to_kill);
}


QString PlaylistContainer::getPlaylistName(int index, bool &reference)
{
    if (active_playlist)
    {
        if (active_playlist->getID() == index)
        {
            return active_playlist->getName();
        }

        list<Playlist*>::reverse_iterator it = all_other_playlists->rbegin();
        for (; it != all_other_playlists->rend(); it++)
        {
            if ((*it)->getID() == index)
                return (*it)->getName();
        }
    }

    VERBOSE(VB_IMPORTANT, LOC_ERR +
            "getPlaylistName() called with unknown index number");

    reference = true;
    return QObject::tr("Something is Wrong");
}

void PlaylistContainer::showRelevantPlaylists(TreeCheckItem *alllists)
{
    QString templevel, temptitle;
    int id;
    //  Deleting anything that's there
    while (alllists->childCount() > 0)
    {
        UIListGenericTree *first_child;
        first_child = (UIListGenericTree *)(alllists->getChildAt(0));
        {
            first_child->RemoveFromParent();
            //delete first_child;  Deleted by GenericTree.
        }
    }

    //  Add everything but the current playlist
    list<Playlist*>::iterator it = all_other_playlists->begin();
    for (; it != all_other_playlists->end(); ++it)
    {
        id = (*it)->getID() * -1 ;
        temptitle = (*it)->getName();
        templevel = "playlist";

        TreeCheckItem *some_item = new TreeCheckItem(alllists, temptitle,
                                                     templevel, id);

        some_item->setCheckable(true);
        some_item->setActive(true);

        if ((*it)->containsReference(pending_writeback_index, 0) ||
            (id * -1) == pending_writeback_index)
        {
            some_item->setCheckable(false);
            some_item->setActive(false);
        }

        (*it)->putYourselfOnTheListView(some_item);
    }

    if (alllists->childCount() == 0)
        alllists->setCheckable(false);
    else
        alllists->setCheckable(true);
}

void PlaylistContainer::refreshRelevantPlaylists(TreeCheckItem *alllists)
{
    if (alllists->childCount() == 0)
    {
        alllists->setCheckable(false);
        return;
    }

    UIListGenericTree *walker = (UIListGenericTree *)(alllists->getChildAt(0));
    while (walker)
    {
        if (TreeCheckItem *check_item = dynamic_cast<TreeCheckItem*>(walker))
        {
            int id = check_item->getID() * -1;
            Playlist *check_playlist = getPlaylist(id);
            if ((check_playlist &&
                check_playlist->containsReference(pending_writeback_index, 0))
               || id == pending_writeback_index)
            {
                check_item->setCheckable(false);
                check_item->setActive(false);
            }
            else
            {
                check_item->setCheckable(true);
                check_item->setActive(true);
            }
        }
        walker = (UIListGenericTree *)(walker->nextSibling(1));
    }

    alllists->setCheckable(true);
}

void PlaylistContainer::postLoad()
{
    //  Now that everything is loaded, we need to recheck all
    //  tracks and update those that refer to a playlist

    active_playlist->postLoad();
    backup_playlist->postLoad();

    list<Playlist*>::iterator it = all_other_playlists->begin();
    for (; it != all_other_playlists->end(); ++it)
        (*it)->postLoad();
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

    list<Playlist*>::iterator it = all_other_playlists->begin();
    for (; it != all_other_playlists->end(); ++it)
    {
        if ((*it)->getName() == a_name && (*it)->getID() != which_id)
            return false;
    }

    return true;
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
    active_widget->setText(QObject::tr("Active Play Queue"));
}

