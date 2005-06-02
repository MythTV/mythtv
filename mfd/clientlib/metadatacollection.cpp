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
    
    //
    //  Set defaults
    //    
    
    editable = false;
    ripable = false;
    being_ripped = false;
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

void MetadataCollection::beUpToDate()
{
    //
    //  Our parent metadata client seems to think we are current with
    //  everything. Set a few things, and complain if we aren't
    //
    
    if(current_metadata_generation != current_playlist_generation)
    {
        cerr << "metadatacollection.o was told to beUpToDate(), but "
             << "metadatata generation is "
             << current_metadata_generation
             << " while playlist generation is "
             << current_playlist_generation
             << endl; 
    }
    current_combined_generation = current_metadata_generation;
    
    if(current_combined_generation != pending_generation)
    {
        cerr << "metadatacollection.o was told to beUpToDate(), but "
             << "combined generation is "
             << current_combined_generation
             << " while pending generation is "
             << pending_generation
             << endl;
    }
    
    if(expected_count != (int) metadata.count())
    {
        cerr << "metadatacollection.o was told to beUpToDate(), but "
             << "expected count is "
             << expected_count
             << " while the actual amount of metadata is "
             << metadata.count()
             << endl;
    }
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
    bool new_item_dup = false;
    
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
                
            case MarkupCodes::item_dup_flag:
                new_item_dup = mdcap_input.popItemDupFlag();
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
        if(new_item_dup)
        {
            new_audio->markAsDuplicate(true);
        }                                            
        metadata.insert(new_item_id, new_audio);

    }
    
}

void MetadataCollection::addList(MdcapInput &mdcap_input)
{

    int new_list_id = -1;
    bool new_list_editable = false;
    bool new_list_ripable = false;
    bool new_list_being_ripped = false;
    QString new_list_name;
    QValueList<PlaylistEntry> list_entries;
    
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
                
            case MarkupCodes::list_is_editable:
                new_list_editable = mdcap_input.popListEditable();
                break;
                
            case MarkupCodes::list_is_ripable:
                new_list_ripable = mdcap_input.popListRipable();
                break;    
                
            case MarkupCodes::list_being_ripped:
                new_list_being_ripped = mdcap_input.popListBeingRipped();
                break;    
                
            case MarkupCodes::added_list_item_group:
                {
                    QValueVector<char> *list_entry = new QValueVector<char>;
                    mdcap_input.popGroup(list_entry);
                    MdcapInput rebuilt_internals(list_entry);
                    PlaylistEntry new_entry = addListEntry(rebuilt_internals);
                    if(new_entry.getId() > 0)
                    {
                        list_entries.push_back(new_entry);
                    }
                    else
                    {
                        cerr << "metadatacollection.o: got a negative value "
                             << "for an item on a playlist"
                             << endl;
                    }
                    delete list_entry;
                }

                break;
                
            case MarkupCodes::added_list_list_group:
                {
                    QValueVector<char> *list_entry = new QValueVector<char>;
                    mdcap_input.popGroup(list_entry);
                    MdcapInput rebuilt_internals(list_entry);
                    PlaylistEntry new_entry = addListEntry(rebuilt_internals, true);
                    if(new_entry.getId() > 0)
                    {
                        list_entries.push_back(new_entry);
                    }
                    else
                    {
                        cerr << "metadatacollection.o: got a negative value "
                             << "for a (sub) playlist on a playlist"
                             << endl;
                    }
                    delete list_entry;
                    
                }
                break;
                
            default:
                cerr << "metadatacollection.o: unknown tag while doing "
                     << "addList()"
                     << endl;
                all_is_well = false;
        }
    }
    
    //
    //  If we have all the right info, add a metadata list and/or update an existing list
    //
    

    if(new_list_id > 0)
    {   
        //
        //  Does this list exist already?
        //

        if(playlists.find(new_list_id))
        {
            //
            //  Remove the old copy
            //
            
            playlists.remove(new_list_id);

        }

        ClientPlaylist *new_playlist = new ClientPlaylist(
                                                            id,
                                                            new_list_name,
                                                            &list_entries,
                                                            new_list_id
                                                         );
        new_playlist->isEditable(new_list_editable);
        new_playlist->isRipable(new_list_ripable);
        new_playlist->setBeingRipped(new_list_being_ripped);
        playlists.insert(new_list_id, new_playlist);
    }
}

PlaylistEntry MetadataCollection::addListEntry(MdcapInput &mdcap_input, bool is_another_playlist)
{
    QString list_entry_name;
    int     list_entry_id;
    
    bool all_is_well = true;
    while(mdcap_input.size() > 0 && all_is_well) 
    {
        char content_code = mdcap_input.peekAtNextCode();
        if(content_code == MarkupCodes::list_item_name)
        {
            list_entry_name = mdcap_input.popListItemName();
        }
        else if(content_code == MarkupCodes::list_item)
        {
            list_entry_id = mdcap_input.popListItem();
        }
        else
        {
            cerr << "metadatacollection.o: while doing addListEntry(), "
                 << "seeing content codes that should not be there"
                 << endl;
            all_is_well = false;
        }
    }
    
    if(all_is_well)
    {
        return PlaylistEntry(list_entry_id, list_entry_name, is_another_playlist);
    }
    
    return PlaylistEntry();
}

void MetadataCollection::deleteItem(uint which_item)
{
    if(!metadata.remove((int) which_item))
    {
        cerr << "metadatacollection.o: asked to remove metadata with id of "
             << (int) which_item
             << ", but can't find it"
             << endl;
    }
}

void MetadataCollection::deleteList(uint which_list)
{
    if(!playlists.remove((int) which_list))
    {
        cerr << "metadatacollection.o: asked to remove playlist with id of "
             << (int) which_list
             << ", but can't find it"
             << endl;
    }
}

void MetadataCollection::printMetadata()
{
    cout << "\n\tCollection number "
         << id 
         << " has name of \""
         << name
         << "\" and "
         << metadata.count()
         << " items"
         << endl;
         
    cout << "\tPlaylists:" << endl;


    QIntDictIterator<ClientPlaylist> it( playlists );
    for ( ; it.current(); ++it )
    {
        cout << "\t\tPlaylist called \""
             << it.current()->getName()
             << "\" has "
             << it.current()->getCount()
             << " items"
             << endl;
    }
}

ClientPlaylist* MetadataCollection::getPlaylistById(int which_playlist)
{
    return playlists.find(which_playlist);
}

MetadataCollection::~MetadataCollection()
{
    metadata.clear();
    playlists.clear();
}

