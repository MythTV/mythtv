#include <unistd.h>
#include <inttypes.h>
#include <cstdlib>

#include <map>
#include <algorithm>
using namespace std;

// qt
#include <QApplication>
#include <QFileInfo>
#include <QObject>

// mythmusic
#include "playlist.h"
#include "playlistcontainer.h"
#include "smartplaylist.h"
#include "musicplayer.h"

// mythtv
#include <mythcontext.h>
#include <mythdb.h>
#include <compat.h>
#include <mythmediamonitor.h>
#include <mythmiscutil.h>
#include <mythsystem.h>
#include <exitcodes.h>

const char *kID0err = "Song with ID of 0 in playlist, this shouldn't happen.";

///////////////////////////////////////////////////////////////////////////////
// Playlist

#define LOC      QString("Playlist: ")
#define LOC_WARN QString("Playlist, Warning: ")
#define LOC_ERR  QString("Playlist, Error: ")

bool Playlist::checkTrack(int a_track_id) const
{
    if (m_songMap.contains(a_track_id))
        return true;

    return false;
}

void Playlist::copyTracks(Playlist *to_ptr, bool update_display)
{
    disableSaves();

    SongList::const_iterator it = m_songs.begin();
    for (; it != m_songs.end(); ++it)
    {
        to_ptr->addTrack(*it, update_display);
    }

    enableSaves();

    changed();
}

/// Given a tracks ID, add that track to this playlist
void Playlist::addTrack(int trackID, bool update_display)
{
    int repo = ID_TO_REPO(trackID);
    Metadata *mdata = NULL;

    if (repo == RT_Radio)
        mdata = gMusicData->all_streams->getMetadata(trackID);
    else
        mdata = gMusicData->all_music->getMetadata(trackID);

    if (mdata)
        addTrack(mdata, update_display);
    else
        LOG(VB_GENERAL, LOG_ERR, LOC + "Can't add track, given a bad track ID");
}

/// Given a tracks metadata, add that track to this playlist
void Playlist::addTrack(Metadata *mdata, bool update_display)
{
    m_songs.push_back(mdata);
    m_shuffledSongs.push_back(mdata);
    m_songMap.insert(mdata->ID(), mdata);

    changed();

    if (update_display)
        gPlayer->activePlaylistChanged(mdata->ID(), false);
}

void Playlist::removeAllTracks(void)
{
    m_songs.clear();
    m_songMap.clear();
    m_shuffledSongs.clear();

    changed();
}

void Playlist::removeTrack(int the_track)
{
    QMap<int, Metadata*>::iterator it = m_songMap.find(the_track);
    if (it != m_songMap.end())
    {
        m_songMap.remove(the_track);
        m_songs.removeAll(*it);
        m_shuffledSongs.removeAll(*it);
    }

    changed();

    gPlayer->activePlaylistChanged(the_track, true);
}

void Playlist::moveTrackUpDown(bool flag, int where_its_at)
{
    Metadata *the_track = m_shuffledSongs.at(where_its_at);

    if (!the_track)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "A playlist was asked to move a track, but can't find it");
        return;
    }

    moveTrackUpDown(flag, the_track);
}

void Playlist::moveTrackUpDown(bool flag, Metadata* mdata)
{
    uint insertion_point = 0;
    int where_its_at = m_shuffledSongs.indexOf(mdata);
    if (where_its_at < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "A playlist was asked to move a track, but can'd find it");
        return;
    }

    if (flag)
        insertion_point = ((uint)where_its_at) - 1;
    else
        insertion_point = ((uint)where_its_at) + 1;

    m_shuffledSongs.removeAt(where_its_at);
    m_shuffledSongs.insert(insertion_point, mdata);

    changed();
}

Playlist::Playlist(void) :
    m_playlistid(0),
    m_name(QObject::tr("oops")),
    m_parent(NULL),
    m_changed(false),
    m_doSave(true),
    m_progress(NULL),
    m_proc(NULL),
    m_procExitVal(0)
{
}

Playlist::~Playlist()
{
    m_songs.clear();
    m_songMap.clear();
    m_shuffledSongs.clear();
}

