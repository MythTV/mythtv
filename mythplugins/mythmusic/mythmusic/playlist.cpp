#include <unistd.h>
#include <inttypes.h>
#include <iostream>
using namespace std;
#include "playlist.h"
#include "qdatetime.h"
#include <mythtv/mythcontext.h>
#include "smartplaylist.h"
#include <mythtv/mythdbcon.h>

#include <qfileinfo.h>
#include <qprocess.h>
#include <qapplication.h>

Track::Track(int x, AllMusic *all_music_ptr)
{
    index_value = x;
    all_available_music = all_music_ptr;
    my_widget = NULL;
    parent = NULL;    
    bad_reference = false;
    label = QObject::tr("Not Initialized");
    cd_flag = false;
}

void Track::postLoad(PlaylistsContainer *grandparent)
{
    if (cd_flag)
    {
        int val = (index_value < 0) ? -index_value : index_value;
        label = all_available_music->getLabel(val, &bad_reference);
        return;
    }

    if (index_value > 0) // Normal Track
        label = all_available_music->getLabel(index_value, &bad_reference);
    else if (index_value < 0)
        label = grandparent->getPlaylistName( (-1 * index_value),  bad_reference);
    else
    {
        cerr << "playlist.o: Not sure how I got 0 as a track number, but "
            "it ain't good" << endl;
    }
}

void Track::setParent(Playlist *parent_ptr)
{
    //  I'm a track, what's my playlist?
    parent = parent_ptr;
}

void Playlist::postLoad()
{
    Track *it;
    for (it = songs.first(); it; it = songs.current())
    {
        it->postLoad(parent);
        if (it->badReference())
        {
            songs.remove(it);
            Changed();
        }
        else
            it = songs.next();
    }  
}

bool Playlist::checkTrack(int a_track_id, bool cd_flag)
{
    // XXX SPEED THIS UP
    // Should be a straight lookup against cached index
    Track *it;

    for (it = songs.first(); it; it = songs.next())
    {
        if (it->getValue() == a_track_id && it->getCDFlag() == cd_flag)
        {
            return true;
        }
    }  

    return false;
}

void Playlist::copyTracks(Playlist *to_ptr, bool update_display)
{
    Track *it;
    for (it = songs.first(); it; it = songs.next())
    {
        if (!it->getCDFlag())
            to_ptr->addTrack((*it).getValue(), update_display, false);
    }  
}



void Playlist::addTrack(int the_track, bool update_display, bool cd)
{
    // Given a track id number, add that track to this playlist

    Track *a_track = new Track(the_track, all_available_music);
    a_track->setCDFlag(cd);
    a_track->postLoad(parent);
    a_track->setParent(this);
    songs.append(a_track);
    changed = true;

    // If I'm part of a GUI, display the existence of this new track

    if (!update_display)
        return;

    PlaylistTitle *which_widget = parent->getActiveWidget();

    if (which_widget)
        a_track->putYourselfOnTheListView(which_widget);
}

void Track::deleteYourself()
{
    parent->removeTrack(index_value, cd_flag);
}

void Track::deleteYourWidget()
{
    if (my_widget)
    {
       my_widget->RemoveFromParent();
    //    delete my_widget;  Deleted by the parent..
        my_widget = NULL;
    }
}

void Playlist::removeAllTracks()
{
    Track *it;
    for (it = songs.first(); it; it = songs.current())
    {
        it->deleteYourWidget();
        songs.remove(it);
    }

    changed = true;
}

void Playlist::removeAllWidgets()
{
    Track *it;
    for (it = songs.first(); it; it = songs.next())
    {
        it->deleteYourWidget();
    }
}

void Playlist::ripOutAllCDTracksNow()
{
    Track *it;
    for (it = songs.first(); it; it = songs.current())
    {
        if (it->getCDFlag())
        {
            it->deleteYourWidget();
            songs.remove(it);
        }
        else
            it = songs.next();
    }  
    changed = true;
}

void Playlist::removeTrack(int the_track, bool cd_flag)
{
    // Should be a straight lookup against cached index
    Track *it;
    for (it = songs.first(); it; it = songs.current())
    {
        if (it->getValue() == the_track && cd_flag == it->getCDFlag())
        {
            it->deleteYourWidget();
            songs.remove(it);
            //it = songs.last();
        }
        else
            it = songs.next();
    }  
    changed = true;
}

void Playlist::moveTrackUpDown(bool flag, Track *the_track)
{
    // Slightly complicated, as the PtrList owns the pointers
    // Need to turn off auto delete
    
    songs.setAutoDelete(false);    

    uint insertion_point = 0;
    int where_its_at = songs.findRef(the_track);
    if (where_its_at < 0)
        cerr << "playlist.o: A playlist was asked to move a track, but can'd "
                "find it\n";
    else
    {
        if (flag)
            insertion_point = ((uint)where_its_at) - 1;
        else
            insertion_point = ((uint)where_its_at) + 1;
    
        songs.remove(the_track);
        songs.insert(insertion_point, the_track);
    }
    
    songs.setAutoDelete(true);   
    changed = true; //  This playlist is now different than Database
}

void Track::moveUpDown(bool flag)
{
    parent->moveTrackUpDown(flag, this);
}

PlaylistLoadingThread::PlaylistLoadingThread(PlaylistsContainer *parent_ptr,
                                             AllMusic *all_music_ptr)
{
    parent = parent_ptr;
    all_music = all_music_ptr;
}

void PlaylistLoadingThread::run()
{
    while(!all_music->doneLoading())
    {
        sleep(1);
    }
    parent->load();
}

void PlaylistsContainer::clearCDList()
{
    cd_playlist.clear();
}

void PlaylistsContainer::addCDTrack(int track)
{
    cd_playlist.append(track);
}

void PlaylistsContainer::removeCDTrack(int track)
{
    cd_playlist.remove(track);
}

bool PlaylistsContainer::checkCDTrack(int track)
{
    for (int i = 0; i < (int)cd_playlist.count(); i++)
    {
        if (cd_playlist[i] == track)
            return true;
    }    
    return false;
}

