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
                    int l_play_count,
                    bool l_ephemeral
                  )

{
    collection_id = l_collection_id,
    id = l_id;
    url = l_url;
    rating = l_rating;
    last_played = l_last_played;
    play_count = l_play_count;
    ephemeral = l_ephemeral;
    metadata_type = MDT_unknown;
    db_id = -1;
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
                                bool l_ephemeral,
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
                        l_play_count, 
                        l_ephemeral
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
    url = filename;
    artist = l_artist;
    album = l_album;
    title = l_title;
    year = l_year;
    tracknum = l_tracknum;
    length = l_length;
    genre = l_genre;
}


AudioMetadata::~AudioMetadata()
{
}

/*
---------------------------------------------------------------------
*/
/*

AudioDBMetadata::AudioDBMetadata(
                                  int l_collection_id,
                                  int l_id,
                                  int l_db_id,
                                  QUrl l_url,
                                  int l_rating,
                                  QDateTime l_last_played,
                                  int l_play_count,
                                  bool l_ephemeral,
                                  QString l_artist, 
                                  QString l_album, 
                                  QString l_title, 
                                  QString l_genre, 
                                  int l_year, 
                                  int l_tracknum, 
                                  int l_length 
                            )
              :AudioMetadata(
                                l_collection_id,
                                l_id,
                                l_url,
                                l_rating,
                                l_last_played,
                                l_play_count,
                                l_ephemeral,
                                l_artist, 
                                l_album, 
                                l_title, 
                                l_genre, 
                                l_year, 
                                l_tracknum, 
                                l_length 
                            )
{
    db_id = l_db_id;
    changed = false;    
}                            

AudioDBMetadata::~AudioDBMetadata()
{
}

*/

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

uint Playlist::getUniversalId()
{
    return ((collection_id * METADATA_UNIVERSAL_ID_DIVIDER) + id );
}

Playlist::~Playlist()
{
}
