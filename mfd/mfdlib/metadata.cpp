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
    marked_as_duplicate = false;
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
    flat_tree_item_count = 0;
    collection_id = l_collection_id;
    id = new_id;
    db_id = -1;
    is_editable = false;
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
    user_new_list = false;
    marked_for_deletion = false;
}

Playlist::Playlist(
                    int l_collection_id, 
                    QString new_name, 
                    QValueList<int> *l_song_references,
                    uint new_id
                  )
{
    flat_tree_item_count = 0;
    collection_id = l_collection_id;
    id = new_id;
    db_id = -1;
    is_editable = false;
    name = new_name;

    QValueList<int>::iterator it;
    for(it = l_song_references->begin(); it != l_song_references->end(); ++it)
    {
        ++flat_tree_item_count;
        song_references.push_back((*it));
    }

    internal_change = false;
    waiting_for_list = false;
    user_new_list = false;
    marked_for_deletion = false;
}

void Playlist::mapDatabaseToId(
                                QIntDict<Metadata> *the_metadata, 
                                QValueList<int> *reference_list,
                                QValueList<int> *song_list,
                                QIntDict<Playlist> *the_playlists,
                                int depth,
                                bool flatten_playlists,
                                bool prune_dead
                              )
{
    if(depth == 0)
    {
        song_list->clear();
        flat_tree_item_count = 0;
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
    for ( iter = reference_list->begin(); iter != reference_list->end(); )
    {
        bool advance_iter = true;
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
                flat_tree_item_count++;
            }
            else
            {
                //
                //  Something here doesn't map to any metadata
                //
                
                if(prune_dead)
                {
                    iter = reference_list->remove(iter);
                    advance_iter = false;
                }
                else
                {
                    is_editable = false;
                }
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
                if(flatten_playlists)
                {
                    which_one->mapDatabaseToId(
                                                the_metadata, 
                                                which_one->getDbList(),
                                                song_list,
                                                the_playlists,
                                                depth + 1,
                                                flatten_playlists,
                                                prune_dead
                                              );
                    flat_tree_item_count = song_list->count();
                }
                else
                {
                    //  
                    //  Just put the playlist id on the list, but as a negative value
                    //
                    
                    int reference_id = which_one->getId();
                    reference_id = reference_id * -1;
                    song_list->append(reference_id);
                    
                    //
                    //  We still need to ask this sub playlist to map out
                    //  it's playlist entries (on itself, at depth 0) in
                    //  order to get an accurate flat_tree_item_count from
                    //  it.
                    //
                    
                    which_one->mapDatabaseToId(
                                                the_metadata,
                                                which_one->getDbList(),
                                                which_one->getListPtr(),
                                                the_playlists,
                                                0,
                                                false,
                                                prune_dead
                                              );
                    flat_tree_item_count += which_one->getFlatCount();
                }
            }
            else
            {
                //
                //  cerr << "playlist had a reference to another playlist "
                //     << "that does not exist" 
                //     << endl;
                //
                //  ^^ which is fine, 'cause someone may have just deleted a playlist
            }
        }
        if(advance_iter)
        {
            ++iter;
        }
    }
}

void Playlist::mapIdToDatabase(
                                QIntDict<Metadata> *the_metadata, 
                                QIntDict<Playlist> *the_playlists
                              )
{
    //
    //  Use the song_references list to (re-)create db_references
    //

    db_references.clear();
    QValueList<int>::iterator iter;
    for ( iter = song_references.begin(); iter != song_references.end(); ++iter )
    {
        if((*iter) > 0)
        {
            Metadata *which_one = NULL;
            which_one = the_metadata->find( (*iter) );
            if(which_one)
            {
                db_references.append(which_one->getDbId());
            }
            else
            {
                warning("playlist has a track entry that does not map to a DB entry");
            }
        }
        else
        {
            Playlist *which_one = NULL;
            which_one = the_playlists->find( (*iter) * -1 );
            if(which_one)
            {
                db_references.append(which_one->getDbId() * -1);
            }
            else
            {
                cerr << "playlist had a track reference to another playlist "
                     << "that does not exist" 
                     << endl;
            }
        }
    }
}

void Playlist::addToList(int an_id)
{
    song_references.push_back(an_id);
    flat_tree_item_count++;
}

bool Playlist::removeFromList(int an_id)
{
    bool deleted_something = false;

    //
    //  This is slightly broken at the moment. It will remove the first
    //  reference to the track, even if the source of the deletion was a
    //  different reference.
    //

    QValueList<int>::Iterator it;
    for( it = song_references.begin(); it != song_references.end(); ++it )
    {
        if( (*it) == an_id)
        {
            song_references.remove(it);
            deleted_something = true;
            flat_tree_item_count--;
            break;
        }
    }

    return deleted_something;
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
            flat_tree_item_count--;
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

void Playlist::addToIndirectMap(int key, int value)
{
    indirect_map[key] = value;
}

int Playlist::getFromIndirectMap(int key)
{
    int return_value = -1;
    if(indirect_map.find(key) != indirect_map.end())
    {
        return_value = indirect_map[key];
    }
    return return_value;
}

void Playlist::removeFromIndirectMap(int key)
{
    indirect_map.remove(key);
}

Playlist::~Playlist()
{
}