PlaylistsContainer::PlaylistsContainer(AllMusic *all_music, QString host_name)
{
    active_widget = NULL;
    my_host = host_name;
 
    active_playlist = NULL;
    backup_playlist = NULL;
    all_other_playlists = NULL;

    all_available_music = all_music;

    done_loading = false;

    RatingWeight = gContext->GetNumSetting("IntelliRatingWeight", 2);
    PlayCountWeight = gContext->GetNumSetting("IntelliPlayCountWeight", 2);
    LastPlayWeight = gContext->GetNumSetting("IntelliLastPlayWeight", 2);
    RandomWeight = gContext->GetNumSetting("IntelliRandomWeight", 2);

    playlists_loader = new PlaylistLoadingThread(this, all_music);
    playlists_loader->start();
}

PlaylistsContainer::~PlaylistsContainer()
{
    if (active_playlist)
        delete active_playlist;
    if (backup_playlist)
        delete backup_playlist;
    if (all_other_playlists)
        delete all_other_playlists;

    playlists_loader->wait();
    delete playlists_loader;
}

void PlaylistsContainer::FillIntelliWeights(int &rating, int &playcount,
                                            int &lastplay, int &random)
{
    rating = RatingWeight;
    playcount = PlayCountWeight;
    lastplay = LastPlayWeight;
    random = RandomWeight;
}

void PlaylistsContainer::load()
{
    done_loading = false;
    active_playlist = new Playlist(all_available_music);
    active_playlist->setParent(this);
    
    backup_playlist = new Playlist(all_available_music);
    backup_playlist->setParent(this);
    
    all_other_playlists = new QPtrList<Playlist>;
    all_other_playlists->setAutoDelete(true);
    
    cd_playlist.clear();

    active_playlist->loadPlaylist("default_playlist_storage", my_host);
    active_playlist->fillSongsFromSonglist(false);
    
    backup_playlist->loadPlaylist("backup_playlist_storage", my_host);
    backup_playlist->fillSongsFromSonglist(false);

    all_other_playlists->clear();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT playlist_id FROM music_playlists "
                  "WHERE playlist_name != :DEFAULT"
                  " AND playlist_name != :BACKUP "
                  " AND (hostname = '' OR hostname = :HOST) "
                  "ORDER BY playlist_id;");
    query.bindValue(":DEFAULT", "default_playlist_storage");
    query.bindValue(":BACKUP", "backup_playlist_storage"); 
    query.bindValue(":HOST", my_host);

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        while (query.next())
        {
            Playlist *temp_playlist = new Playlist(all_available_music);   //  No, we don't destruct this ...
            temp_playlist->setParent(this);
            temp_playlist->loadPlaylistByID(query.value(0).toInt(), my_host);
            temp_playlist->fillSongsFromSonglist(false);
            all_other_playlists->append(temp_playlist); //  ... cause it's sitting on this PtrList
        }
    }
    postLoad();
   
    pending_writeback_index = 0;

    int x = gContext->GetNumSetting("LastMusicPlaylistPush");
    setPending(x);
    done_loading = true;
}

void PlaylistsContainer::describeYourself()
{
    //    Debugging
        
    active_playlist->describeYourself();
    Playlist *a_list;
    for (a_list = all_other_playlists->first(); a_list; 
         a_list = all_other_playlists->next())
    {
        a_list->describeYourself();
    }
}

Playlist::Playlist(AllMusic *all_music_ptr)
{
    //  fallback values
    playlistid = 0;
    name = QObject::tr("oops");
    raw_songlist = "";
    songs.setAutoDelete(true);  //  mine!
    all_available_music = all_music_ptr;
    changed = false;
}

void Track::putYourselfOnTheListView(UIListGenericTree *a_listviewitem)
{
    if (cd_flag)
    {
        my_widget = new PlaylistCD(a_listviewitem, label);
        my_widget->setOwner(this); 
    }
    else
    {
        if (index_value > 0)
        {
            my_widget = new PlaylistTrack(a_listviewitem, label);
            my_widget->setOwner(this); 
        }
        else if (index_value < 0)
        {
            my_widget = new PlaylistPlaylist(a_listviewitem, label);
            my_widget->setOwner(this); 
        }
    }
}

void Playlist::putYourselfOnTheListView(UIListGenericTree *a_listviewitem)
{
    Track *it;
    for (it = songs.first(); it; it = songs.next())
        it->putYourselfOnTheListView(a_listviewitem);
}

int Playlist::getFirstTrackID()
{
    Track *it = songs.first();
    if (it)
    {
        return it->getValue();
    }
    return 0;
}

Playlist::~Playlist()
{
    songs.setAutoDelete(true);
    songs.clear();
}

Playlist& Playlist::operator=(const Playlist& rhs)
{
    if (this == &rhs)
    {
        return *this;
    }
    
    playlistid = rhs.playlistid;
    name = rhs.name;
    raw_songlist = rhs.raw_songlist;
    songs = rhs.songs;
    return *this;
}

void Playlist::describeYourself()
{
    //  This is for debugging
/*
    cout << "Playlist with name of \"" << name << "\"" << endl;
    cout << "        playlistid is " << playlistid << endl;
    cout << "     songlist(raw) is \"" << raw_songlist << "\"" << endl;
    cout << "     songlist list is ";
*/

    Track *it;
    for(it = songs.first(); it; it = songs.next())
    {
        cout << it->getValue() << "," ;
    }  

    cout << endl;
}