void Playlist::shuffleTracks(MusicPlayer::ShuffleMode shuffleMode)
{
    m_shuffledSongs.clear();

    switch (shuffleMode)
    {
        case MusicPlayer::SHUFFLE_RANDOM:
        {
            QMultiMap<int, Metadata*> songMap;

            SongList::const_iterator it = m_songs.begin();
            for (; it != m_songs.end(); ++it)
            {
                songMap.insert(rand(), *it);
            }

            QMultiMap<int, Metadata*>::const_iterator i = songMap.constBegin();
            while (i != songMap.constEnd())
            {
                m_shuffledSongs.append(i.value());
                ++i;
            }

            break;
        }

        case MusicPlayer::SHUFFLE_INTELLIGENT:
        {
            int RatingWeight = 2;
            int PlayCountWeight = 2;
            int LastPlayWeight = 2;
            int RandomWeight = 2;
            m_parent->FillIntelliWeights(RatingWeight, PlayCountWeight,
                                         LastPlayWeight, RandomWeight);

            // compute max/min playcount,lastplay for this playlist
            int playcountMin = 0;
            int playcountMax = 0;
            double lastplayMin = 0.0;
            double lastplayMax = 0.0;

            uint idx = 0;
            SongList::const_iterator it = m_songs.begin();
            for (; it != m_songs.end(); ++it, ++idx)
            {
                if (!(*it)->isCDTrack())
                {
                    Metadata *mdata = (*it);

                    if (0 == idx)
                    {
                        // first song
                        playcountMin = playcountMax = mdata->PlayCount();
                        lastplayMin = lastplayMax = mdata->LastPlay().toTime_t();
                    }
                    else
                    {
                        if (mdata->PlayCount() < playcountMin)
                            playcountMin = mdata->PlayCount();
                        else if (mdata->PlayCount() > playcountMax)
                            playcountMax = mdata->PlayCount();

                        if (mdata->LastPlay().toTime_t() < lastplayMin)
                            lastplayMin = mdata->LastPlay().toTime_t();
                        else if (mdata->LastPlay().toTime_t() > lastplayMax)
                            lastplayMax = mdata->LastPlay().toTime_t();
                    }
                }
            }

            // next we compute all the weights
            std::map<int,double> weights;
            std::map<int,int> ratings;
            std::map<int,int> ratingCounts;
            int TotalWeight = RatingWeight + PlayCountWeight + LastPlayWeight;
            for (int trackItI = 0; trackItI < m_songs.size(); ++trackItI)
            {
                Metadata *mdata = m_songs[trackItI];
                if (!mdata->isCDTrack())
                {
                    int rating = mdata->Rating();
                    int playcount = mdata->PlayCount();
                    double lastplaydbl = mdata->LastPlay().toTime_t();
                    double ratingValue = (double)(rating) / 10;
                    double playcountValue, lastplayValue;

                    if (playcountMax == playcountMin)
                        playcountValue = 0;
                    else
                        playcountValue = ((playcountMin - (double)playcount) / (playcountMax - playcountMin) + 1);

                    if (lastplayMax == lastplayMin)
                        lastplayValue = 0;
                    else
                        lastplayValue = ((lastplayMin - lastplaydbl) / (lastplayMax - lastplayMin) + 1);

                    double weight = (RatingWeight * ratingValue +
                                        PlayCountWeight * playcountValue +
                                        LastPlayWeight * lastplayValue) / TotalWeight;
                    weights[mdata->ID()] = weight;
                    ratings[mdata->ID()] = rating;
                    ++ratingCounts[rating];
                }
            }

            // then we divide weights with the number of songs in the rating class
            // (more songs in a class ==> lower weight, without affecting other classes)
            double totalWeights = 0;
            std::map<int,double>::iterator weightsIt, weightsEnd = weights.end();
            for (weightsIt = weights.begin() ; weightsIt != weightsEnd ; ++weightsIt)
            {
                weightsIt->second /= ratingCounts[ratings[weightsIt->first]];
                totalWeights += weightsIt->second;
            }

            // then we get a random order, balanced with relative weights of remaining songs
            std::map<int,uint32_t> order;
            uint32_t orderCpt = 1;
            std::map<int,double>::iterator weightIt, weightEnd;
            while (!weights.empty())
            {
                double hit = totalWeights * (double)rand() / (double)RAND_MAX;
                weightEnd = weights.end();
                weightIt = weights.begin();
                double pos = 0;
                while (weightIt != weightEnd)
                {
                    pos += weightIt->second;
                    if (pos >= hit)
                        break;
                    ++weightIt;
                }

                // FIXME If we don't exit here then we'll segfault, but it
                //       probably won't give us the desired randomisation
                //       either - There seems to be a flaw in this code, we
                //       erase items from the map but never adjust
                //       'totalWeights' so at a point 'pos' will never be
                //       greater or equal to 'hit' and we will always hit the
                //       end of the map
                if (weightIt == weightEnd)
                    break;

                order[weightIt->first] = orderCpt;
                totalWeights -= weightIt->second;
                weights.erase(weightIt);
                ++orderCpt;
            }

            // create a map of tracks sorted by the computed order
            QMultiMap<int, Metadata*> songMap;
            it = m_songs.begin();
            for (; it != m_songs.end(); ++it)
                songMap.insert(order[(*it)->ID()], *it);

            // copy the shuffled tracks to the shuffled song list
            QMultiMap<int, Metadata*>::const_iterator i = songMap.constBegin();
            while (i != songMap.constEnd())
            {
                m_shuffledSongs.append(i.value());
                ++i;
            }

            break;
        }

        case MusicPlayer::SHUFFLE_ALBUM:
        {
            // "intellegent/album" order

            typedef map<QString, uint32_t> AlbumMap;
            AlbumMap                       album_map;
            AlbumMap::iterator             Ialbum;
            QString                        album;

            // pre-fill the album-map with the album name.
            // This allows us to do album mode in album order
            SongList::const_iterator it = m_songs.begin();
            for (; it != m_songs.end(); ++it)
            {
                Metadata *mdata = (*it);
                album = mdata->Album() + " ~ " + QString("%1").arg(mdata->getAlbumId());
                if ((Ialbum = album_map.find(album)) == album_map.end())
                    album_map.insert(AlbumMap::value_type(album, 0));
            }

            // populate the sort id into the album map
            uint32_t album_count = 1;
            for (Ialbum = album_map.begin(); Ialbum != album_map.end(); ++Ialbum)
            {
                Ialbum->second = album_count;
                album_count++;
            }

            // create a map of tracks sorted by the computed order
            QMultiMap<int, Metadata*> songMap;
            it = m_songs.begin();
            for (; it != m_songs.end(); ++it)
            {
                uint32_t album_order;
                Metadata *mdata = (*it);
                if (mdata)
                {
                    album = album = mdata->Album() + " ~ " + QString("%1").arg(mdata->getAlbumId());;
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
                        album_order = Ialbum->second * 1000;
                    }
                    album_order += mdata->Track();

                    songMap.insert(album_order, *it);
                }
            }

            // copy the shuffled tracks to the shuffled song list
            QMultiMap<int, Metadata*>::const_iterator i = songMap.constBegin();
            while (i != songMap.constEnd())
            {
                m_shuffledSongs.append(i.value());
                ++i;
            }

            break;
        }

        case MusicPlayer::SHUFFLE_ARTIST:
        {
            // "intellegent/album" order

            typedef map<QString, uint32_t> ArtistMap;
            ArtistMap                      artist_map;
            ArtistMap::iterator            Iartist;
            QString                        artist;

            // pre-fill the album-map with the album name.
            // This allows us to do artist mode in artist order
            SongList::const_iterator it = m_songs.begin();
            for (; it != m_songs.end(); ++it)
            {
                Metadata *mdata = (*it);
                artist = mdata->Artist() + " ~ " + mdata->Title();
                if ((Iartist = artist_map.find(artist)) == artist_map.end())
                    artist_map.insert(ArtistMap::value_type(artist,0));
            }

            // populate the sort id into the artist map
            uint32_t artist_count = 1;
            for (Iartist = artist_map.begin(); Iartist != artist_map.end(); ++Iartist)
            {
                Iartist->second = artist_count;
                artist_count++;
            }

            // create a map of tracks sorted by the computed order
            QMultiMap<int, Metadata*> songMap;
            it = m_songs.begin();
            for (; it != m_songs.end(); ++it)
            {
                uint32_t artist_order;
                Metadata *mdata = (*it);
                if (mdata)
                {
                    artist = mdata->Artist() + " ~ " + mdata->Title();
                    if ((Iartist = artist_map.find(artist)) == artist_map.end())
                    {
                        // we didn't find this artist in the map,
                        // yet we pre-loaded them all. we are broken,
                        // but we just set the track order to 1, since there
                        // is no real point in reporting an error
                        artist_order = 1;
                    }
                    else
                    {
                        artist_order = Iartist->second * 1000;
                    }
                    artist_order += mdata->Track();

                    songMap.insert(artist_order, *it);
                }
            }

            // copy the shuffled tracks to the shuffled song list
            QMultiMap<int, Metadata*>::const_iterator i = songMap.constBegin();
            while (i != songMap.constEnd())
            {
                m_shuffledSongs.append(i.value());
                ++i;
            }

            break;
        }

        default:
        {
            // copy the raw song list to the shuffled track list
            SongList::const_iterator it = m_songs.begin();
            for (; it != m_songs.end(); ++it)
            {
                m_shuffledSongs.append(*it);
            }

            break;
        }
    }
}

