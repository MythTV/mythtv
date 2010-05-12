#include <unistd.h>
#include <inttypes.h>
#include <cstdlib>

#include <map>
#include <algorithm>
using namespace std;

#include <QApplication>
#include <QTextStream>
#include <QFileInfo>
#include <Q3Process>

#include "playlist.h"
#include "playlistcontainer.h"
#include "smartplaylist.h"

#include <mythcontext.h>
#include <mythdb.h>
#include <compat.h>
#include <mythmediamonitor.h>

const char *kID0err = "Song with ID of 0 in playlist, this shouldn't happen.";

#define LOC      QString("Track: ")
#define LOC_WARN QString("Track, Warning: ")
#define LOC_ERR  QString("Track, Error: ")

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

void Track::postLoad(PlaylistContainer *grandparent)
{
    if (cd_flag)
    {
        label = all_available_music->getLabel(index_value, &bad_reference);
        return;
    }

    if (index_value > 0) // Normal Track
        label = all_available_music->getLabel(index_value, &bad_reference);
    else if (index_value < 0)
        label = grandparent->getPlaylistName( (-1 * index_value),
                                             bad_reference);
    else
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "Track Number of 0 is invalid!");
    }
}

void Track::setParent(Playlist *parent_ptr)
{
    //  I'm a track, what's my playlist?
    parent = parent_ptr;
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
        //delete my_widget;  Deleted by the parent..
        my_widget = NULL;
    }
}

void Track::moveUpDown(bool flag)
{
    parent->moveTrackUpDown(flag, this);
}

TrackType Track::GetTrackType(void) const
{
    if (my_widget)
    {
        if (dynamic_cast<PlaylistCD*>(my_widget))
            return kTrackCD;

        if (dynamic_cast<PlaylistPlaylist*>(my_widget))
            return kTrackPlaylist;

        if (dynamic_cast<PlaylistTrack*>(my_widget))
            return kTrackSong;

        return kTrackUnknown;
    }

    if (cd_flag)
        return kTrackCD;

    if (index_value < 0)
        return kTrackPlaylist;

    if (index_value > 0)
        return kTrackSong;

    return kTrackUnknown;
}

void Track::putYourselfOnTheListView(UIListGenericTree *a_listviewitem)
{
    if (my_widget)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "putYourselfOnTheListView() called when my_widget already exists.");
        return;
    }

    TrackType type = GetTrackType();
    switch (type)
    {
        case kTrackCD:
            my_widget = new PlaylistCD(a_listviewitem, label);       break;
        case kTrackSong:
            my_widget = new PlaylistTrack(a_listviewitem, label);    break;
        case kTrackPlaylist:
            my_widget = new PlaylistPlaylist(a_listviewitem, label); break;
        default: break;
    }

    if (my_widget)
    {
        my_widget->setOwner(this);
    }
    else
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "putYourselfOnTheListView() failed to create a widget");
    }
}

#undef LOC
#undef LOC_WARN
#undef LOC_ERR

#define LOC      QString("Playlist: ")
#define LOC_WARN QString("Playlist, Warning: ")
#define LOC_ERR  QString("Playlist, Error: ")

void Playlist::postLoad()
{
    SongList::iterator it = songs.begin();
    while (it != songs.end())
    {
        (*it)->postLoad(parent);
        if ((*it)->badReference())
        {
            delete *it;
            it = songs.erase(it);
            Changed();
        }
        else
        {
            ++it;
        }
    }
}

bool Playlist::checkTrack(int a_track_id, bool cd_flag) const
{
    // XXX SPEED THIS UP
    // Should be a straight lookup against cached index

    SongList::const_iterator it = songs.begin();
    for (; it != songs.end(); ++it)
    {
        if ((*it)->getValue() == a_track_id && (*it)->getCDFlag() == cd_flag)
            return true;
    }

    return false;
}

