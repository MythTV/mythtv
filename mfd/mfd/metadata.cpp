/*
	metadata.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	metadata objects

*/

#include <iostream>
using namespace std;

#include "metadata.h"

Metadata::Metadata(
                    int l_id, 
                    QUrl l_url,
                    int l_rating,
                    QDateTime l_last_played,
                    int l_play_count,
                    bool l_ephemeral
                  )

{
    type = MT_GENERIC;
    id = l_id;
    url = l_url;
    rating = l_rating;
    last_played = l_last_played;
    play_count = l_play_count;
    ephemeral = l_ephemeral;
}

/*
---------------------------------------------------------------------
*/

AudioMetadata::AudioMetadata(
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
              :Metadata(
                        l_id, 
                        l_url, 
                        l_rating, 
                        l_last_played, 
                        l_play_count, 
                        l_ephemeral
                       )

{
    type = MT_AUDIO;
    db_id = l_db_id;
    artist = l_artist;
    album = l_album;
    title = l_title;
    genre = l_genre;
    year = l_year;
    tracknum = l_tracknum;
    length = l_length;
    
    changed = false;    
}                            

