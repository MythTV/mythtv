// C++
#include <algorithm>
#include <cinttypes>
#include <cstdlib>
#include <map>
#include <unistd.h>

// qt
#include <QApplication>
#include <QFileInfo>
#include <QObject>
#include <QRegularExpression>

// MythTV
#include <libmythbase/compat.h>
#include <libmythbase/exitcodes.h>
#include <libmythbase/mythcorecontext.h>
#include <libmythbase/mythdb.h>
#include <libmythbase/mythlogging.h>
#include <libmythbase/mythmiscutil.h>
#include <libmythbase/mythsystemlegacy.h>
#include <libmythui/mediamonitor.h>

// mythmusic
#include "musicdata.h"
#include "musicplayer.h"
#include "playlist.h"
#include "playlistcontainer.h"
#include "smartplaylist.h"

///////////////////////////////////////////////////////////////////////////////
// Playlist

#define LOC      QString("Playlist: ")
#define LOC_WARN QString("Playlist, Warning: ")
#define LOC_ERR  QString("Playlist, Error: ")

bool Playlist::checkTrack(MusicMetadata::IdType trackID) const
{
    return m_songs.contains(trackID);
}

void Playlist::copyTracks(Playlist *to_ptr, bool update_display)
{
    disableSaves();

    for (int x = 0; x < m_songs.size(); x++)
    {
        MusicMetadata *mdata = getRawSongAt(x);
        if (mdata)
        {
            if (mdata->isDBTrack())
                to_ptr->addTrack(mdata->ID(), update_display);
        }
    }

    enableSaves();

    changed();
}

/// Given a tracks ID, add that track to this playlist
void Playlist::addTrack(MusicMetadata::IdType trackID, bool update_display)
{
    int repo = ID_TO_REPO(trackID);
    MusicMetadata *mdata = nullptr;

    if (repo == RT_Radio)
        mdata = gMusicData->m_all_streams->getMetadata(trackID);
    else
        mdata = gMusicData->m_all_music->getMetadata(trackID);

    if (mdata)
    {
        m_songs.push_back(trackID);
        m_shuffledSongs.push_back(trackID);

        changed();

        if (update_display && isActivePlaylist())
            gPlayer->activePlaylistChanged(trackID, false);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Can't add track, given a bad track ID");
    }
}

void Playlist::removeAllTracks(void)
{
    m_songs.clear();
    m_shuffledSongs.clear();

    changed();
}

void Playlist::removeAllCDTracks(void)
{
    // find the cd tracks
    SongList cdTracks;
    for (int x = 0; x < m_songs.count(); x++)
    {
        MusicMetadata *mdata = getRawSongAt(x);

        if (mdata && mdata->isCDTrack())
            cdTracks.append(m_songs.at(x));
    }

    // remove the tracks from our lists
    for (int x = 0; x < cdTracks.count(); x++)
    {
        m_songs.removeAll(cdTracks.at(x));
        m_shuffledSongs.removeAll(cdTracks.at(x));;
    }

    changed();
}

void Playlist::removeTrack(MusicMetadata::IdType trackID)
{
    m_songs.removeAll(trackID);
    m_shuffledSongs.removeAll(trackID);

    changed();

    if (isActivePlaylist())
        gPlayer->activePlaylistChanged(trackID, true);
}

void Playlist::moveTrackUpDown(bool flag, int where_its_at)
{
    uint insertion_point = 0;
    MusicMetadata::IdType id = m_shuffledSongs.at(where_its_at);

    if (flag)
        insertion_point = ((uint)where_its_at) - 1;
    else
        insertion_point = ((uint)where_its_at) + 1;

    m_shuffledSongs.removeAt(where_its_at);
    m_shuffledSongs.insert(insertion_point, id);

    changed();
}

Playlist::Playlist(void) :
    m_name(tr("oops"))
{
}

Playlist::~Playlist()
{
    m_songs.clear();
    m_shuffledSongs.clear();
}

