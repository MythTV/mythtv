/*
	metadata.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	metadata objects

*/

#include <iostream>
using namespace std;

#include <openssl/evp.h>

#include <qstringlist.h>
#include <qfile.h>

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
    myth_digest = "";
    changed = false;
}

QString Metadata::getFilePath()
{
    QString return_value = url.toString(false, false);
    return return_value;
}

void Metadata::setRating(int l_rating)
{
    //
    //  must lie between 0 and 10
    //
    
    rating = l_rating;
    
    if(rating < 0)
    {
        rating = 0;
    }
    if(rating > 10)
    {
        rating = 10;
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

Playlist::Playlist(
                    int l_collection_id, 
                    QString new_name, 
                    QString raw_songlist, 
                    uint new_id
                  )
{
    collection_id = l_collection_id;
    id = new_id;
    name = new_name;

    raw_song_list = raw_songlist;
    QStringList list = QStringList::split(",",raw_songlist);
    QStringList::iterator it = list.begin();
    for (; it != list.end(); it++)
    {
        //
        //  Map string list of integers to actual metadata
        //

        bool ok;
        if((*it).toInt(&ok) != 0 && ok)
        {
            db_references.append((*it).toInt());
        }
        else
        {
            cerr << "this is not a valid entry for a playlist: \""
                 <<  (*it)
                 << "\"" << endl;
        }
    }

    internal_change = false;
    waiting_for_list = false;
}

Playlist::Playlist(
                    int l_collection_id, 
                    QString new_name, 
                    QValueList<int> *l_song_references,
                    uint new_id
                  )
{
    collection_id = l_collection_id;
    id = new_id;
    name = new_name;

    QValueList<int>::iterator it;
    for(it = l_song_references->begin(); it != l_song_references->end(); ++it)
    {
        song_references.push_back((*it));
    }

    internal_change = false;
    waiting_for_list = false;
}

void Playlist::mapDatabaseToId(
                                QIntDict<Metadata> *the_metadata, 
                                QValueList<int> *reference_list,
                                QValueList<int> *song_list,
                                QIntDict<Playlist> *the_playlists,
                                int depth
                              )
{
    if(depth == 0)
    {
        song_list->clear();
    }
    else if(depth > 20)
    {
        //
        //  Arbitrary recursion limit (to avoid endless recursion if people
        //  futz with their database directly
        //
        
        cerr << "playlists within playlists stopped trying to go any "
             << "deeper after hitting a depth of 20" 
             << endl;
        return;
    }

    QValueList<int>::iterator iter;
    for ( iter = reference_list->begin(); iter != reference_list->end(); ++iter )
    {
        if((*iter) > 0)
        {
            Metadata *which_one = NULL;
            QIntDictIterator<Metadata> md_it( *the_metadata );
            for ( ; md_it.current(); ++md_it )
            {
                if(md_it.current()->getDbId() == (*iter))
                {
                    which_one = md_it.current();
                    break;
                }
            }
            if(which_one)
            {
                song_list->append(which_one->getId());
            }
            else
            {
                //warning("playlist has entry that does not map to metadata");
            }
        }
        else
        {
            Playlist *which_one = NULL;
            QIntDictIterator<Playlist> pl_it( *the_playlists );
            for ( ; pl_it.current(); ++pl_it )
            {
                if(pl_it.current()->getDbId() == (*iter) * -1)
                {
                    which_one = pl_it.current();
                    break;
                }
            }
            if(which_one)
            {
                which_one->mapDatabaseToId(
                                            the_metadata, 
                                            which_one->getDbList(),
                                            song_list,
                                            the_playlists,
                                            depth + 1
                                          );
            }
            else
            {
                cerr << "playlist had a reference to another playlist "
                     << "that does not exist" 
                     << endl;
            }
            
        }
    }
}

void Playlist::addToList(int an_id)
{
    song_references.push_back(an_id);
}

bool Playlist::removeFromList(int an_id)
{
    int how_many = song_references.remove(an_id);
    if(how_many == 1)
    {
        return true;
    }
    if(how_many == 0)
    {
        return false;
    }
    cerr << "metadata.o: playlist had more than 1 reference to same track"
         << endl;
    return true;
}

void Playlist::checkAgainstMetadata(QIntDict<Metadata> *the_metadata)
{
    //
    //  Make sure all my song references actually point to something
    //   
    
    QValueList<int>::iterator it;
    for ( it = song_references.begin(); it != song_references.end(); )
    {
        if(!the_metadata->find((*it)))
        {
            it = song_references.remove(it);
        }
        else
        {
            ++it;
        }
    }
}

uint Playlist::getUniversalId()
{
    return ((collection_id * METADATA_UNIVERSAL_ID_DIVIDER) + id );
}

Playlist::~Playlist()
{
}