void Playlist::describeYourself(void) const
{
    //  This is for debugging
#if 0
    LOG(VB_GENERAL, LOG_DEBUG,
        QString("Playlist with name of \"%1\"").arg(name));
    LOG(VB_GENERAL, LOG_DEBUG,
        QString("        playlistid is %1").arg(laylistid));
    LOG(VB_GENERAL, LOG_DEBUG,
        QString("     songlist(raw) is \"%1\"").arg(raw_songlist));
    LOG(VB_GENERAL, LOG_DEBUG, "     songlist list is ");
#endif

    QString msg;
    SongList::const_iterator it = m_songs.begin();
    for (; it != m_songs.end(); ++it)
        msg += (*it)->ID() + ",";

    LOG(VB_GENERAL, LOG_INFO, LOC + msg);
}

void Playlist::getStats(uint *trackCount, uint *totalLength, uint currenttrack, uint *playedLength) const
{
    uint64_t total = 0, played = 0;

    *trackCount = m_shuffledSongs.size();

    if ((int)currenttrack >= m_shuffledSongs.size())
        currenttrack = 0;

    uint track = 0;
    SongList::const_iterator it = m_shuffledSongs.begin();
    for (; it != m_shuffledSongs.end(); ++it, ++track)
    {
        Metadata *mdata = (*it);
        if (mdata)
        {
            total += mdata->Length();
            if (track < currenttrack)
                played += mdata->Length();
        }
    }

    if (playedLength)
        *playedLength = played / 1000;

    *totalLength = total / 1000;
}