void Playlist::loadPlaylist(QString a_name, QString a_host)
{
    QString thequery;
    if (a_host.length() < 1)
    {
        cerr << "playlist.o: Hey! I can't load playlists if you don't give "
                "me a hostname!" << endl; 
        return;
    }
   
    MSqlQuery query(MSqlQuery::InitCon());

    if (name == "default_playlist_storage" || name == "backup_playlist_storage")
    {
        query.prepare("SELECT playlist_id, playlist_name, playlist_songs "
                      "FROM  music_playlists "
                      "WHERE playlist_name = :NAME"
                      " AND hostname = :HOST;");
    }
    else
    {
        // Technically this is never called as this function is only used to load
        // the default/backup playlists.
        query.prepare("SELECT playlist_id, playlist_name, playlist_songs "
                      "FROM music_playlists "
                      "WHERE playlist_name = :NAME"
                      " AND (hostname = '' OR hostname = :HOST);");
    }
    query.bindValue(":NAME", a_name.utf8());
    query.bindValue(":HOST", a_host);

    if (query.exec() && query.size() > 0)
    {
        while (query.next())
        {
            playlistid = query.value(0).toInt();
            name = QString::fromUtf8(query.value(1).toString());
            raw_songlist = query.value(2).toString();
        }
        if (name == "default_playlist_storage")
            name = "the user should never see this";
        if (name == "backup_playlist_storage")
            name = "and they should **REALLY** never see this";
    }
    else
    {
        // Asked me to load a playlist I can't find so let's create a new one :)
        playlistid = 0; // Be safe just in case we call load over the top
                        // of an existing playlist
        raw_songlist = "";
        savePlaylist(a_name, a_host);
        changed = true;
    }
}

void Playlist::loadPlaylistByID(int id, QString a_host)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT playlist_id, playlist_name, playlist_songs "
                  "FROM music_playlists "
                  "WHERE playlist_id = :ID"
                  " AND (hostname = '' OR hostname = :HOST);");
    query.bindValue(":ID", id);
    query.bindValue(":HOST", a_host);

    query.exec();

    while (query.next())
    {
        playlistid = query.value(0).toInt();
        name = QString::fromUtf8(query.value(1).toString());
        raw_songlist = query.value(2).toString();
    }

    if (name == "default_playlist_storage")
        name = "the user should never see this";
    if (name == "backup_playlist_storage")
        name = "and they should **REALLY** never see this";
}

void Playlist::fillSongsFromSonglist(QString songList, bool filter)
{
    raw_songlist = songList;
    fillSongsFromSonglist(filter);
}

void Playlist::fillSongsFromSonglist(bool filter)
{
    int an_int;

    if (filter)
        all_available_music->setAllVisible(false);

    QStringList list = QStringList::split(",", raw_songlist);
    QStringList::iterator it = list.begin();
    for (; it != list.end(); it++)
    {
        an_int = QString(*it).toInt();
        if (an_int != 0)
        {
            if (filter)
            {
                Metadata *md = all_available_music->getMetadata(an_int);
                if(md)
                    md->setVisible(true);
            }
            else
            {
                Track *a_track = new Track(an_int, all_available_music);
                a_track->setParent(this);
                songs.append(a_track);
            }
        }
        else
        {
            changed = true;
            cerr << "playlist.o: Taking a 0 (zero) off a playlist" << endl;
            cerr << "            If this happens on repeated invocations of mythmusic, then something is really wrong" << endl;
        }
    }

    if (filter)
    {
        all_available_music->buildTree();
        all_available_music->sortTree();
    }
}

void Playlist::fillSonglistFromSongs()
{
    QString a_list = "";
    Track *it;
    for (it = songs.first(); it; it = songs.next())
    {
        if (!it->getCDFlag())
        {
            a_list += QString(",%1").arg(it->getValue());
        }
    }

    raw_songlist = "";
    if (a_list.length() > 1)
        raw_songlist = a_list.remove(0, 1);
}

void Playlist::fillSonglistFromQuery(QString whereClause,
                                     bool removeDuplicates,
                                     InsertPLOption insertOption,
                                     int currentTrackID)
{
    QString orig_songlist;

    if (insertOption != PL_FILTERONLY)
        removeAllTracks();

    MSqlQuery query(MSqlQuery::InitCon());

    QString theQuery;

    theQuery = "SELECT song_id FROM music_songs "
               "LEFT JOIN music_artists ON music_songs.artist_id=music_artists.artist_id "
               "LEFT JOIN music_albums ON music_songs.album_id=music_albums.album_id "
               "LEFT JOIN music_genres ON music_songs.genre_id=music_genres.genre_id ";

    if (whereClause.length() > 0)
      theQuery += whereClause;

    if (!query.exec(theQuery))
    {
        MythContext::DBError("Load songlist from query", query);
        raw_songlist = "";
        return;
    }

    QString new_songlist = "";
    while (query.next())
    {
        new_songlist += "," + query.value(0).toString();
    }
    new_songlist.remove(0, 1);

    if (insertOption != PL_FILTERONLY && removeDuplicates)
    {
        new_songlist = removeDuplicateTracks(new_songlist);
    }

    switch (insertOption)
    {
        case PL_REPLACE:
            raw_songlist = new_songlist;
            break;

        case PL_INSERTATBEGINNING:
            raw_songlist = new_songlist + "," + raw_songlist;
            break;

        case PL_INSERTATEND:
            raw_songlist = raw_songlist + "," + new_songlist;
            break;

        case PL_INSERTAFTERCURRENT:
        {
            QStringList list = QStringList::split(",", raw_songlist);
            QStringList::iterator it = list.begin();
            raw_songlist = "";
            bool bFound = false;

            for (; it != list.end(); it++)
            {
                int an_int = QString(*it).toInt();
                raw_songlist += "," + QString(*it);
                if (!bFound && an_int == currentTrackID)
                {
                    bFound = true;
                    raw_songlist += "," + new_songlist;
                }
            }

            if (!bFound)
            {
                raw_songlist += "," + new_songlist;
            }

            raw_songlist.remove(0, 1);

            break;
        }

        case PL_FILTERONLY:
            orig_songlist = raw_songlist;
            raw_songlist = new_songlist;
            break;

        default:
            raw_songlist = new_songlist;
    }

    if (insertOption != PL_FILTERONLY)
    {
        fillSongsFromSonglist(false);
        postLoad();
    }
    else
    {
        fillSongsFromSonglist(true);
        raw_songlist = orig_songlist;
    }
}