void Playlist::copyTracks(Playlist *to_ptr, bool update_display) const
{
    SongList::const_iterator it = songs.begin();
    for (; it != songs.end(); ++it)
    {
        if (!(*it)->getCDFlag())
            to_ptr->addTrack((*it)->getValue(), update_display, false);
    }

    to_ptr->fillSonglistFromSongs();
}



void Playlist::addTrack(int the_track, bool update_display, bool cd)
{
    // Given a track id number, add that track to this playlist

    Track *a_track = new Track(the_track, all_available_music);
    a_track->setCDFlag(cd);
    a_track->postLoad(parent);
    a_track->setParent(this);
    songs.push_back(a_track);
    changed = true;

    // If I'm part of a GUI, display the existence of this new track

    if (!update_display)
        return;

    PlaylistTitle *which_widget = parent->getActiveWidget();

    if (which_widget)
        a_track->putYourselfOnTheListView(which_widget);
}

void Playlist::removeAllTracks(void)
{
    while (!songs.empty())
    {
        songs.back()->deleteYourWidget();
        delete songs.back();
        songs.pop_back();
    }

    changed = true;
}

void Playlist::removeAllWidgets(void)
{
    SongList::iterator it = songs.begin();
    for (; it != songs.end(); ++it)
        (*it)->deleteYourWidget();
}

void Playlist::ripOutAllCDTracksNow()
{
    SongList::iterator it = songs.begin();
    while (it != songs.end())
    {
        if ((*it)->getCDFlag())
        {
            (*it)->deleteYourWidget();
            delete *it;
            it = songs.erase(it);
            changed = true;
        }
        else
        {
            ++it;
        }
    }
}

void Playlist::removeTrack(int the_track, bool cd_flag)
{
    // Should be a straight lookup against cached index

    SongList::iterator it = songs.begin();
    while (it != songs.end())
    {
        if ((*it)->getValue() == the_track && cd_flag == (*it)->getCDFlag())
        {
            (*it)->deleteYourWidget();
            delete *it;
            it = songs.erase(it);
            changed = true;
        }
        else
        {
            ++it;
        }
    }
}

void Playlist::moveTrackUpDown(bool flag, int where_its_at)
{
    Track *the_track = songs.at(where_its_at);

    if (!the_track)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "A playlist was asked to move a "
                                        "track, but can't find it");
        return;
    }

    moveTrackUpDown(flag, the_track);
}

void Playlist::moveTrackUpDown(bool flag, Track *the_track)
{
    uint insertion_point = 0;
    int where_its_at = songs.indexOf(the_track);
    if (where_its_at < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "A playlist was asked to move a "
                "track, but can'd find it");
        return;
    }

    if (flag)
        insertion_point = ((uint)where_its_at) - 1;
    else
        insertion_point = ((uint)where_its_at) + 1;

    songs.removeAt(where_its_at);
    songs.insert(insertion_point, the_track);

    changed = true; //  This playlist is now different than Database
}

Playlist::Playlist(AllMusic *all_music_ptr)
{
    //  fallback values
    playlistid = 0;
    name = QObject::tr("oops");
    all_available_music = all_music_ptr;
    changed = false;
}

void Playlist::putYourselfOnTheListView(UIListGenericTree *a_listviewitem)
{
    SongList::iterator it = songs.begin();
    for (; it != songs.end(); ++it)
        (*it)->putYourselfOnTheListView(a_listviewitem);
}

int Playlist::getFirstTrackID(void) const
{
    SongList::const_iterator it = songs.begin();
    if (it != songs.end())
        return (*it)->getValue();
    return 0;
}

Playlist::~Playlist()
{
    while (!songs.empty())
    {
        delete songs.front();
        songs.pop_front();
    }
}

/*
Playlist& Playlist::operator=(const Playlist& rhs)
{
    if (this == &rhs)
    {
        return *this;
    }

    playlistid = rhs.playlistid;
    name = rhs.name;
    raw_songlist = rhs.raw_songlist;

    while (!songs.empty())
    {
        delete songs.front();
        songs.pop_front();
    }
#error Can not safely copy Playlist.. Track does not have a deep copy constructor

    return *this;
}
*/

