/*
	metadatacollection.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Object to hold metadata collections sent over the wire

*/

#include <iostream>
using namespace std;

#include "metadatacollection.h"
#include "../mdcaplib/mdcapinput.h"
#include "../mdcaplib/markupcodes.h"

MetadataCollection::MetadataCollection(
                                        int an_id,
                                        int new_collection_count,
                                        QString new_collection_name,
                                        int new_collection_generation,
                                        MetadataType l_metadata_type
                                      )
{
    id = an_id;
    expected_count = new_collection_count;
    name = new_collection_name;
    pending_generation = new_collection_generation;
    metadata_type = l_metadata_type;

    current_metadata_generation = 0;
    current_playlist_generation = 0;
    current_combined_generation = 0;

    //
    //  Prep the thing that holds the metadata
    //
    
    metadata.resize(9973);          // Big prime
    metadata.setAutoDelete(true);
    
    //
    //  Prep the thing that holds the playlists
    //
    
    playlists.resize(9973);          // Big prime
    playlists.setAutoDelete(true);
    
    
}

bool MetadataCollection::itemsUpToDate()
{
    if(current_metadata_generation == 0)
    {
        return false;
    }
    
    if(current_metadata_generation == pending_generation)
    {
        return true;
    }
    
    return false;
}

bool MetadataCollection::listsUpToDate()
{
    if(current_playlist_generation == 0)
    {
        return false;
    }
    
    if(current_playlist_generation == pending_generation)
    {
        return true;
    }
    
    return false;
}

QString MetadataCollection::getItemsRequest(uint32_t session_id)
{
    QString request_string = QString("/containers/%1/items?session-id=%2")
                                    .arg(id)
                                    .arg(session_id);
    if(current_metadata_generation == 0)
    {
        return request_string;
    }
    
    request_string.append(
                            QString("&generation=%1&delta=%2")
                                    .arg(pending_generation)
                                    .arg(current_metadata_generation)
                         );
    return request_string;
}

QString MetadataCollection::getListsRequest(uint32_t session_id)
{
    QString request_string = QString("/containers/%1/lists?session-id=%2")
                                    .arg(id)
                                    .arg(session_id);
    if(current_playlist_generation == 0)
    {
        return request_string;
    }
    
    request_string.append(
                            QString("&generation=%1&delta=%2")
                                    .arg(pending_generation)
                                    .arg(current_playlist_generation)
                         );
    return request_string;
}

void MetadataCollection::addItem(MdcapInput &mdcap_input)
{

    int new_item_id = -1;
    int new_item_type = -1;

    uint new_item_rating = 50;
    uint new_item_last_played = 0;
    uint new_item_play_count = 0;
    uint new_item_year = 0;
    uint new_item_track = 0;
    uint new_item_length = 0;
    
    QString new_item_url;
    QString new_item_title;
    QString new_item_artist;
    QString new_item_album;
    QString new_item_genre;

    bool all_is_well = true;
    while(mdcap_input.size() > 0 && all_is_well)
    {
        char content_code = mdcap_input.peekAtNextCode();
        
        switch(content_code)
        {
            case MarkupCodes::item_type:
                new_item_type = mdcap_input.popItemType();
                break;

            case MarkupCodes::item_id:
                new_item_id = mdcap_input.popItemId();
                break;

            case MarkupCodes::item_rating:
                new_item_rating = (int) mdcap_input.popItemRating();
                break;
                
            case MarkupCodes::item_last_played:
                new_item_last_played = mdcap_input.popItemLastPlayed();
                break;
                
            case MarkupCodes::item_play_count:
                new_item_play_count = mdcap_input.popItemPlayCount();
                break;
                
            case MarkupCodes::item_year:
                new_item_year = mdcap_input.popItemYear();
                break;
                
            case MarkupCodes::item_track:
                new_item_track = mdcap_input.popItemTrack();
                break;
                
            case MarkupCodes::item_length:
                new_item_length = mdcap_input.popItemLength();
                break;
                
            case MarkupCodes::item_url:
                new_item_url = mdcap_input.popItemUrl();
                break;
                
            case MarkupCodes::item_artist:
                new_item_artist = mdcap_input.popItemArtist();
                break;
                
            case MarkupCodes::item_album:
                new_item_album = mdcap_input.popItemAlbum();
                break;
                
            case MarkupCodes::item_title:
                new_item_title = mdcap_input.popItemTitle();
                break;
                
            case MarkupCodes::item_genre:
                new_item_genre = mdcap_input.popItemGenre();
                break;
            
            default:
                cerr << "metadatacollection.o: unknown tag while doing "
                     << "addItem()"
                     << endl;
                all_is_well = false;
        }
    }
    
    //
    //  If we have all the right info, add a metadata item
    //
    
    if(new_item_type != MDT_audio)
    {
        cerr << "I don't do non audio yet" << endl;
        return;
    }
    
    
    
    if(new_item_id > 0)
    {   
        QDateTime last_played;
        last_played.setTime_t(new_item_last_played);
        AudioMetadata *new_audio = new AudioMetadata(
                                                        id,
                                                        new_item_id,
                                                        QUrl(new_item_url),
                                                        new_item_rating,
                                                        last_played,
                                                        new_item_play_count,
                                                        new_item_artist,
                                                        new_item_album,
                                                        new_item_title,
                                                        new_item_genre,
                                                        new_item_year,
                                                        new_item_track,
                                                        new_item_length
                                                    );
                                                    
        metadata.insert(new_item_id, new_audio);
        cout << "Should be adding something to container " << id << " with metadata id of " << new_item_id << endl;
        cout << "Title is " << new_audio->getTitle() << endl;
    }
    
}

void MetadataCollection::addList(MdcapInput &mdcap_input)
{

    int new_list_id = -1;
    QString new_list_name;
    QValueList<int> list_entries;    
    uint32_t a_number;
    
    bool all_is_well = true;

    while(mdcap_input.size() > 0 && all_is_well)
    {
        char content_code = mdcap_input.peekAtNextCode();
        
        switch(content_code)
        {

            case MarkupCodes::list_id:
                new_list_id = mdcap_input.popListId();
                break;

            case MarkupCodes::list_name:
                new_list_name = mdcap_input.popListName();
                break;
                
            case MarkupCodes::list_item:
                a_number = mdcap_input.popListItem();
                list_entries.push_back(a_number);
                break;
                
            default:
                cerr << "metadatacollection.o: unknown tag while doing "
                     << "addList()"
                     << endl;
                all_is_well = false;
        }
    }
    
    //
    //  If we have all the right info, add a metadata list
    //
    

    if(new_list_id > 0)
    {   
        Playlist *new_playlist = new Playlist(
                                                id,
                                                new_list_name,
                                                &list_entries,
                                                new_list_id
                                             );
        playlists.insert(new_list_id, new_playlist);
        cout << "Should be adding something to container " << id << " with playlist id of " << new_list_id << endl;
        cout << "Title is " << new_playlist->getName() << endl;
    }
}

MetadataCollection::~MetadataCollection()
{
    metadata.clear();
    playlists.clear();
}