void Playlist::fillSongsFromCD()
{
    for (int i = 1; i <= all_available_music->getCDTrackCount(); i++)
       addTrack(-1 * i, true, true);
}

void Playlist::fillSonglistFromSmartPlaylist(QString category, QString name,
                                             bool removeDuplicates,
                                             InsertPLOption insertOption,
                                             int currentTrackID)
{
    MSqlQuery query(MSqlQuery::InitCon());

    // find the correct categoryid
    int categoryID = SmartPlaylistEditor::lookupCategoryID(category);
    if (categoryID == -1)
    {
        cout << "Cannot find Smartplaylist Category: " << category << endl;
        return;
    }
    
    // find smartplaylist
    int ID;
    QString matchType;
    QString orderBy; 
    int limitTo;
    
    query.prepare("SELECT smartplaylistid, matchtype, orderby, limitto "
                  "FROM music_smartplaylists WHERE categoryid = :CATEGORYID AND name = :NAME;");
    query.bindValue(":NAME", name.utf8());
    query.bindValue(":CATEGORYID", categoryID);
        
    if (query.exec())
    {
        if (query.isActive() && query.size() > 0)
        {
            query.first();
            ID = query.value(0).toInt();
            matchType = (query.value(1).toString() == "All") ? " AND " : " OR ";
            orderBy = QString::fromUtf8(query.value(2).toString());
            limitTo = query.value(3).toInt();
        }
        else
        {
            cout << "Cannot find smartplaylist: " << name << endl;
            return;
        }
    }
    else
    {
        MythContext::DBError("Find SmartPlaylist", query);
        return;   
    }
    
    // get smartplaylist items
    QString whereClause = "WHERE ";
    
    query.prepare("SELECT field, operator, value1, value2 "
                  "FROM music_smartplaylist_items WHERE smartplaylistid = :ID;");
    query.bindValue(":ID", ID);
    query.exec();
    if (query.isActive() && query.size() > 0)
    {
        bool bFirst = true;
        while (query.next())
        {
            QString fieldName = QString::fromUtf8(query.value(0).toString());
            QString operatorName = QString::fromUtf8(query.value(1).toString());
            QString value1 = QString::fromUtf8(query.value(2).toString());
            QString value2 = QString::fromUtf8(query.value(3).toString());
            if (!bFirst)
                whereClause += matchType + getCriteriaSQL(fieldName, operatorName, value1, value2); 
            else
            {
               bFirst = false;
               whereClause += " " + getCriteriaSQL(fieldName, operatorName, value1, value2);
            }
        }
    }

    // add order by clause
    whereClause += getOrderBySQL(orderBy);

    // add limit
    if (limitTo > 0)
        whereClause +=  " LIMIT " + QString::number(limitTo);

    fillSonglistFromQuery(whereClause, removeDuplicates, insertOption, currentTrackID);
}

void Playlist::savePlaylist(QString a_name, QString a_host)
{
    name = a_name.simplifyWhiteSpace();
    if (name.length() < 1)
    {
        cerr << "playlist.o: Not going to save a playlist with no name" << endl ;
        return;
    }

    if (a_host.length() < 1)
    {
        cerr << "playlist.o: Not going to save a playlist with no hostname" << endl;
        return;
    }
    if (name.length() < 1)
        return;

    fillSonglistFromSongs();
    MSqlQuery query(MSqlQuery::InitCon());

    int songcount = 0, playtime = 0, an_int;
    QStringList list = QStringList::split(",", raw_songlist);
    QStringList::iterator it = list.begin();
    for (; it != list.end(); it++)
    {
        an_int = QString(*it).toInt();
        if (an_int != 0)
        {
            songcount++;
            if (an_int > 0)
            {
                query.prepare("SELECT length FROM music_songs WHERE song_id = :ID ;");
            }
            else
            {
                query.prepare("SELECT length FROM music_playlists WHERE playlist_id = :ID ;");
                an_int *= -1;
            }
            query.bindValue(":ID", an_int);
            query.exec();
            if (query.size() > 0)
            {
                query.next();
                playtime += query.value(0).toInt();
            }
        }
    }

    bool save_host = ("default_playlist_storage" == a_name || "backup_playlist_storage" == a_name);
    if (playlistid > 0)
    {
        QString str_query = "UPDATE music_playlists SET playlist_songs = :LIST,"
                            " playlist_name = :NAME, songcount = :SONGCOUNT, length = :PLAYTIME";
        if (save_host)
            str_query += ", hostname = :HOSTNAME";
        str_query += " WHERE playlist_id = :ID ;";

        query.prepare(str_query);
        query.bindValue(":ID", playlistid);
    }
    else
    {
        QString str_query = "INSERT INTO music_playlists"
                            " (playlist_name, playlist_songs, songcount, length";
        if (save_host)
            str_query += ", hostname";
        str_query += ") VALUES(:NAME, :LIST, :SONGCOUNT, :PLAYTIME";
        if (save_host)
            str_query += ", :HOSTNAME";
        str_query += ");";

        query.prepare(str_query);
    }
    query.bindValue(":LIST", raw_songlist);
    query.bindValue(":NAME", a_name.utf8());
    query.bindValue(":SONGCOUNT", songcount);
    query.bindValue(":PLAYTIME", playtime);
    if (save_host)
        query.bindValue(":HOSTNAME", a_host);

    if (!query.exec() || (playlistid < 1 && query.numRowsAffected() < 1))
    {
        MythContext::DBError("Problem saving playlist", query);
    }

    if (playlistid < 1)
        playlistid = query.lastInsertId().toInt();
}

QString Playlist::removeDuplicateTracks(const QString &new_songlist)
{
    raw_songlist.remove(' ');

    QStringList curList = QStringList::split(",", raw_songlist);
    QStringList newList = QStringList::split(",", new_songlist);
    QStringList::iterator it = newList.begin();
    QString songlist = "";

    for (; it != newList.end(); it++)
    {
        if (curList.find(QString(*it)) == curList.end())
            songlist += "," + QString(*it);
    }
    songlist.remove(0, 1);
    return songlist;
}