void Playlist::shuffleTracks(MusicPlayer::ShuffleMode shuffleMode)
{
    m_shuffledSongs.clear();

    switch (shuffleMode)
    {
        case MusicPlayer::SHUFFLE_RANDOM:
        {
            QMultiMap<int, MusicMetadata::IdType> songMap;

            for (int x = 0; x < m_songs.count(); x++)
            {
                // Pseudo-random is good enough. Don't need a true random.
                // NOLINTNEXTLINE(cert-msc30-c,cert-msc50-cpp)
                songMap.insert(rand(), m_songs.at(x));
            }

            QMultiMap<int, MusicMetadata::IdType>::const_iterator i = songMap.constBegin();
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

            for (int x = 0; x < m_songs.count(); x++)
            {
                MusicMetadata *mdata = getRawSongAt(x);
                if (!mdata)
                    continue;

                if (!mdata->isCDTrack())
                {

                    if (0 == x)
                    {
                        // first song
                        playcountMin = playcountMax = mdata->PlayCount();
                        lastplayMin = lastplayMax = mdata->LastPlay().toSecsSinceEpoch();
                    }
                    else
                    {
                        if (mdata->PlayCount() < playcountMin)
                            playcountMin = mdata->PlayCount();
                        else if (mdata->PlayCount() > playcountMax)
                            playcountMax = mdata->PlayCount();

                        double lastplaysecs = mdata->LastPlay().toSecsSinceEpoch();
                        if (lastplaysecs < lastplayMin)
                            lastplayMin = lastplaysecs;
                        else if (lastplaysecs > lastplayMax)
                            lastplayMax = lastplaysecs;
                    }
                }
            }

            // next we compute all the weights
            std::map<int,double> weights;
            std::map<int,int> ratings;
            std::map<int,int> ratingCounts;
            int TotalWeight = RatingWeight + PlayCountWeight + LastPlayWeight;
            for (int x = 0; x < m_songs.size(); x++)
            {
                MusicMetadata *mdata =  getRawSongAt(x);
                if (mdata && !mdata->isCDTrack())
                {
                    int rating = mdata->Rating();
                    int playcount = mdata->PlayCount();
                    double lastplaydbl = mdata->LastPlay().toSecsSinceEpoch();
                    double ratingValue = (double)(rating) / 10;
                    double playcountValue = __builtin_nan("");
                    double lastplayValue = __builtin_nan("");

                    if (playcountMax == playcountMin)
                        playcountValue = 0;
                    else
                        playcountValue = (((playcountMin - (double)playcount) / (playcountMax - playcountMin)) + 1);

                    if (lastplayMax == lastplayMin)
                        lastplayValue = 0;
                    else
                        lastplayValue = (((lastplayMin - lastplaydbl) / (lastplayMax - lastplayMin)) + 1);

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
            auto weightsEnd = weights.end();
            for (auto weightsIt = weights.begin() ; weightsIt != weightsEnd ; ++weightsIt)
            {
                weightsIt->second /= ratingCounts[ratings[weightsIt->first]];
                totalWeights += weightsIt->second;
            }

            // then we get a random order, balanced with relative weights of remaining songs
            std::map<int,uint32_t> order;
            uint32_t orderCpt = 1;
            while (!weights.empty())
            {
                // Pseudo-random is good enough. Don't need a true random.
                // NOLINTNEXTLINE(cert-msc30-c,cert-msc50-cpp)
                double hit = totalWeights * (double)rand() / (double)RAND_MAX;
                auto weightEnd = weights.end();
                auto weightIt = weights.begin();
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
            QMultiMap<int, MusicMetadata::IdType> songMap;
            for (int x = 0; x < m_songs.count(); x++)
                songMap.insert(order[m_songs.at(x)], m_songs.at(x));

            // copy the shuffled tracks to the shuffled song list
            QMultiMap<int, MusicMetadata::IdType>::const_iterator i = songMap.constBegin();
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

            using AlbumMap = std::map<QString, uint32_t>;
            AlbumMap                       album_map;
            AlbumMap::iterator             Ialbum;
            QString                        album;

            // pre-fill the album-map with the album name.
            // This allows us to do album mode in album order
            for (int x = 0; x < m_songs.count(); x++)
            {
                MusicMetadata *mdata = getRawSongAt(x);
                if (mdata)
                {
                    album = mdata->Album() + " ~ " + QString("%1").arg(mdata->getAlbumId());
                    Ialbum = album_map.find(album);
                    if (Ialbum == album_map.end())
                        album_map.insert(AlbumMap::value_type(album, 0));
                }
            }

            // populate the sort id into the album map
            uint32_t album_count = 1;
            for (Ialbum = album_map.begin(); Ialbum != album_map.end(); ++Ialbum)
            {
                Ialbum->second = album_count;
                album_count++;
            }

            // create a map of tracks sorted by the computed order
            QMultiMap<int, MusicMetadata::IdType> songMap;
            for (int x = 0;  x < m_songs.count(); x++)
            {
                MusicMetadata *mdata = getRawSongAt(x);
                if (mdata)
                {
                    uint32_t album_order = 1;
                    album = album = mdata->Album() + " ~ " + QString("%1").arg(mdata->getAlbumId());;
                    Ialbum = album_map.find(album);
                    if (Ialbum == album_map.end())
                    {
                        // we didn't find this album in the map,
                        // yet we pre-loaded them all. we are broken,
                        // but we just set the track order to 1, since there
                        // is no real point in reporting an error
                        album_order = 1;
                    }
                    else
                    {
                        album_order = Ialbum->second * 10000;
                    }
                    if (mdata->DiscNumber() != -1)
                        album_order += mdata->DiscNumber()*100;
                    album_order += mdata->Track();

                    songMap.insert(album_order, m_songs.at(x));
                }
            }

            // copy the shuffled tracks to the shuffled song list
            QMultiMap<int, MusicMetadata::IdType>::const_iterator i = songMap.constBegin();
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

            using ArtistMap = std::map<QString, uint32_t>;
            ArtistMap                      artist_map;
            ArtistMap::iterator            Iartist;
            QString                        artist;

            // pre-fill the album-map with the album name.
            // This allows us to do artist mode in artist order
            for (int x = 0; x < m_songs.count(); x++)
            {
                MusicMetadata *mdata = getRawSongAt(x);
                if (mdata)
                {
                    artist = mdata->Artist() + " ~ " + mdata->Title();
                    Iartist = artist_map.find(artist);
                    if (Iartist == artist_map.end())
                        artist_map.insert(ArtistMap::value_type(artist,0));
                }
            }

            // populate the sort id into the artist map
            uint32_t artist_count = 1;
            for (Iartist = artist_map.begin(); Iartist != artist_map.end(); ++Iartist)
            {
                Iartist->second = artist_count;
                artist_count++;
            }

            // create a map of tracks sorted by the computed order
            QMultiMap<int, MusicMetadata::IdType> songMap;
            for (int x = 0; x < m_songs.count(); x++)
            {
                MusicMetadata *mdata = getRawSongAt(x);
                if (mdata)
                {
                    uint32_t artist_order = 1;
                    artist = mdata->Artist() + " ~ " + mdata->Title();
                    Iartist = artist_map.find(artist);
                    if (Iartist == artist_map.end())
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

                    songMap.insert(artist_order, m_songs.at(x));
                }
            }

            // copy the shuffled tracks to the shuffled song list
            QMultiMap<int, MusicMetadata::IdType>::const_iterator i = songMap.constBegin();
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
            // NOLINTNEXTLINE(modernize-loop-convert)
            for (auto it = m_songs.begin(); it != m_songs.end(); ++it)
                m_shuffledSongs.append(*it);

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
    for (int x = 0; x < m_songs.count(); x++)
        msg += QString("%1,").arg(m_songs.at(x));

    LOG(VB_GENERAL, LOG_INFO, LOC + msg);
}

void Playlist::getStats(uint *trackCount, std::chrono::seconds *totalLength,
                        uint currenttrack, std::chrono::seconds *playedLength) const
{
    std::chrono::milliseconds total = 0ms;
    std::chrono::milliseconds played = 0ms;

    *trackCount = m_shuffledSongs.size();

    if ((int)currenttrack >= m_shuffledSongs.size())
        currenttrack = 0;

    for (int x = 0; x < m_shuffledSongs.count(); x++)
    {
        MusicMetadata *mdata = getSongAt(x);
        if (mdata)
        {
            total += mdata->Length();
            if (x < (int)currenttrack)
                played += mdata->Length();
        }
    }

    if (playedLength)
        *playedLength = duration_cast<std::chrono::seconds>(played);

    *totalLength = duration_cast<std::chrono::seconds>(total);
}

void Playlist::loadPlaylist(const QString& a_name, const QString& a_host)
{
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

void Playlist::loadPlaylistByID(int id, const QString& a_host)
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
        m_name = tr("Default Playlist");

    fillSongsFromSonglist(rawSonglist);
}

/// make sure all tracks are still valid after a scan
void Playlist::resync(void)
{
    bool needUpdate = false;

    for (int x = 0; x < m_songs.count(); x++)
    {
        MusicMetadata::IdType id = m_songs.at(x);
        MusicMetadata *mdata = getRawSongAt(x);
        if (!mdata)
        {
            m_songs.removeAll(id);
            m_shuffledSongs.removeAll(id);
            needUpdate = true;
        }
    }

    if (needUpdate)
    {
        changed();

        gPlayer->playlistChanged(m_playlistid);

        // TODO check we actually need this
        if (isActivePlaylist())
            gPlayer->activePlaylistChanged(-1, false);
    }
}

void Playlist::fillSongsFromSonglist(const QString& songList)
{
    bool badTrack = false;

    QStringList list = songList.split(",", Qt::SkipEmptyParts);
    for (const auto & song : std::as_const(list))
    {
        MusicMetadata::IdType id = song.toUInt();
        int repo = ID_TO_REPO(id);
        if (repo == RT_Radio)
        {
            // check this is a valid stream ID
            if (gMusicData->m_all_streams->isValidID(id))
                m_songs.push_back(id);
            else
            {
                badTrack = true;
                LOG(VB_GENERAL, LOG_ERR, LOC + QString("Got a bad track %1").arg(id));
            }
        }
        else
        {
            // check this is a valid track ID
            if (gMusicData->m_all_music->isValidID(id))
                m_songs.push_back(id);
            else
            {
                badTrack = true;
                LOG(VB_GENERAL, LOG_ERR, LOC + QString("Got a bad track %1").arg(id));
            }
        }
    }

    if (this == gPlayer->getCurrentPlaylist())
        shuffleTracks(gPlayer->getShuffleMode());
    else
        shuffleTracks(MusicPlayer::SHUFFLE_OFF);

    if (badTrack)
        changed();

    if (isActivePlaylist())
        gPlayer->activePlaylistChanged(-1, false);
}

int Playlist::fillSonglistFromQuery(const QString& whereClause,
                                    bool removeDuplicates,
                                    InsertPLOption insertOption,
                                    int currentTrackID)
{
    QString orig_songlist = toRawSonglist();
    QString new_songlist;
    int added = 0;

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
        return 0;
    }

    while (query.next())
    {
        new_songlist += "," + query.value(0).toString();
        added++;
    }
    new_songlist.remove(0, 1);

    if (removeDuplicates && insertOption != PL_REPLACE)
        orig_songlist = removeItemsFromList(new_songlist, orig_songlist);

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
            QStringList list = orig_songlist.split(",", Qt::SkipEmptyParts);
            bool bFound = false;
            QString tempList;
            for (const auto& song : std::as_const(list))
            {
                int an_int = song.toInt();
                tempList += "," + song;
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
    return added;
}

// songList is a list of trackIDs to add
int Playlist::fillSonglistFromList(const QList<int> &songList,
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
        orig_songlist = removeItemsFromList(new_songlist, orig_songlist);

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
            QStringList list = orig_songlist.split(",", Qt::SkipEmptyParts);
            bool bFound = false;
            QString tempList;
            for (const auto & song : std::as_const(list))
            {
                int an_int = song.toInt();
                tempList += "," + song;
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
    return songList.count();
}

QString Playlist::toRawSonglist(bool shuffled, bool tracksOnly)
{
    QString rawList = "";

    if (shuffled)
    {
        for (int x = 0; x < m_shuffledSongs.count(); x++)
        {
            MusicMetadata::IdType id = m_shuffledSongs.at(x);
            if (tracksOnly)
            {
                if (ID_TO_REPO(id) == RT_Database)
                    rawList += QString(",%1").arg(id);
            }
            else
            {
                rawList += QString(",%1").arg(id);
            }
        }
    }
    else
    {
        for (int x = 0; x < m_songs.count(); x++)
        {
            MusicMetadata::IdType id = m_songs.at(x);
            if (tracksOnly)
            {
                if (ID_TO_REPO(id) == RT_Database)
                    rawList += QString(",%1").arg(id);
            }
            else
            {
                rawList += QString(",%1").arg(id);
            }
        }
    }

    if (!rawList.isEmpty())
        rawList = rawList.remove(0, 1);

    return rawList;
}

int Playlist::fillSonglistFromSmartPlaylist(const QString& category, const QString& name,
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
        return 0;
    }

    // find smartplaylist
    int ID = 0;
    QString matchType;
    QString orderBy;
    int limitTo = 0;

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
            return 0;
        }
    }
    else
    {
        MythDB::DBError("Find SmartPlaylist", query);
        return 0;
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
            {
                whereClause += matchType + getCriteriaSQL(fieldName,
                                           operatorName, value1, value2);
            }
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

    return fillSonglistFromQuery(whereClause, removeDuplicates,
                                 insertOption, currentTrackID);
}

void Playlist::changed(void)
{
    m_changed = true;

    if (m_doSave)
        savePlaylist(m_name, gCoreContext->GetHostName());
}

void Playlist::savePlaylist(const QString& a_name, const QString& a_host)
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

    // get the shuffled list of tracks excluding any cd tracks and radio streams
    QString rawSonglist = toRawSonglist(true, true);

    MSqlQuery query(MSqlQuery::InitCon());
    uint songcount = 0;
    std::chrono::seconds playtime = 0s;

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
    query.bindValue(":PLAYTIME", qlonglong(playtime.count()));
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

// Return a copy of the second list, having removed any item that
// also appears in the first list.
//
// @param remove_list A comma separated list of strings to be
//                    removed from the source list.
// @param source_list A comma separated list of strings to be
//                    processed.
// @return A comma separated list of the strings remaining after
//         processing.
QString Playlist::removeItemsFromList(const QString &remove_list, const QString &source_list)
{
    QStringList removeList = remove_list.split(",", Qt::SkipEmptyParts);
    QStringList sourceList = source_list.split(",", Qt::SkipEmptyParts);
    QString songlist;

    for (const auto & song : std::as_const(sourceList))
    {
        if (removeList.indexOf(song) == -1)
            songlist += "," + song;
    }
    songlist.remove(0, 1);
    return songlist;
}

MusicMetadata* Playlist::getSongAt(int pos) const
{
    MusicMetadata *mdata = nullptr;

    if (pos >= 0 && pos < m_shuffledSongs.size())
    {
        MusicMetadata::IdType id = m_shuffledSongs.at(pos);
        int repo = ID_TO_REPO(id);

        if (repo == RT_Radio)
            mdata = gMusicData->m_all_streams->getMetadata(id);
        else
            mdata = gMusicData->m_all_music->getMetadata(id);
    }

    return mdata;
}

MusicMetadata* Playlist::getRawSongAt(int pos) const
{
    MusicMetadata *mdata = nullptr;

    if (pos >= 0 && pos < m_songs.size())
    {
        MusicMetadata::IdType id = m_songs.at(pos);
        int repo = ID_TO_REPO(id);

        if (repo == RT_Radio)
            mdata = gMusicData->m_all_streams->getMetadata(id);
        else
            mdata = gMusicData->m_all_music->getMetadata(id);
    }

    return mdata;
}

// Here begins CD Writing things. ComputeSize, CreateCDMP3 & CreateCDAudio
// FIXME none of this is currently used
#ifdef CD_WRTITING_FIXED
void Playlist::computeSize(double &size_in_MB, double &size_in_sec)
{
    //double child_MB;
    //double child_sec;

    // Clear return values
    size_in_MB = 0.0;
    size_in_sec = 0.0;

    for (int x = 0; x < m_songs.size(); x++)
    {
        MusicMetadata *mdata = getRawSongAt(x);
        if (mdata)
        {
            if (mdata->isCDTrack())
                continue;

            // Normal track
            if (mdata->Length() > 0ms)
                size_in_sec += duration_cast<floatsecs>(mdata->Length()).count();
            else
                LOG(VB_GENERAL, LOG_ERR, "Computing track lengths. "
                                         "One track <=0");

            size_in_MB += mdata->FileSize() / 1000000;
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
        static const QRegularExpression newline { "\\R" }; // Any unicode newline
        QStringList list = data.split(newline, Qt::SkipEmptyParts);

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

void Playlist::processExit(void)
{
    m_procExitVal = GENERIC_EXIT_OK;
}

// FIXME: this needs updating to work with storage groups
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

    for (int x = 0; x < m_shuffledSongs.count(); x++)
    {
        MusicMetadata *mdata = getRawSongAt(x);

        // Normal track
        if (mdata)
        {
            if (mdata->isCDTrack())
                continue;

            // check filename..
            QFileInfo testit(mdata->Filename());
            if (!testit.exists())
                continue;
            size_in_MB += testit.size() / 1000000.0;
            QString outline;
            if (MP3_dir_flag)
            {
                if (mdata->Artist().length() > 0)
                    outline += mdata->Artist() + "/";
                if (mdata->Album().length() > 0)
                    outline += mdata->Album() + "/";
            }

            outline += "=";
            outline += mdata->Filename();

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

    m_progress = new MythProgressDialog(tr("Creating CD File System"),
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

    uint flags = kMSRunShell | kMSStdErr |
                 kMSDontDisableDrawing | kMSDontBlockInputDevs |
                 kMSRunBackground;

    m_proc = new MythSystemLegacy(command, args, flags);

    connect(m_proc, &MythSystemLegacy::readDataReady, this, &Playlist::mkisofsData,
            Qt::DirectConnection);
    connect(m_proc, &MythSystemLegacy::finished,      this, qOverload<>(&Playlist::processExit),
            Qt::DirectConnection);
    connect(m_proc, &MythSystemLegacy::error,         this, qOverload<uint>(&Playlist::processExit),
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
        m_progress = new MythProgressDialog(tr("Burning CD"), 100);
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

        flags = kMSRunShell | kMSStdErr | kMSStdOut |
                kMSDontDisableDrawing | kMSDontBlockInputDevs |
                kMSRunBackground;

        m_proc = new MythSystemLegacy(command, args, flags);
        connect(m_proc, &MythSystemLegacy::readDataReady,
                this, &Playlist::cdrecordData, Qt::DirectConnection);
        connect(m_proc, &MythSystemLegacy::finished,
                this, qOverload<>(&Playlist::processExit), Qt::DirectConnection);
        connect(m_proc, &MythSystemLegacy::error,
                this, qOverload<uint>(&Playlist::processExit), Qt::DirectConnection);
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
#endif