void Playlist::loadPlaylist(QString a_name, QString a_host)
{
    QString thequery;
    QString rawSonglist;

    if (a_host.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "loadPlaylist() - We need a valid hostname");
        return;
    }

    MSqlQuery query(MSqlQuery::InitCon());

    if (m_name == "default_playlist_storage" ||
        m_name == "stream_playlist")
    {
        query.prepare("SELECT playlist_id, playlist_name, playlist_songs "
                      "FROM  music_playlists "
                      "WHERE playlist_name = :NAME"
                      " AND hostname = :HOST;");
    }
    else
    {
        // Technically this is never called as this function
        // is only used to load the default playlist.
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
            m_playlistid = query.value(0).toInt();
            m_name = query.value(1).toString();
            rawSonglist = query.value(2).toString();
        }
    }
    else
    {
        // Asked me to load a playlist I can't find so let's create a new one :)
        m_playlistid = 0; // Be safe just in case we call load over the top
                          // of an existing playlist
        rawSonglist.clear();
        savePlaylist(a_name, a_host);
    }

    fillSongsFromSonglist(rawSonglist);

    shuffleTracks(MusicPlayer::SHUFFLE_OFF);
}

void Playlist::loadPlaylistByID(int id, QString a_host)
{
    QString rawSonglist;
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
        m_playlistid = query.value(0).toInt();
        m_name = query.value(1).toString();
        rawSonglist = query.value(2).toString();
    }

    if (m_name == "default_playlist_storage")
        m_name = QObject::tr("Default Playlist");

    fillSongsFromSonglist(rawSonglist);
}