int Playlist::writeTree(GenericTree *tree_to_write_to, int a_counter)
{
    Track *it;

    // compute max/min playcount,lastplay for this playlist
    int playcountMin = 0;
    int playcountMax = 0;
    double lastplayMin = 0.0;
    double lastplayMax = 0.0;

    typedef map<QString, uint32_t> AlbumMap;
    AlbumMap                       album_map;
    AlbumMap::iterator             Ialbum;
    QString                        album;


    for(it = songs.first(); it; it = songs.next())
    {
        if(!it->getCDFlag())
        {
            if(it->getValue() == 0)
            {
                cerr << "playlist.o: Oh crap ... how did we get something with an ID of 0 on a playlist?" << endl ;
            }
            if(it->getValue() > 0)
            {
                // Normal track
                Metadata *tmpdata = all_available_music->getMetadata(it->getValue());
                if (tmpdata)
                {
                    if (songs.at() == 0) { // first song
                        playcountMin = playcountMax = tmpdata->PlayCount();
                        lastplayMin = lastplayMax = tmpdata->LastPlay();
                    } else {
                        if (tmpdata->PlayCount() < playcountMin) { playcountMin = tmpdata->PlayCount(); }
                        else if (tmpdata->PlayCount() > playcountMax) { playcountMax = tmpdata->PlayCount(); }
                 
                        if (tmpdata->LastPlay() < lastplayMin) { lastplayMin = tmpdata->LastPlay(); }
                        else if (tmpdata->LastPlay() > lastplayMax) { lastplayMax = tmpdata->LastPlay(); }
                    }
                }
            }
        }
    }

    int RatingWeight = 2;
    int PlayCountWeight = 2;
    int LastPlayWeight = 2;
    int RandomWeight = 2;

    parent->FillIntelliWeights(RatingWeight, PlayCountWeight, LastPlayWeight,
                               RandomWeight);

    for(it = songs.first(); it; it = songs.next())
    {
        if(!it->getCDFlag())
        {
            if(it->getValue() == 0)
            {
                cerr << "playlist.o: Oh crap ... how did we get something with an ID of 0 on a playlist?" << endl ;
            }
            if(it->getValue() > 0)
            {
                // Normal track
                Metadata *tmpdata = all_available_music->getMetadata(it->getValue());
                if (tmpdata)
                {
                    QString a_string = QString("%1 ~ %2").arg(tmpdata->FormatArtist()).arg(tmpdata->FormatTitle());
                    GenericTree *added_node = tree_to_write_to->addNode(a_string, it->getValue(), true);
                    ++a_counter;
                    added_node->setAttribute(0, 1);
                    added_node->setAttribute(1, a_counter); //  regular order
                    added_node->setAttribute(2, rand()); //  random order
                    
                    //
                    //  Compute "intelligent" weighting
                    //

                    int rating = tmpdata->Rating();
                    int playcount = tmpdata->PlayCount();
                    double lastplaydbl = tmpdata->LastPlay();
                    double ratingValue = (double)(rating) / 10;
                    double playcountValue, lastplayValue;
                    if (playcountMax == playcountMin) { playcountValue = 0; }
                    else { playcountValue = ((playcountMin - (double)playcount) / (playcountMax - playcountMin) + 1); }
                    if (lastplayMax == lastplayMin) { lastplayValue = 0; }
                    else { lastplayValue = ((lastplayMin - lastplaydbl) / (lastplayMax - lastplayMin) + 1); }
                    double rating_value =  (RatingWeight * ratingValue + PlayCountWeight * playcountValue + 
                                            LastPlayWeight * lastplayValue + RandomWeight * (double)rand() / 
                                            (RAND_MAX + 1.0));
                    uint32_t integer_rating = (int) (4000001 - rating_value * 10000);
                    added_node->setAttribute(3, integer_rating); //  "intelligent" order

                    // "intellegent/album" order
                    album = tmpdata->Artist() + "~" + tmpdata->Album();
                    if ((Ialbum = album_map.find(album)) == album_map.end()) {
                      // Add room for 100 albums with 100 tracks.
                      integer_rating *= 10000;
                      integer_rating += (a_counter * 100);

                      album_map.insert(AlbumMap::value_type(album,
                                                            integer_rating));

                      integer_rating += tmpdata->Track();
                      added_node->setAttribute(4, integer_rating);
                    }
                    else {
                      integer_rating = Ialbum->second;
                      integer_rating += tmpdata->Track();
                      added_node->setAttribute(4, integer_rating);
                    }
                }
            }
            if(it->getValue() < 0)
            {
                // it's a playlist, recurse (mildly)
                Playlist *level_down = parent->getPlaylist((it->getValue()) * -1);
                if (level_down)
                {
                    a_counter = level_down->writeTree(tree_to_write_to, a_counter);
                }
            }
        }
        else
        {
            //
            //  f'ing CD tracks to keep Isaac happy
            //

            Metadata *tmpdata = all_available_music->getMetadata(it->getValue());
            if (tmpdata)
            {
                QString a_string = QString("CD: %1 ~ %2 - %3")
                  .arg(tmpdata->Track()).arg(tmpdata->FormatTitle()).arg(tmpdata->FormatArtist());

                if(tmpdata->FormatArtist().length() < 1 ||
                   tmpdata->FormatTitle().length() < 1)
                {
                    a_string = QString("CD Track %1").arg(tmpdata->Track());
                }
                GenericTree *added_node = tree_to_write_to->addNode(a_string, it->getValue(), true);
                ++a_counter;
                added_node->setAttribute(0, 1);
                added_node->setAttribute(1, a_counter); //  regular order
                added_node->setAttribute(2, rand()); //  random order
                added_node->setAttribute(3, rand()); //  "intelligent" order
            }
        }
    }  
    return a_counter;
}