void Playlist::describeYourself(void) const
{
    //  This is for debugging
/*
    cout << "Playlist with name of \"" << name << "\"" << endl;
    cout << "        playlistid is " << playlistid << endl;
    cout << "     songlist(raw) is \"" << raw_songlist << "\"" << endl;
    cout << "     songlist list is ";
*/
    QString msg;
    SongList::const_iterator it = songs.begin();
    for (; it != songs.end(); ++it)
        msg += (*it)->getValue() + ",";

    VERBOSE(VB_IMPORTANT, LOC + msg);
}

void Playlist::getStats(int *trackCount, int *totalLength, int currenttrack, int *playedLength) const
{
    *trackCount = songs.size();
    *totalLength = 0;
    if (playedLength)
        *playedLength = 0;

    if (currenttrack < 0 || currenttrack >= songs.size())
        currenttrack = 0;

    int track = 0;
    SongList::const_iterator it = songs.begin();
    for (; it != songs.end(); ++it, ++track)
    {
        int trackID = (*it)->getValue();
        Metadata *mdata = gMusicData->all_music->getMetadata(trackID);
        if (mdata)
        {
            *totalLength += mdata->Length();
            if (playedLength && track < currenttrack)
                *playedLength += mdata->Length();
        }
    }
}

void Playlist::loadPlaylist(QString a_name, QString a_host)
{
    QString thequery;
    if (a_host.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "loadPlaylist() - We need a valid hostname");
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
        // Technically this is never called as this function
        // is only used to load the default/backup playlists.
        query.prepare("SELECT playlist_id, playlist_name, playlist_songs "
                      "FROM music_playlists "
                      "WHERE playlist_name = :NAME"
                      " AND (hostname = '' OR hostname = :HOST);");
    }
    query.bindValue(":NAME", a_name);
    query.bindValue(":HOST", a_host);

    if (query.exec() && query.size() > 0)
    {
        while (query.next())
        {
            playlistid = query.value(0).toInt();
            name = query.value(1).toString();
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
        raw_songlist.clear();
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

    if (!query.exec())
        MythDB::DBError("Playlist::loadPlaylistByID", query);

    while (query.next())
    {
        playlistid = query.value(0).toInt();
        name = query.value(1).toString();
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

    QStringList list = raw_songlist.split(",", QString::SkipEmptyParts);
    QStringList::iterator it = list.begin();
    for (; it != list.end(); it++)
    {
        an_int = QString(*it).toInt();
        if (an_int != 0)
        {
            if (filter)
            {
                Metadata *md = all_available_music->getMetadata(an_int);
                if (md)
                    md->setVisible(true);
            }
            else
            {
                Track *a_track = new Track(an_int, all_available_music);
                a_track->setParent(this);
                songs.push_back(a_track);
            }
        }
        else
        {
            changed = true;

            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Taking a 0 (zero) off a playlist. \n\t\t\t"
                    "If this happens on repeated invocations of "
                    "mythmusic, then something is really wrong");
        }
    }

    if (filter)
    {
        all_available_music->clearTree();
        all_available_music->buildTree();
        all_available_music->sortTree();
    }
}

void Playlist::fillSonglistFromSongs(void)
{
    QString a_list;
    SongList::const_iterator it = songs.begin();
    for (; it != songs.end(); ++it)
    {
        if (!(*it)->getCDFlag())
            a_list += QString(",%1").arg((*it)->getValue());
    }

    raw_songlist.clear();
    if (!a_list.isEmpty())
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
               "LEFT JOIN music_directories ON"
               " music_songs.directory_id=music_directories.directory_id "
               "LEFT JOIN music_artists ON"
               " music_songs.artist_id=music_artists.artist_id "
               "LEFT JOIN music_albums ON"
               " music_songs.album_id=music_albums.album_id "
               "LEFT JOIN music_genres ON"
               " music_songs.genre_id=music_genres.genre_id "
               "LEFT JOIN music_artists AS music_comp_artists ON "
               "music_albums.artist_id=music_comp_artists.artist_id ";
    if (whereClause.length() > 0)
      theQuery += whereClause;

    if (!query.exec(theQuery))
    {
        MythDB::DBError("Load songlist from query", query);
        raw_songlist.clear();
        return;
    }

    QString new_songlist;
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
            QStringList list = raw_songlist.split(",", QString::SkipEmptyParts);
            QStringList::iterator it = list.begin();
            raw_songlist.clear();
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
       addTrack(-1 * i, false, true);
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
        VERBOSE(VB_GENERAL, LOC_WARN +
                QString("Cannot find Smartplaylist Category: %1")
                .arg(category));
        return;
    }

    // find smartplaylist
    int ID;
    QString matchType;
    QString orderBy;
    int limitTo;

    query.prepare("SELECT smartplaylistid, matchtype, orderby, limitto "
                  "FROM music_smartplaylists "
                  "WHERE categoryid = :CATEGORYID AND name = :NAME;");
    query.bindValue(":NAME", name);
    query.bindValue(":CATEGORYID", categoryID);

    if (query.exec())
    {
        if (query.isActive() && query.size() > 0)
        {
            query.first();
            ID = query.value(0).toInt();
            matchType = (query.value(1).toString() == "All") ? " AND " : " OR ";
            orderBy = query.value(2).toString();
            limitTo = query.value(3).toInt();
        }
        else
        {
            VERBOSE(VB_GENERAL, LOC_WARN +
                    QString("Cannot find smartplaylist: %1").arg(name));
            return;
        }
    }
    else
    {
        MythDB::DBError("Find SmartPlaylist", query);
        return;
    }

    // get smartplaylist items
    QString whereClause = "WHERE ";

    query.prepare("SELECT field, operator, value1, value2 "
                  "FROM music_smartplaylist_items "
                  "WHERE smartplaylistid = :ID;");
    query.bindValue(":ID", ID);
    if (query.exec())
    {
        bool bFirst = true;
        while (query.next())
        {
            QString fieldName = query.value(0).toString();
            QString operatorName = query.value(1).toString();
            QString value1 = query.value(2).toString();
            QString value2 = query.value(3).toString();
            if (!bFirst)
                whereClause += matchType + getCriteriaSQL(fieldName,
                                           operatorName, value1, value2);
            else
            {
               bFirst = false;
               whereClause += " " + getCriteriaSQL(fieldName, operatorName,
                                                   value1, value2);
            }
        }
    }

    // add order by clause
    whereClause += getOrderBySQL(orderBy);

    // add limit
    if (limitTo > 0)
        whereClause +=  " LIMIT " + QString::number(limitTo);

    fillSonglistFromQuery(whereClause, removeDuplicates,
                          insertOption, currentTrackID);
}