void Playlist::fillSongsFromSonglist(QString songList)
{
    Metadata::IdType id;
    bool badTrack = false;

    QStringList list = songList.split(",", QString::SkipEmptyParts);
    QStringList::iterator it = list.begin();
    for (; it != list.end(); ++it)
    {
        id = (*it).toUInt();
        int repo = ID_TO_REPO(id);
        if (repo == RT_Radio)
        {
            // check this is a valid stream ID
            if (gMusicData->all_streams->isValidID(id))
            {
                Metadata *mdata = gMusicData->all_streams->getMetadata(id);
                m_songs.push_back(mdata);
                m_songMap.insert(id, mdata);
            }
            else
            {
                badTrack = true;
                LOG(VB_GENERAL, LOG_ERR, LOC + QString("Got a bad track %1").arg(id));
            }
        }
        else
        {
            // check this is a valid track ID
            if (gMusicData->all_music->isValidID(id))
            {
                Metadata *mdata = gMusicData->all_music->getMetadata(id);
                m_songs.push_back(mdata);
                m_songMap.insert(id, mdata);
            }
            else
            {
                badTrack = true;
                LOG(VB_GENERAL, LOG_ERR, LOC + QString("Got a bad track %1").arg(id));
            }
        }
    }

    if (this == gPlayer->getPlaylist())
        shuffleTracks(gPlayer->getShuffleMode());
    else
        shuffleTracks(MusicPlayer::SHUFFLE_OFF);

    if (badTrack)
        changed();

    gPlayer->activePlaylistChanged(-1, false);
}

void Playlist::fillSonglistFromQuery(QString whereClause,
                                     bool removeDuplicates,
                                     InsertPLOption insertOption,
                                     int currentTrackID)
{
    QString orig_songlist = toRawSonglist();
    QString new_songlist;

    disableSaves();
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
        new_songlist.clear();
        fillSongsFromSonglist(new_songlist);
        enableSaves();
        changed();
        return;
    }

    while (query.next())
    {
        new_songlist += "," + query.value(0).toString();
    }
    new_songlist.remove(0, 1);

    if (removeDuplicates && insertOption != PL_REPLACE)
        new_songlist = removeDuplicateTracks(orig_songlist, new_songlist);

    switch (insertOption)
    {
        case PL_REPLACE:
            break;

        case PL_INSERTATBEGINNING:
            new_songlist = new_songlist + "," + orig_songlist;
            break;

        case PL_INSERTATEND:
            new_songlist = orig_songlist + "," + new_songlist;
            break;

        case PL_INSERTAFTERCURRENT:
        {
            QStringList list = orig_songlist.split(",", QString::SkipEmptyParts);
            QStringList::iterator it = list.begin();
            bool bFound = false;
            QString tempList;
            for (; it != list.end(); ++it)
            {
                int an_int = (*it).toInt();
                tempList += "," + QString(*it);
                if (!bFound && an_int == currentTrackID)
                {
                    bFound = true;
                    tempList += "," + new_songlist;
                }
            }

            if (!bFound)
                tempList = orig_songlist + "," + new_songlist;

            new_songlist = tempList.remove(0, 1);

            break;
        }

        default:
            new_songlist = orig_songlist;
    }

    fillSongsFromSonglist(new_songlist);

    enableSaves();
    changed();
}