void PlaylistsContainer::writeTree(GenericTree *tree_to_write_to)
{
    all_available_music->writeTree(tree_to_write_to);

    GenericTree *sub_node = tree_to_write_to->addNode(QObject::tr("All My Playlists"), 1);
    sub_node->setAttribute(0, 1);
    sub_node->setAttribute(1, 1);
    sub_node->setAttribute(2, 1);
    sub_node->setAttribute(3, 1);
    
    GenericTree *subsub_node = sub_node->addNode(QObject::tr("Active Play Queue"), 0);
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
    if(cd_playlist.count() > 0)
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
    
    QPtrListIterator<Playlist> iterator( *all_other_playlists );  
    Playlist *a_list;
    while( ( a_list = iterator.current() ) != 0)
    {
        ++a_counter;
        GenericTree *new_node = sub_node->addNode(a_list->getName(), a_list->getID());
        new_node->setAttribute(0, 0);
        new_node->setAttribute(1, a_counter);
        new_node->setAttribute(2, rand());
        new_node->setAttribute(3, rand());
        a_list->writeTree(new_node, 0);
        ++iterator;
    }
    
}

void PlaylistsContainer::save()
{
    Playlist *a_list;

    for(a_list = all_other_playlists->first(); a_list; a_list = all_other_playlists->next())
    {
        if(a_list->hasChanged())
        {
            a_list->fillSonglistFromSongs();
            a_list->savePlaylist(a_list->getName(), my_host);
        }
    }

    active_playlist->savePlaylist("default_playlist_storage", my_host);
    backup_playlist->savePlaylist("backup_playlist_storage", my_host);
}

void PlaylistsContainer::createNewPlaylist(QString name)
{
    Playlist *new_list = new Playlist(all_available_music);
    new_list->setParent(this);

    //  Need to touch the database to get persistent ID
    new_list->savePlaylist(name, my_host);
    new_list->Changed();
    all_other_playlists->append(new_list);
    //if(my_widget)
    //{
    //    new_list->putYourselfOnTheListView(my_widget);
    //}
}

void PlaylistsContainer::copyNewPlaylist(QString name)
{
    Playlist *new_list = new Playlist(all_available_music);
    new_list->setParent(this);

    //  Need to touch the database to get persistent ID
    new_list->savePlaylist(name, my_host);
    new_list->Changed();
    all_other_playlists->append(new_list);
    active_playlist->copyTracks(new_list, false);
    pending_writeback_index = 0;
    active_widget->setText(QObject::tr("Active Play Queue"));
    active_playlist->removeAllTracks();
    active_playlist->addTrack(new_list->getID() * -1, true, false);
}

void PlaylistsContainer::setActiveWidget(PlaylistTitle *widget)
{
    active_widget = widget;
    if (active_widget && pending_writeback_index > 0)
    {
        bool bad = false;
        QString newlabel = QString(QObject::tr("Active Play Queue (%1)")).arg(getPlaylistName(pending_writeback_index, bad));
        active_widget->setText(newlabel);
    }    
}