void Playlist::savePlaylist(QString a_name, QString a_host)
{
    name = a_name.simplified();
    if (name.length() < 1)
    {
        VERBOSE(VB_GENERAL, LOC_WARN +
                "Not saving unnamed playlist");
        return;
    }

    if (a_host.length() < 1)
    {
        VERBOSE(VB_GENERAL, LOC_WARN +
                "Not saving playlist without a host name");
        return;
    }
    if (name.length() < 1)
        return;

    fillSonglistFromSongs();
    MSqlQuery query(MSqlQuery::InitCon());

    int length = 0, songcount = 0, playtime = 0, an_int;
    QStringList list = raw_songlist.split(",", QString::SkipEmptyParts);
    QStringList::iterator it = list.begin();
    for (; it != list.end(); it++)
    {
        an_int = QString(*it).toInt();
        if (an_int != 0)
        {
            songcount++;
            if (an_int > 0)
            {
                Metadata *md = all_available_music->getMetadata(an_int);
                if (md)
                    length = md->Length();
            }
            else
            {
                query.prepare("SELECT length FROM music_playlists "
                    "WHERE playlist_id = :ID ;");
                an_int *= -1;
                query.bindValue(":ID", an_int);

                if (query.exec() && query.next())
                {
                    length = query.value(0).toInt();
                }
            }

            playtime += length;
        }
    }

    bool save_host = ("default_playlist_storage" == a_name
        || "backup_playlist_storage" == a_name);
    if (playlistid > 0)
    {
        QString str_query = "UPDATE music_playlists SET "
                            "playlist_songs = :LIST, "
                            "playlist_name = :NAME, "
                            "songcount = :SONGCOUNT, "
                            "length = :PLAYTIME";
        if (save_host)
            str_query += ", hostname = :HOSTNAME";
        str_query += " WHERE playlist_id = :ID ;";

        query.prepare(str_query);
        query.bindValue(":ID", playlistid);
    }
    else
    {
        QString str_query = "INSERT INTO music_playlists"
                            " (playlist_name, playlist_songs,"
                            "  songcount, length";
        if (save_host)
            str_query += ", hostname";
        str_query += ") VALUES(:NAME, :LIST, :SONGCOUNT, :PLAYTIME";
        if (save_host)
            str_query += ", :HOSTNAME";
        str_query += ");";

        query.prepare(str_query);
    }
    query.bindValue(":LIST", raw_songlist);
    query.bindValue(":NAME", a_name);
    query.bindValue(":SONGCOUNT", songcount);
    query.bindValue(":PLAYTIME", playtime);
    if (save_host)
        query.bindValue(":HOSTNAME", a_host);

    if (!query.exec() || (playlistid < 1 && query.numRowsAffected() < 1))
    {
        MythDB::DBError("Problem saving playlist", query);
    }

    if (playlistid < 1)
        playlistid = query.lastInsertId().toInt();
}

