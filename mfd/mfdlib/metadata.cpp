/*
	metadata.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	metadata objects

*/

#include <iostream>
using namespace std;

#include <qstringlist.h>

#include "metadata.h"

Metadata::Metadata(
                    int l_collection_id,
                    int l_id, 
                    QUrl l_url,
                    int l_rating,
                    QDateTime l_last_played,
                    int l_play_count
                  )

{
    collection_id = l_collection_id,
    id = l_id;
    url = l_url;
    setRating(l_rating);
    last_played = l_last_played;
    play_count = l_play_count;
    metadata_type = MDT_unknown;
    db_id = -1;
    title = "";
    QDateTime when;
    when.setTime_t(0);
    date_added = when;
    date_modified = when;
    format = "";
    description = "";
    comment = "";
    size = -1;    
}

void Metadata::setRating(int l_rating)
{
    //
    //  must lie between 0 and 100
    //
    
    rating = l_rating;
    
    if(rating < 0)
    {
        rating = 0;
    }
    if(rating > 100)
    {
        rating = 100;
    }
}

int Metadata::getUniversalId()
{
    //
    //  This is a single number that we can use to find this object again
    //  (at some later point)
    //
    
    return ((collection_id * METADATA_UNIVERSAL_ID_DIVIDER) + id );
}

Metadata::~Metadata()
{
}


/*
---------------------------------------------------------------------
*/

AudioMetadata::AudioMetadata(
                                int l_collection_id,
                                int l_id,
                                QUrl l_url,
                                int l_rating,
                                QDateTime l_last_played,
                                int l_play_count,
                                QString l_artist, 
                                QString l_album, 
                                QString l_title, 
                                QString l_genre, 
                                int l_year, 
                                int l_tracknum, 
                                int l_length 
                            )
              :Metadata(
                        l_collection_id,
                        l_id, 
                        l_url, 
                        l_rating, 
                        l_last_played, 
                        l_play_count
                       )

{
    metadata_type = MDT_audio;
    artist = l_artist;
    album = l_album;
    title = l_title;
    genre = l_genre;
    year = l_year;
    tracknum = l_tracknum;
    length = l_length;

    bpm = -1;
    bitrate = -1;
    compilation = false;
    composer = "";
    disc_count = -1;
    disc_number = -1;
    eq_preset = "";
    relative_volume = 0;
    sample_rate = -1;
    start_time = -1;
    stop_time = -1;
    track_count = -1;    
}                            

AudioMetadata::AudioMetadata(
                             QString filename,
                             QString l_artist,
                             QString l_album,
                             QString l_title,
                             QString l_genre,
                             int     l_year,
                             int     l_tracknum,
                             int     l_length
                            )
              :Metadata(
                        -1,
                        -1, 
                        filename
                       )
{
    metadata_type = MDT_audio;
    url = filename;
    artist = l_artist;
    album = l_album;
    title = l_title;
    year = l_year;
    tracknum = l_tracknum;
    length = l_length;
    genre = l_genre;

    bpm = -1;
    bitrate = -1;
    compilation = false;
    composer = "";
    disc_count = -1;
    disc_number = -1;
    eq_preset = "";
    relative_volume = 0;
    sample_rate = -1;
    size = -1;
    start_time = -1;
    stop_time = -1;
    track_count = -1;
}


AudioMetadata::~AudioMetadata()
{
}

/*
---------------------------------------------------------------------
*/

Playlist::Playlist(int l_collection_id, QString new_name, QString raw_songlist, uint new_id)
{
    collection_id = l_collection_id;
    id = new_id;
    name = new_name;

    QStringList list = QStringList::split(",",raw_songlist);
    QStringList::iterator it = list.begin();
    for (; it != list.end(); it++)
    {
        //
        //  FIX THIS; throwing away playlists within playlists
        //
        if((*it).toInt() > 0)
        {
            db_references.append((*it).toUInt());
        }
    }
    waiting_for_list = false;
}

void Playlist::mapDatabaseToId(QIntDict<Metadata> *the_metadata)
{
    typedef QValueList<uint> UINTList;
    UINTList::iterator iter;
    for ( iter = db_references.begin(); iter != db_references.end(); ++iter )
    {
        Metadata *which_one = NULL;
        QIntDictIterator<Metadata> md_it( *the_metadata );
        for ( ; md_it.current(); ++md_it )
        {
            if((uint) md_it.current()->getId() == (*iter))
            {
                which_one = md_it.current();
                break;
            }
        }
        if(which_one)
        {
            song_references.append(which_one->getId());
        }
        else
        {
            // warning("playlist had an entry that did not map to any metadata");
        }
    }
}

void Playlist::addToList(int an_id)
{
    song_references.push_back(an_id);
}

uint Playlist::getUniversalId()
{
    return ((collection_id * METADATA_UNIVERSAL_ID_DIVIDER) + id );
}

Playlist::~Playlist()
{
}