void PlaylistsContainer::popBackPlaylist()
{
    Playlist *destination = getPlaylist(pending_writeback_index);
    if (!destination)
    {
        cerr << "Unknown playlist: " << pending_writeback_index << endl;
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

void PlaylistsContainer::copyToActive(int index)
{
    backup_playlist->removeAllTracks();
    active_playlist->copyTracks(backup_playlist, false);

    pending_writeback_index = index;
    if(active_widget)
    {
        bool bad = false;
        QString newlabel = QString(QObject::tr("Active Play Queue (%1)")).arg(getPlaylistName(index, bad));
        active_widget->setText(newlabel);
    }    
    active_playlist->removeAllTracks();
    Playlist *copy_from = getPlaylist(index);
    if (!copy_from)
    {
        cerr << "Unknown playlist: " << index << endl;
        return;
    }
    copy_from->copyTracks(active_playlist, true);

    active_playlist->Changed();
    backup_playlist->Changed();

}


void PlaylistsContainer::renamePlaylist(int index, QString new_name)
{
    Playlist *list_to_rename = getPlaylist(index);
    if (list_to_rename)
    {
        list_to_rename->setName(new_name);
        list_to_rename->Changed();
        if(list_to_rename->getID() == pending_writeback_index)
        {
            QString newlabel = QString(QObject::tr("Active Play Queue (%1)")).arg(new_name);
            active_widget->setText(newlabel);
        }
    }
}

void PlaylistsContainer::deletePlaylist(int kill_me)
{
    Playlist *list_to_kill = getPlaylist(kill_me);
    if (!list_to_kill)
    {
        cerr << "Unknown playlist: " << kill_me << endl;
        return;
    }
    //  First, we need to take out any **track** on any other
    //  playlist that is actually a reference to this
    //  playlist

    if (kill_me == pending_writeback_index)
        popBackPlaylist();

    active_playlist->removeTrack(kill_me * -1, false);

    QPtrListIterator<Playlist> iterator( *all_other_playlists );    
    Playlist *a_list;
    while( ( a_list = iterator.current() ) != 0)
    {
        ++iterator;
        if(a_list != list_to_kill)
        {
            a_list->removeTrack(kill_me * -1, false);
        }
    }

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("DELETE FROM music_playlists WHERE playlist_id = :ID ;");
    query.bindValue(":ID", kill_me);

    if (!query.exec() || query.numRowsAffected() < 1)
    {
        MythContext::DBError("playlist delete", query);
    }
    list_to_kill->removeAllTracks();
    all_other_playlists->remove(list_to_kill);
}


QString PlaylistsContainer::getPlaylistName(int index, bool &reference)
{
    if(active_playlist)
    {
        if(active_playlist->getID() == index)
        {
            return active_playlist->getName();
        }

        Playlist *a_list;
        for(a_list = all_other_playlists->last(); a_list; a_list = all_other_playlists->prev())
        {
            if (a_list->getID() == index)
            {
                return a_list->getName();   
            }
        }
    }
    cerr << "playlist.o: Asked to getPlaylistName() with an index number I couldn't find" << endl ;
    reference = true;
    return QObject::tr("Something is Wrong");
}

bool Playlist::containsReference(int to_check, int depth)
{
    if(depth > 10)
    {
        cerr << "playlist.o: I'm recursively checking playlists, and have reached a search depth over 10 " << endl ;
    }
    bool ref_exists = false;

    int check;
    //  Attempt to avoid infinite recursion
    //  in playlists (within playlists) =P

    Track *it;
    for(it = songs.first(); it; it = songs.next())
    {
        check = it->getValue();
        if (check < 0 && !it->getCDFlag())
        {
            if(check * -1 == to_check)
            {
                ref_exists = true;
                return ref_exists;
            }
            else
            {
                //  Recurse down one level
                int new_depth = depth + 1;
                Playlist *new_check = parent->getPlaylist(check * -1);
                if (new_check)
                    ref_exists = new_check->containsReference(to_check, 
                                                              new_depth);
            }
        }
    }  
    return ref_exists;
}

Playlist *PlaylistsContainer::getPlaylist(int id)
{
    //  return a pointer to a playlist
    //  by id;
    
    if(active_playlist->getID() == id)
    {
        return active_playlist;
    }

    //  Very Suprisingly, we need to use an iterator on this,
    //  because if we just traverse the list, other functions (that
    //  called this) that are also traversing the same list get
    //  reset. That took a **long** time to figure out

    QPtrListIterator<Playlist> iterator( *all_other_playlists );    
    Playlist *a_list;
    while( ( a_list = iterator.current() ) != 0)
    {
        ++iterator;
        if(a_list->getID() == id)
        {
            return a_list;
        }
    }
    cerr << "playlists.o: Something asked me to find a Playlist object with an id I couldn't find" << endl ;
    return NULL;    
}

void PlaylistsContainer::showRelevantPlaylists(TreeCheckItem *alllists)
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
    Playlist *some_list;
    for (some_list = all_other_playlists->first(); some_list; 
         some_list = all_other_playlists->next())
    {
        id = some_list->getID() * -1 ;
        temptitle = some_list->getName();
        templevel = "playlist";

        TreeCheckItem *some_item = new TreeCheckItem(alllists, temptitle, 
                                                     templevel, id);

        some_item->setCheckable(true);
        some_item->setActive(true);

        if (some_list->containsReference(pending_writeback_index, 0) ||
            (id * -1) == pending_writeback_index)
        {
            some_item->setCheckable(false);
            some_item->setActive(false);
        }

        some_list->putYourselfOnTheListView(some_item);
    }

    if (alllists->childCount() == 0)
        alllists->setCheckable(false);
    else
        alllists->setCheckable(true);
}

void PlaylistsContainer::refreshRelevantPlaylists(TreeCheckItem *alllists)
{
    if (alllists->childCount() == 0)
    {
        alllists->setCheckable(false);
        return;
    }

    UIListGenericTree *walker = (UIListGenericTree *)(alllists->getChildAt(0));
    while (walker)
    {
        if(TreeCheckItem *check_item = dynamic_cast<TreeCheckItem*>(walker))
        {
            int id = check_item->getID() * -1;
            Playlist *check_playlist = getPlaylist(id);
            if((check_playlist && 
                check_playlist->containsReference(pending_writeback_index, 0)) ||
               id == pending_writeback_index)
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

void PlaylistsContainer::postLoad()
{
    //  Now that everything is loaded, we need to recheck all
    //  tracks and update those that refer to a playlist

    active_playlist->postLoad();
    backup_playlist->postLoad();
    QPtrListIterator<Playlist> iterator( *all_other_playlists );    
    Playlist *a_list;
    while( ( a_list = iterator.current() ) != 0)
    {
        ++iterator;
        a_list->postLoad();
    }
}

bool PlaylistsContainer::pendingWriteback()
{
    if(pending_writeback_index > 0)
    {
        return true;
    }
    return false;
}

bool PlaylistsContainer::nameIsUnique(QString a_name, int which_id)
{
    if(a_name == "default_playlist_storage")
    {
        return false;
    }
    
    if(a_name == "backup_playlist_storage")
    {
        return false;
    }

    QPtrListIterator<Playlist> iterator( *all_other_playlists );    
    Playlist *a_list;
    while( ( a_list = iterator.current() ) != 0)
    {
        ++iterator;
        if(a_list->getName() == a_name &&
           a_list->getID() != which_id)
        {
            return false;
        }
    }
    return true ;
}

bool PlaylistsContainer::cleanOutThreads()
{
    if(playlists_loader->finished())
    {
        return true;
    }
    playlists_loader->wait();
    return false;
}

void PlaylistsContainer::clearActive()
{
    backup_playlist->removeAllTracks();
    active_playlist->removeAllTracks();
    backup_playlist->Changed();
    active_playlist->Changed();
    pending_writeback_index = 0;
    active_widget->setText(QObject::tr("Active Play Queue"));
}

// Here begins CD Writing things. ComputeSize, CreateCDMP3 & CreateCDAudio

void Playlist::computeSize(double &size_in_MB, double &size_in_sec)
{
    Track *it;
    double child_MB;
    double child_sec;

    // Clear return values
    size_in_MB = 0.0;
    size_in_sec = 0.0;

    for (it = songs.first(); it; it = songs.next())
    {
        if (it->getCDFlag())
            continue;

        if (it->getValue() == 0)
        {
            cerr << "playlist.o: Oh crap ... how did we get something with an ID of 0 on a playlist?" << endl ;
        }
        else if (it->getValue() > 0)
        {
            // Normal track
            Metadata *tmpdata = all_available_music->getMetadata(it->getValue());
            if (tmpdata)
            {
                if (tmpdata->Length() > 0)
                    size_in_sec += tmpdata->Length();
                else
                    cerr << "playlist.o: Computing track lengths. One track <=0" <<endl ;

                // Check tmpdata->Filename
                QFileInfo finfo(tmpdata->Filename());

                size_in_MB += finfo.size() / 1000000;
            }
        }
        if (it->getValue() < 0)
        {
            // it's a playlist, recurse (mildly)

            // Comment: I can't make this thing work. Nothing is computed with playlists
            Playlist *level_down = parent->getPlaylist((it->getValue()) * -1);
            if (level_down)
            {
                level_down->computeSize(child_MB, child_sec);
                size_in_MB += child_MB;
                size_in_sec += child_sec;
            }
        }
    }
}

int Playlist::CreateCDMP3(void)
{
    // Check & get global settings
    if (!gContext->GetNumSetting("CDWriterEnabled")) 
    {
        cerr << "CD Writer is not enabled.\n";
        return 1;
    }

    QString scsidev = gContext->GetSetting("CDWriterDevice");
    if (scsidev.isEmpty() || scsidev.isNull()) 
    {
        cerr << "No CD Writer device defined.\n";
        return 1;
    }

    int disksize = gContext->GetNumSetting("CDDiskSize", 2);
    QString writespeed = gContext->GetSetting("CDWriteSpeed", "2");
    bool MP3_dir_flag = gContext->GetNumSetting("CDCreateDir", 1);

    double size_in_MB = 0.0;

    QStringList reclist;

    Track *it;
    for (it = songs.first(); it; it = songs.next())
    {
        if (it->getCDFlag())
            continue;

        if (it->getValue() == 0)
        {
            cerr << "playlist.o: Oh crap ... how did we get something with an ID of 0 on a playlist?" << endl ;
        }
        else if (it->getValue() > 0)
        {
            // Normal track
            Metadata *tmpdata = all_available_music->getMetadata(it->getValue());
            if (tmpdata)
            {
                // check filename..
                QFileInfo testit(tmpdata->Filename());
                if (!testit.exists())
                    continue;
                size_in_MB += testit.size() / 1000000.0;
                QString outline;
                if (MP3_dir_flag)
                {
                    if (tmpdata->Artist().length() > 0)
                        outline += tmpdata->Artist() + "/";
                    if (tmpdata->Album().length() > 0)
                        outline += tmpdata->Album() + "/";
                }

                outline += "=";
                outline += tmpdata->Filename();

                reclist += outline;
            }
        }
        else if (it->getValue() < 0)
        {
            // FIXME: handle playlists
        }
    }

    int max_size;
    if (disksize == 0)
        max_size = 650;
    else
        max_size = 700;

    if (size_in_MB >= max_size)
    {
        cerr << "MP3 CD creation aborted -- cd size too big.\n";
        return 1;
    }

    // probably should tie stdout of mkisofs to stdin of cdrecord sometime
    char tmprecordlist[L_tmpnam];
    char tmprecordisofs[L_tmpnam];

    tmpnam(tmprecordlist);
    tmpnam(tmprecordisofs);

    QFile reclistfile(tmprecordlist);

    if (!reclistfile.open(IO_WriteOnly))
    {
        cerr << "Unable to open temporary file\n";
        return 1;
    }

    QTextStream recstream(&reclistfile);

    QStringList::Iterator iter;

    for (iter = reclist.begin(); iter != reclist.end(); ++iter)
    {
        recstream << *iter << "\n";
    }

    reclistfile.close();

    MythProgressDialog *progress;
    progress = new MythProgressDialog(QObject::tr("Creating CD File System"),
                                      100);
    progress->setProgress(1);

    QStringList args;
    args = "mkisofs";
    args += "-graft-points";
    args += "-path-list";
    args += tmprecordlist;
    args += "-o";
    args += tmprecordisofs;
    args += "-J";
    args += "-R";

    cout << "Running: " << args.join(" ") << endl;

    bool retval = 0;

    QProcess isofs(args);
    
    if (isofs.start())
    {
        while (1)
        {
            while (isofs.canReadLineStderr())
            {
                QString buf = isofs.readLineStderr();
                if (buf[6] == '%')
                {
                    buf = buf.mid(0, 3);
                    progress->setProgress(buf.stripWhiteSpace().toInt());
                }
            }
            if (isofs.isRunning())
            {
                qApp->processEvents();
                usleep(100000);
            }
            else
            {
                if (!isofs.normalExit())
                {
                    cerr << "Unable to run 'mkisofs'\n";
                    retval = 1;
                }
                break;
            }
        }
    }
    else
    {
        cerr << "Unable to run 'mkisofs'\n";
        retval = 1;
    }

    progress->Close();
    delete progress;

    progress = new MythProgressDialog(QObject::tr("Burning CD"), 100);
    progress->setProgress(2);

    args = "cdrecord";
    args += "-v";
    //args += "-dummy";
    args += "dev=";
    args += scsidev;

    if (writespeed.toInt() > 0)
    {
        args += "-speed=";
        args += writespeed;
    }

    args += "-data";
    args += tmprecordisofs;

    cout << "Running: " << args.join(" ") << endl;

    QProcess burn(args);

    if (burn.start())
    {
        while (1)
        {
            while (burn.canReadLineStderr())
            {
                QString err = burn.readLineStderr();
                if (err == "cdrecord: Drive needs to reload the media" ||
                    err == "cdrecord: Input/output error." ||
                    err == "cdrecord: No disk / Wrong disk!")
                {
                    cerr << err << endl;
                    burn.kill();
                    retval = 1;
                }
            }
            while (burn.canReadLineStdout())
            {
                QString line = burn.readLineStdout();
                if (line.mid(15, 2) == "of")
                {
                    int mbdone = line.mid(10, 5).stripWhiteSpace().toInt();
                    int mbtotal = line.mid(17, 5).stripWhiteSpace().toInt();

                    if (mbtotal > 0)
                    {
                        progress->setProgress((mbdone * 100) / mbtotal);
                    }
                }
            }

            if (burn.isRunning())
            {
                qApp->processEvents();
                usleep(10000);
            }
            else
            {
                if (!burn.normalExit())
                {
                    cerr << "Unable to run 'cdrecord'\n";
                    retval = 1;
                }
                break;
            }
        }
    }
    else
    {
        cerr << "Unable to run 'cdrecord'\n";
        retval = 1;
    }

    progress->Close();
    delete progress;

    QFile::remove(tmprecordlist);
    QFile::remove(tmprecordisofs);

    return retval;
}

int Playlist::CreateCDAudio(void)
{
    return -1;
}