// songList is a list of trackIDs to add
void Playlist::fillSonglistFromList(const QList<int> &songList,
                                    bool removeDuplicates,
                                    InsertPLOption insertOption,
                                    int currentTrackID)
{
    QString orig_songlist = toRawSonglist();
    QString new_songlist;

    disableSaves();

    removeAllTracks();

    for (int x = 0; x < songList.count(); x++)
    {
        new_songlist += "," + QString::number(songList.at(x));
    }
    new_songlist.remove(0, 1);

    if (removeDuplicates && insertOption != PL_REPLACE)
        new_songlist = removeDuplicateTracks(orig_songlist, new_songlist);

    switch (insertOption)
    {
        case PL_REPLACE:
            break;

        case PL_INSERTATBEGINNING:
            new_songlist = new_songlist + "," + orig_songlist;
            break;

        case PL_INSERTATEND:
            new_songlist = orig_songlist + "," + new_songlist;
            break;

        case PL_INSERTAFTERCURRENT:
        {
            QStringList list = orig_songlist.split(",", QString::SkipEmptyParts);
            QStringList::iterator it = list.begin();
            bool bFound = false;
            QString tempList;
            for (; it != list.end(); ++it)
            {
                int an_int = QString(*it).toInt();
                tempList += "," + QString(*it);
                if (!bFound && an_int == currentTrackID)
                {
                    bFound = true;
                    tempList += "," + new_songlist;
                }
            }

            if (!bFound)
                tempList = orig_songlist + "," + new_songlist;

            new_songlist = tempList.remove(0, 1);

            break;
        }

        default:
            new_songlist = orig_songlist;
    }

    fillSongsFromSonglist(new_songlist);

    enableSaves();

    changed();
}

QString Playlist::toRawSonglist(bool shuffled)
{
    QString rawList;

    if (shuffled)
    {
        SongList::const_iterator it = m_shuffledSongs.begin();
        for (; it != m_shuffledSongs.end(); ++it)
        {
            rawList += QString(",%1").arg((*it)->ID());
        }
    }
    else
    {
        SongList::const_iterator it = m_songs.begin();
        for (; it != m_songs.end(); ++it)
        {
            rawList += QString(",%1").arg((*it)->ID());
        }
    }

    if (!rawList.isEmpty())
        rawList = rawList.remove(0, 1);

    return rawList;
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
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("Cannot find Smartplaylist Category: %1") .arg(category));
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
            LOG(VB_GENERAL, LOG_WARNING, LOC +
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

void Playlist::changed(void)
{
    m_changed = true;

    if (m_doSave)
        savePlaylist(m_name, gCoreContext->GetHostName());
}

void Playlist::savePlaylist(QString a_name, QString a_host)
{
    LOG(VB_GENERAL, LOG_DEBUG, LOC + "Saving playlist: " + a_name);

    m_name = a_name.simplified();
    if (m_name.isEmpty())
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Not saving unnamed playlist");
        return;
    }

    if (a_host.isEmpty())
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Not saving playlist without a host name");
        return;
    }

    QString rawSonglist = toRawSonglist(true);

    MSqlQuery query(MSqlQuery::InitCon());
    uint songcount = 0, playtime = 0;

    getStats(&songcount, &playtime);

    bool save_host = ("default_playlist_storage" == a_name);
    if (m_playlistid > 0)
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
        query.bindValue(":ID", m_playlistid);
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
    query.bindValue(":LIST", rawSonglist);
    query.bindValue(":NAME", a_name);
    query.bindValue(":SONGCOUNT", songcount);
    query.bindValue(":PLAYTIME", qlonglong(playtime));
    if (save_host)
        query.bindValue(":HOSTNAME", a_host);

    if (!query.exec() || (m_playlistid < 1 && query.numRowsAffected() < 1))
    {
        MythDB::DBError("Problem saving playlist", query);
    }

    if (m_playlistid < 1)
        m_playlistid = query.lastInsertId().toInt();

    m_changed = false;
}

QString Playlist::removeDuplicateTracks(const QString &orig_songlist, const QString &new_songlist)
{
    QStringList curList = orig_songlist.split(",", QString::SkipEmptyParts);
    QStringList newList = new_songlist.split(",", QString::SkipEmptyParts);
    QStringList::iterator it = newList.begin();
    QString songlist;

    for (; it != newList.end(); ++it)
    {
        if (curList.indexOf(*it) == -1)
            songlist += "," + *it;
    }
    songlist.remove(0, 1);
    return songlist;
}

Metadata* Playlist::getSongAt(int pos)
{
    if (pos >= 0 && pos < m_shuffledSongs.size())
        return m_shuffledSongs.at(pos);

    return NULL;
}


// Here begins CD Writing things. ComputeSize, CreateCDMP3 & CreateCDAudio
// FIXME non of this is currently used

