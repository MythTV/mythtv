/*
	mfdinfo.cpp

	Copyright (c) 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
*/

#include <iostream>
using namespace std;

#include "mfdinfo.h"

MfdInfo::MfdInfo( int an_id, const QString &a_name, const QString &a_host)
{
    id = an_id;
    name = a_name;
    host = a_host;
    mfd_content_collection = NULL;
    showing_menu = false;
    played_percentage = 0;
    pause_state = false;
    knows_whats_playing = true;
    current_container = -1;
    current_item = -1;
    previous_container = -2;
    previous_item = -2;
    current_elapsed = -1;
    is_stopped = true;
}

AudioMetadata*  MfdInfo::getAudioMetadata(int collection_id, int item_id)
{
    if(mfd_content_collection)
    {
        return mfd_content_collection->getAudioItem(collection_id, item_id);    
    }
    return NULL;
}

void MfdInfo::setCurrentPlayingData()
{
    if(
        current_container > -1 &&
        current_item > -1 &&
        current_elapsed > -1
      )
    {
        setCurrentPlayingData(current_container, current_item, current_elapsed);
    }
}


bool MfdInfo::setCurrentPlayingData(int which_container, int which_metadata, int numb_seconds)
{

    current_container = which_container;
    current_item = which_metadata;
    current_elapsed = numb_seconds;

    AudioMetadata *whats_playing = getAudioMetadata(which_container, which_metadata);
    playing_strings.clear();
    if(whats_playing)
    {
        bool return_value = true;
        playing_strings.append(whats_playing->getTitle());
        playing_strings.append(whats_playing->getArtist());
        playing_strings.append(whats_playing->getAlbum());
        played_percentage = (double) ((numb_seconds + 0.0) * 1000.0) / (whats_playing->getLength() + 0.0);
        knows_whats_playing = true;
        
        if(previous_container == current_container && previous_item == current_item)
        {
            return_value = false;
        }
        previous_container = current_container;
        previous_item = current_item;
     
        return return_value;   
    }
    else
    {
        played_percentage = 0.0;
        knows_whats_playing = false;
        previous_container = -2;
        previous_item = -2;
    }
    return false;
}


MfdInfo::~MfdInfo()
{
}