QString Playlist::removeDuplicateTracks(const QString &new_songlist)
{
    raw_songlist.remove(' ');

    QStringList curList = raw_songlist.split(",", QString::SkipEmptyParts);
    QStringList newList = new_songlist.split(",", QString::SkipEmptyParts);
    QStringList::iterator it = newList.begin();
    QString songlist;

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
    // compute max/min playcount,lastplay for this playlist
    int playcountMin = 0;
    int playcountMax = 0;
    double lastplayMin = 0.0;
    double lastplayMax = 0.0;

    typedef map<QString, uint32_t> AlbumMap;
    AlbumMap                       album_map;
    AlbumMap::iterator             Ialbum;
    QString                        album;

    typedef map<QString, uint32_t> ArtistMap;
    ArtistMap                      artist_map;
    ArtistMap::iterator            Iartist;
    QString                        artist;

    uint idx = 0;
    SongList::const_iterator it = songs.begin();
    for (; it != songs.end(); ++it, ++idx)
    {
        if (!(*it)->getCDFlag())
        {
            if ((*it)->getValue() == 0)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR + kID0err);
            }
            if ((*it)->getValue() > 0)
            {
                // Normal track
                Metadata *tmpdata =
                    all_available_music->getMetadata((*it)->getValue());
                if (tmpdata)
                {
                    if (tmpdata->isVisible())
                    {
                        if (0 == idx)
                        { // first song
                            playcountMin = playcountMax = tmpdata->PlayCount();
                            lastplayMin = lastplayMax = tmpdata->LastPlay().toTime_t();
                        }
                        else
                        {
                            if (tmpdata->PlayCount() < playcountMin)
                                playcountMin = tmpdata->PlayCount();
                            else if (tmpdata->PlayCount() > playcountMax)
                                playcountMax = tmpdata->PlayCount();

                            if (tmpdata->LastPlay().toTime_t() < lastplayMin)
                                lastplayMin = tmpdata->LastPlay().toTime_t();
                            else if (tmpdata->LastPlay().toTime_t() > lastplayMax)
                                lastplayMax = tmpdata->LastPlay().toTime_t();
                        }
                    }
                    // pre-fill the album-map with the album name.
                    // This allows us to do album mode in album order
                    album = tmpdata->Album();
                    // pre-fill the artist map with the artist name and song title
                    artist = tmpdata->Artist() + "~" + tmpdata->Title();
                }
                if ((Ialbum = album_map.find(album)) == album_map.end())
                    album_map.insert(AlbumMap::value_type(album,0));

                if ((Iartist = artist_map.find(artist)) == artist_map.end())
                    artist_map.insert(ArtistMap::value_type(artist,0));
            }
        }
    }
    // populate the sort id into the album map
    uint32_t album_count = 1;
    for (Ialbum = album_map.begin(); Ialbum != album_map.end(); Ialbum++)
    {
        Ialbum->second = album_count;
        album_count++;
    }

    // populate the sort id into the artist map
    uint32_t count = 1;
    for (Iartist = artist_map.begin(); Iartist != artist_map.end(); Iartist++)
    {
        Iartist->second = count;
        count++;
    }

    int RatingWeight = 2;
    int PlayCountWeight = 2;
    int LastPlayWeight = 2;
    int RandomWeight = 2;

    parent->FillIntelliWeights(RatingWeight, PlayCountWeight, LastPlayWeight,
                               RandomWeight);

    for (it = songs.begin(); it != songs.end(); ++it)
    {
        if (!(*it)->getCDFlag())
        {
            if ((*it)->getValue() == 0)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR + kID0err);
            }
            if ((*it)->getValue() > 0)
            {
                // Normal track
                Metadata *tmpdata =
                    all_available_music->getMetadata((*it)->getValue());
                if (tmpdata && tmpdata->isVisible())
                {
                    QString a_string = QString("%1 ~ %2")
                                       .arg(tmpdata->FormatArtist())
                                       .arg(tmpdata->FormatTitle());
                    GenericTree *added_node = tree_to_write_to->addNode(
                        a_string, (*it)->getValue(), true);
                    ++a_counter;
                    added_node->setAttribute(0, 1);
                    added_node->setAttribute(1, a_counter); //  regular order
                    added_node->setAttribute(2, rand()); //  random order

                    //
                    //  Compute "intelligent" weighting
                    //

                    int rating = tmpdata->Rating();
                    int playcount = tmpdata->PlayCount();
                    double lastplaydbl = tmpdata->LastPlay().toTime_t();
                    double ratingValue = (double)(rating) / 10;
                    double playcountValue, lastplayValue;

                    if (playcountMax == playcountMin)
                        playcountValue = 0;
                    else
                        playcountValue = ((playcountMin - (double)playcount)
                                         / (playcountMax - playcountMin) + 1);

                    if (lastplayMax == lastplayMin)
                        lastplayValue = 0;
                    else
                        lastplayValue = ((lastplayMin - lastplaydbl)
                                        / (lastplayMax - lastplayMin) + 1);

                    double rating_value =  (RatingWeight * ratingValue +
                                            PlayCountWeight * playcountValue +
                                            LastPlayWeight * lastplayValue +
                                            RandomWeight * (double)rand() /
                                            (RAND_MAX + 1.0));
                    uint32_t integer_rating = (int) (4000001 -
                                                     rating_value * 10000);
                    //  "intelligent" order
                    added_node->setAttribute(3, integer_rating);

                    // "intellegent/album" order
                    uint32_t album_order;
                    album = tmpdata->Album();
                    if ((Ialbum = album_map.find(album)) == album_map.end())
                    {
                        // we didn't find this album in the map,
                        // yet we pre-loaded them all. we are broken,
                        // but we just set the track order to 1, since there
                        // is no real point in reporting an error
                        album_order = 1;
                    }
                    else
                    {
                        album_order = Ialbum->second * 100;
                    }
                    album_order += tmpdata->Track();
                    added_node->setAttribute(4, album_order);

                    // "artist" order, sorts by artist name then track title
                    // uses the pre-prepared artist map to do this
                    uint32_t integer_order;
                    artist = tmpdata->Artist() + "~" + tmpdata->Title();
                    if ((Iartist = artist_map.find(artist)) == artist_map.end())
                    {
                        // we didn't find this track in the map,
                        // yet we pre-loaded them all. we are broken,
                        // but we just set the track order to 1, since there
                        // is no real point in reporting an error
                        integer_order = 1;
                    }
                    else
                    {
                        integer_order = Iartist->second;
                    }
                    added_node->setAttribute(5, integer_order);
                }
            }
            if ((*it)->getValue() < 0)
            {
                // it's a playlist, recurse (mildly)
                Playlist *level_down
                    = parent->getPlaylist(((*it)->getValue()) * -1);
                if (level_down)
                {
                    a_counter = level_down->writeTree(tree_to_write_to,
                                                      a_counter);
                }
            }
        }
        else
        {
            Metadata *tmpdata =
                all_available_music->getMetadata((*it)->getValue());
            if (tmpdata)
            {
                QString a_string = QString("(CD) %1 ~ %2")
                  .arg(tmpdata->FormatArtist()).arg(tmpdata->FormatTitle());

                if (tmpdata->FormatArtist().length() < 1 ||
                   tmpdata->FormatTitle().length() < 1)
                {
                    a_string = QString("(CD) Track %1").arg(tmpdata->Track());
                }
                GenericTree *added_node =
                    tree_to_write_to->addNode(a_string, (*it)->getValue(), true);
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

bool Playlist::containsReference(int to_check, int depth)
{
    if (depth > 10)
    {
        VERBOSE(VB_IMPORTANT,
                LOC_ERR + "Recursively checking playlists, and have "
                "reached a search depth over 10 ");
    }
    bool ref_exists = false;

    int check;
    //  Attempt to avoid infinite recursion
    //  in playlists (within playlists) =P

    SongList::const_iterator it = songs.begin();
    for (; it != songs.end(); ++it)
    {
        check = (*it)->getValue();
        if (check < 0 && !(*it)->getCDFlag())
        {
            if (check * -1 == to_check)
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

Track* Playlist::getSongAt(int pos)
{
    if (pos >= 0 && pos < songs.size())
        return songs.at(pos);

    return NULL;
}

// Here begins CD Writing things. ComputeSize, CreateCDMP3 & CreateCDAudio

void Playlist::computeSize(double &size_in_MB, double &size_in_sec)
{
    double child_MB;
    double child_sec;

    // Clear return values
    size_in_MB = 0.0;
    size_in_sec = 0.0;

    SongList::const_iterator it = songs.begin();
    for (; it != songs.end(); ++it)
    {
        if ((*it)->getCDFlag())
            continue;

        if ((*it)->getValue() == 0)
        {
            VERBOSE(VB_IMPORTANT, kID0err);
        }
        else if ((*it)->getValue() > 0)
        {
            // Normal track
            Metadata *tmpdata =
                all_available_music->getMetadata((*it)->getValue());
            if (tmpdata)
            {
                if (tmpdata->Length() > 0)
                    size_in_sec += tmpdata->Length();
                else
                    VERBOSE(VB_GENERAL, "Computing track lengths. "
                                        "One track <=0");

                // Check tmpdata->Filename
                QFileInfo finfo(tmpdata->Filename());

                size_in_MB += finfo.size() / 1000000;
            }
        }
        if ((*it)->getValue() < 0)
        {
            // it's a playlist, recurse (mildly)

            // Comment: I can't make this thing work.
            // Nothing is computed with playlists
            Playlist *level_down = parent->getPlaylist(((*it)->getValue()) * -1);
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
        VERBOSE(VB_GENERAL, "CD Writer is not enabled.");
        return 1;
    }

    QString scsidev = MediaMonitor::defaultCDWriter();
    if (scsidev.isEmpty())
    {
        VERBOSE(VB_GENERAL, "No CD Writer device defined.");
        return 1;
    }

    int disksize = gContext->GetNumSetting("CDDiskSize", 2);
    QString writespeed = gContext->GetSetting("CDWriteSpeed", "2");
    bool MP3_dir_flag = gContext->GetNumSetting("CDCreateDir", 1);

    double size_in_MB = 0.0;

    QStringList reclist;

    SongList::const_iterator it = songs.begin();
    for (; it != songs.end(); ++it)
    {
        if ((*it)->getCDFlag())
            continue;

        if ((*it)->getValue() == 0)
        {
            VERBOSE(VB_IMPORTANT, kID0err);
        }
        else if ((*it)->getValue() > 0)
        {
            // Normal track
            Metadata *tmpdata =
                all_available_music->getMetadata((*it)->getValue());
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
        else if ((*it)->getValue() < 0)
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
        VERBOSE(VB_GENERAL, "MP3 CD creation aborted -- cd size too big.");
        return 1;
    }

    // probably should tie stdout of mkisofs to stdin of cdrecord sometime
    QString tmptemplate("/tmp/mythmusicXXXXXX");

    QString tmprecordlist = createTempFile(tmptemplate);
    if (tmprecordlist == tmptemplate)
    {
        VERBOSE(VB_IMPORTANT, "Unable to open temporary file");
        return 1;
    }

    QString tmprecordisofs = createTempFile(tmptemplate);
    if (tmprecordisofs == tmptemplate)
    {
        VERBOSE(VB_IMPORTANT, "Unable to open temporary file");
        return 1;
    }

    QFile reclistfile(tmprecordlist);

    if (!reclistfile.open(QIODevice::WriteOnly))
    {
        VERBOSE(VB_IMPORTANT, "Unable to open temporary file");
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

    QStringList args("mkisofs");
    args += "-graft-points";
    args += "-path-list";
    args += tmprecordlist;
    args += "-o";
    args += tmprecordisofs;
    args += "-J";
    args += "-R";

    VERBOSE(VB_GENERAL, "Running: " + args.join(" "));

    bool retval = 0;

    Q3Process isofs(args);

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
                    progress->setProgress(buf.trimmed().toInt());
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
                    VERBOSE(VB_IMPORTANT, "Unable to run 'mkisofs'");
                    retval = 1;
                }
                break;
            }
        }
    }
    else
    {
        VERBOSE(VB_IMPORTANT, "Unable to run 'mkisofs'");
        retval = 1;
    }

    progress->Close();
    progress->deleteLater();

    progress = new MythProgressDialog(QObject::tr("Burning CD"), 100);
    progress->setProgress(2);

    args = QStringList("cdrecord");
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

    VERBOSE(VB_GENERAL, "Running: " + args.join(" "));

    Q3Process burn(args);

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
                    VERBOSE(VB_IMPORTANT, err);
                    burn.kill();
                    retval = 1;
                }
            }
            while (burn.canReadLineStdout())
            {
                QString line = burn.readLineStdout();
                if (line.mid(15, 2) == "of")
                {
                    int mbdone = line.mid(10, 5).trimmed().toInt();
                    int mbtotal = line.mid(17, 5).trimmed().toInt();

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
                    VERBOSE(VB_IMPORTANT, "Unable to run 'cdrecord'");
                    retval = 1;
                }
                break;
            }
        }
    }
    else
    {
        VERBOSE(VB_IMPORTANT, "Unable to run 'cdrecord'");
        retval = 1;
    }

    progress->Close();
    progress->deleteLater();

    QFile::remove(tmprecordlist);
    QFile::remove(tmprecordisofs);

    return retval;
}

int Playlist::CreateCDAudio(void)
{
    return -1;
}