void Playlist::computeSize(double &size_in_MB, double &size_in_sec)
{
    //double child_MB;
    //double child_sec;

    // Clear return values
    size_in_MB = 0.0;
    size_in_sec = 0.0;

    SongList::const_iterator it = m_songs.begin();
    for (; it != m_songs.end(); ++it)
    {
        if ((*it)->isCDTrack())
            continue;

        // Normal track
        Metadata *tmpdata = (*it);
        if (tmpdata)
        {
            if (tmpdata->Length() > 0)
                size_in_sec += tmpdata->Length();
            else
                LOG(VB_GENERAL, LOG_ERR, "Computing track lengths. "
                                         "One track <=0");

            // Check tmpdata->Filename
            QFileInfo finfo(tmpdata->Filename());

            size_in_MB += finfo.size() / 1000000;
        }
    }
}

void Playlist::cdrecordData(int fd)
{
    if (!m_progress || !m_proc)
        return;

    QByteArray buf;
    if (fd == 1)
    {
        buf = m_proc->ReadAll();

        // I would just use the QTextStream::readLine(), but wodim uses \r
        // to update the same line, so I'm splitting it on \r or \n
        // Track 01:    6 of  147 MB written (fifo 100%) [buf  99%]  16.3x.
        QString data(buf);
        QStringList list = data.split(QRegExp("[\\r\\n]"),
                                      QString::SkipEmptyParts);

        for (int i = 0; i < list.size(); i++)
        {
            QString line = list.at(i);

            if (line.mid(15, 2) == "of")
            {
                int mbdone  = line.mid(10, 5).trimmed().toInt();
                int mbtotal = line.mid(17, 5).trimmed().toInt();

                if (mbtotal > 0)
                {
                    m_progress->setProgress((mbdone * 100) / mbtotal);
                }
            }
        }
    }
    else
    {
        buf = m_proc->ReadAllErr();

        QTextStream text(buf);

        while (!text.atEnd())
        {
            QString err = text.readLine();
            if (err.contains("Drive needs to reload the media") ||
                err.contains("Input/output error.") ||
                err.contains("No disk / Wrong disk!"))
            {
                LOG(VB_GENERAL, LOG_ERR, err);
                m_proc->Term();
            }
        }
    }
}

void Playlist::mkisofsData(int fd)
{
    if (!m_progress || !m_proc)
        return;

    QByteArray buf;
    if (fd == 1)
        buf = m_proc->ReadAll();
    else
    {
        buf = m_proc->ReadAllErr();

        QTextStream text(buf);

        while (!text.atEnd())
        {
            QString line = text.readLine();
            if (line[6] == '%')
            {
                line = line.mid(0, 3);
                m_progress->setProgress(line.trimmed().toInt());
            }
        }
    }
}

void Playlist::processExit(uint retval)
{
    m_procExitVal = retval;
}

int Playlist::CreateCDMP3(void)
{
    // Check & get global settings
    if (!gCoreContext->GetNumSetting("CDWriterEnabled"))
    {
        LOG(VB_GENERAL, LOG_ERR, "CD Writer is not enabled.");
        return 1;
    }

    QString scsidev = MediaMonitor::defaultCDWriter();
    if (scsidev.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "No CD Writer device defined.");
        return 1;
    }

    int disksize = gCoreContext->GetNumSetting("CDDiskSize", 2);
    QString writespeed = gCoreContext->GetSetting("CDWriteSpeed", "2");
    bool MP3_dir_flag = gCoreContext->GetNumSetting("CDCreateDir", 1);

    double size_in_MB = 0.0;

    QStringList reclist;

    SongList::const_iterator it = m_songs.begin();
    for (; it != m_songs.end(); ++it)
    {
        if ((*it)->isCDTrack())
            continue;

        // Normal track
        Metadata *tmpdata = (*it);
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

    int max_size;
    if (disksize == 0)
        max_size = 650;
    else
        max_size = 700;

    if (size_in_MB >= max_size)
    {
        LOG(VB_GENERAL, LOG_ERR, "MP3 CD creation aborted -- cd size too big.");
        return 1;
    }

    // probably should tie stdout of mkisofs to stdin of cdrecord sometime
    QString tmptemplate("/tmp/mythmusicXXXXXX");

    QString tmprecordlist = createTempFile(tmptemplate);
    if (tmprecordlist == tmptemplate)
    {
        LOG(VB_GENERAL, LOG_ERR, "Unable to open temporary file");
        return 1;
    }

    QString tmprecordisofs = createTempFile(tmptemplate);
    if (tmprecordisofs == tmptemplate)
    {
        LOG(VB_GENERAL, LOG_ERR, "Unable to open temporary file");
        return 1;
    }

    QFile reclistfile(tmprecordlist);

    if (!reclistfile.open(QIODevice::WriteOnly))
    {
        LOG(VB_GENERAL, LOG_ERR, "Unable to open temporary file");
        return 1;
    }

    QTextStream recstream(&reclistfile);

    QStringList::Iterator iter;

    for (iter = reclist.begin(); iter != reclist.end(); ++iter)
    {
        recstream << *iter << "\n";
    }

    reclistfile.close();

    m_progress = new MythProgressDialog(QObject::tr("Creating CD File System"),
                                      100);
    m_progress->setProgress(1);

    QStringList args;
    QString command;

    command = "mkisofs";
    args << "-graft-points";
    args << "-path-list";
    args << tmprecordlist;
    args << "-o";
    args << tmprecordisofs;
    args << "-J";
    args << "-R";

    uint flags = kMSRunShell | kMSStdErr | kMSBuffered |
                 kMSDontDisableDrawing | kMSDontBlockInputDevs |
                 kMSRunBackground;

    m_proc = new MythSystem(command, args, flags);

    connect(m_proc, SIGNAL(readDataReady(int)), this, SLOT(mkisofsData(int)),
            Qt::DirectConnection);
    connect(m_proc, SIGNAL(finished()),         this, SLOT(processExit()),
            Qt::DirectConnection);
    connect(m_proc, SIGNAL(error(uint)),        this, SLOT(processExit(uint)),
            Qt::DirectConnection);

    m_procExitVal = GENERIC_EXIT_RUNNING;
    m_proc->Run();

    while( m_procExitVal == GENERIC_EXIT_RUNNING )
        usleep( 100000 );

    uint retval = m_procExitVal;

    m_progress->Close();
    m_progress->deleteLater();
    m_proc->disconnect();
    delete m_proc;

    if (retval)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Unable to run mkisofs: returns %1")
                .arg(retval));
    }
    else
    {
        m_progress = new MythProgressDialog(QObject::tr("Burning CD"), 100);
        m_progress->setProgress(2);

        command = "cdrecord";
        args = QStringList();
        args << "-v";
        //args << "-dummy";
        args << QString("dev=%1").arg(scsidev);

        if (writespeed.toInt() > 0)
        {
            args << "-speed=";
            args << writespeed;
        }

        args << "-data";
        args << tmprecordisofs;

        flags = kMSRunShell | kMSStdErr | kMSStdOut | kMSBuffered |
                kMSDontDisableDrawing | kMSDontBlockInputDevs |
                kMSRunBackground;

        m_proc = new MythSystem(command, args, flags);
        connect(m_proc, SIGNAL(readDataReady(int)),
                this, SLOT(cdrecordData(int)), Qt::DirectConnection);
        connect(m_proc, SIGNAL(finished()),
                this, SLOT(processExit()), Qt::DirectConnection);
        connect(m_proc, SIGNAL(error(uint)),
                this, SLOT(processExit(uint)), Qt::DirectConnection);
        m_procExitVal = GENERIC_EXIT_RUNNING;
        m_proc->Run();

        while( m_procExitVal == GENERIC_EXIT_RUNNING )
            usleep( 100000 );

        retval = m_procExitVal;

        m_progress->Close();
        m_progress->deleteLater();
        m_proc->disconnect();
        delete m_proc;

        if (retval)
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Unable to run cdrecord: returns %1") .arg(retval));
        }
    }

    QFile::remove(tmprecordlist);
    QFile::remove(tmprecordisofs);

    return retval;
}

int Playlist::CreateCDAudio(void)
{
    return -1;
}
