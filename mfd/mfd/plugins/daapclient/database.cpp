/*
	database.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	This is nothing to do with SQL or MYSQL A large collection of items
	(songs) and containers (playlists) is called a "database" in DAAP.
	That's what this is.

*/

#include "../../../config.h"

#include <iostream>
using namespace std;
#include <qapplication.h>
#include "database.h"
#include "mfd_events.h"

#include "daapclient.h"

Database::Database(
                    int l_daap_id, 
                    const QString& l_name,
                    int l_expected_numb_items,
                    int l_expected_numb_playlists,
                    MFD *my_mfd,
                    DaapClient *owner,
                    int l_session_id,
                    QString l_host_address,
                    int l_host_port,
                    DaapServerType l_daap_server_type
                  )
{
    //
    //  When I start out, I have no data
    //

    have_items = false;
    have_playlist_list = false;
    have_playlists = false;
    generation_delta = 0;

    //
    //  Set info about me
    //    
    
    session_id = l_session_id;
    daap_id = l_daap_id;
    name = l_name;
    parent= owner;
    expected_numb_items = l_expected_numb_items;
    expected_numb_playlists = l_expected_numb_playlists;
    host_address = l_host_address;
    host_port = l_host_port;
    daap_server_type = l_daap_server_type;

    //
    //  Ask the mfd for the metadata server, then ask the metadata server to
    //  give me a container (where I'll store music and playlist data)
    //

    the_mfd = my_mfd;
    metadata_server = the_mfd->getMetadataServer();
    if(daap_server_type == DAAP_SERVER_ITUNES41)
    {
        metadata_container = metadata_server->createContainer(name, MCCT_audio, MCLT_lan, MST_daap_itunes41);
    }
    else if(daap_server_type == DAAP_SERVER_ITUNES42)
    {
         metadata_container = metadata_server->createContainer(name, MCCT_audio, MCLT_lan, MST_daap_itunes42);
    }
    else if(daap_server_type == DAAP_SERVER_ITUNES43)
    {
         metadata_container = metadata_server->createContainer(name, MCCT_audio, MCLT_lan, MST_daap_itunes43);
    }
    else if(daap_server_type == DAAP_SERVER_ITUNES45)
    {
         metadata_container = metadata_server->createContainer(name, MCCT_audio, MCLT_lan, MST_daap_itunes45);
    }
    else if(daap_server_type == DAAP_SERVER_ITUNES46)
    {
         metadata_container = metadata_server->createContainer(name, MCCT_audio, MCLT_lan, MST_daap_itunes46);
    }
    else if(daap_server_type == DAAP_SERVER_ITUNES47)
    {
         metadata_container = metadata_server->createContainer(name, MCCT_audio, MCLT_lan, MST_daap_itunes47);
    }
    else if(daap_server_type == DAAP_SERVER_ITUNES48)
    {
         metadata_container = metadata_server->createContainer(name, MCCT_audio, MCLT_lan, MST_daap_itunes48);
    }
    else
    {
         metadata_container = metadata_server->createContainer(name, MCCT_audio, MCLT_lan);
    }

    container_id = metadata_container->getIdentifier();

    //
    //  Prepare the metadata
    //
    
    new_metadata = new QIntDict<Metadata>;
    new_metadata->setAutoDelete(true);
    new_playlists = new QIntDict<Playlist>;
    new_metadata->setAutoDelete(true);
    full_data_update = true;
    update_type_set = false;
    
    
    metadata_additions.clear();
    metadata_deletions.clear();
    playlist_additions.clear();
    playlist_deletions.clear();
    previous_metadata.clear();
    previous_playlists.clear();

}

int Database::getFirstPlaylistWithoutList()
{
    int return_value = -42;

    QIntDictIterator<Playlist> it( *new_playlists ); 
    for ( ; it.current(); ++it )
    {
        if(it.current()->waitingForList())
        {
            return_value = it.current()->getId();
            break;
        }
    }
    return return_value;
}

void Database::doDatabaseItemsResponse(TagInput& dmap_data)
{

    Tag a_tag;
    Chunk a_chunk;

    bool database_items_status = false;
    int new_numb_items = -1;
    int new_received_numb_items = -1;

    while(!dmap_data.isFinished())
    {
        //
        //  parse responses to a /database/x/items request
        //

        dmap_data >> a_tag;

        u32 a_u32_variable;
        u8  a_u8_variable;

        switch(a_tag.type)
        {
            case 'mstt':

                //
                //  status of request
                //
                
                dmap_data >> a_u32_variable;
                if(a_u32_variable == 200)    // like HTTP 200 (OK!)
                {
                    database_items_status = true;
                }
                else
                {
                    warning("got bad status for database items");
                }
                break;

            case 'muty':
            
                //
                //  0 - full update
                //  1 - delta
                //
                
                dmap_data >> a_u8_variable;
                checkUpdate(a_u8_variable);
                break;
                
            case 'mtco':
                
                //
                //  number of total items
                //
                
                dmap_data >> a_u32_variable;
                new_numb_items = a_u32_variable;
                //checkUpdateType(new_numb_items, new_received_numb_items);
                break;
                
            case 'mrco':
            
                //
                //  received number of items
                //                
                
                dmap_data >> a_u32_variable;
                new_received_numb_items = a_u32_variable;
                //checkUpdateType(new_numb_items, new_received_numb_items);
                break;
                
            case 'mlcl':

                //
                //  This is "listing" tag, saying there's a list of other tags to come
                //
                
                dmap_data >> a_chunk;
                {
                    TagInput re_rebuilt_internal(a_chunk);
                    parseItems(re_rebuilt_internal, new_received_numb_items);
                }
                break;

            case 'mudl':

                //
                //  This is "listing" tag, with a list of things to delete since the previous 
                //
                
                dmap_data >> a_chunk;
                {
                    TagInput re_rebuilt_internal(a_chunk);
                    parseDeletions(re_rebuilt_internal);
                }
                break;

            default:
                warning("got an unknown tag type "
                        "while doing doDatabaseItemsResponse()");
                dmap_data >> a_chunk;
        }

        dmap_data >> end;

    }

    have_items = true;    
}


/*
void Database::checkUpdateType(int new_numb_items, int new_received_numb_items)
{
    if(new_numb_items == -1 || new_received_numb_items == -1)
    {
        return;
    }

    if(new_numb_items == new_received_numb_items)
    {
        if(!full_data_update)
        {
            warning("getting all items in delta update mode");
        }
    }
    else
    {
        if(full_data_update)
        {
            warning("getting partial items in full update mode");
        }
    }
}    
*/

void Database::checkUpdate(u8 update_type)
{
    if(!update_type_set)
    {
        update_type_set = true;
        if(update_type == 0)
        {
            //
            //  Full update
            //

            log("receiving a complete data update from daap server", 9);
            full_data_update = true;
            metadata_additions.clear();
            metadata_deletions.clear();
            playlist_additions.clear();
            playlist_deletions.clear();
        }
        else if(update_type == 1)
        {
            log("receiving a partial (delta) update from daap server", 9);
            full_data_update = false;
            metadata_additions.clear();
            metadata_deletions.clear();
            playlist_additions.clear();
            playlist_deletions.clear();
        }
        else
        {
            warning("unknown update type");
        }
    }
}    

void Database::parseItems(TagInput& dmap_data, int how_many_now)
{

    Tag a_tag;

    u8  a_u8_variable;
    u16 a_u16_variable;
    u32 a_u32_variable;
    u64 a_u64_variable;
    
    std::string a_string;
    Chunk listing;

    QTime loading_time;
    loading_time.start();    
    
    for(int i = 0; i < how_many_now; i++)
    {
        Chunk emergency_throwaway_chunk;

        dmap_data >> a_tag >> listing >> end;
    
        if(a_tag.type != 'mlit')
        {
            //
            //  this should not happen
            //
            warning("got a non mlit tag "
                    "where one simply must be.");
        }
        
        int     new_item_id = -1;
        int     new_item_persistent_id = 1;
        QString new_item_name = "";
        QString new_item_album_name = "";
        QString new_item_artist_name = "";
        int     new_item_bpm = -1;
        int     new_item_bitrate = -1;
        QString new_item_comment = "";
        bool    new_item_compilation = false;
        QString new_item_composer_name = "";
        int     new_item_date_added = 0;   //  seconds since epoch
        int     new_item_date_modified = 0;
        int     new_item_disc_count = -1;
        int     new_item_disc_number = -1;
        int     new_item_disabled = -1;
        QString new_item_eqpreset = "";
        QString new_item_format = "";
        QString new_item_genre = "";
        QString new_item_description = "";
        int     new_item_relative_volume = 0;
        int     new_item_sample_rate = -1;
        int     new_item_size = -1;
        int     new_item_start_time = -1;
        int     new_item_stop_time = -1;
        int     new_item_total_time = -1;
        int     new_item_track_count = -1;
        int     new_item_track_number = -1;
        int     new_item_rating = -1;
        int     new_item_year = -1;
        QString new_item_url = "";  // not daap url, url for extra info ?
        QString new_item_myth_digest = "";
        
        TagInput internal_listing(listing);
        while(!internal_listing.isFinished())
        {
    
            internal_listing >> a_tag;
        
            switch(a_tag.type)
            {
                //
                //  This ain't pretty, and there are a lot of them, but it
                //  works (and, suprisingly, it's not aching slow)
                //

                case 'mikd':
                
                    //
                    //  item kind ... no idea what that means
                    //

                    internal_listing >> a_u8_variable;
                    break;

                case 'asdk':
            
                    //
                    //  data kind ... never seen anything but 0
                    //
                
                    internal_listing >> a_u8_variable;
                    break;
                
                case 'miid':
            
                    //
                    //  item id !
                    //
                
                    internal_listing >> a_u32_variable;
                    new_item_id = a_u32_variable;
                    break;
                    
                case 'mper':
                
                    internal_listing >> a_u64_variable;
                    new_item_persistent_id = a_u64_variable;
                    break;

                case 'minm':
            
                    //
                    //   Item name
                    //
                
                    internal_listing >> a_string;
                    new_item_name = QString::fromUtf8(a_string.c_str());
                    break;
                
                case 'mypi':
                
                    //
                    //  Myth persistent id
                    //
                    
                    internal_listing >> a_string;
                    new_item_myth_digest = QString::fromUtf8(a_string.c_str());
                    break;
                
                case 'asal':
                
                    //
                    //  Album name
                    //
                    
                    internal_listing >> a_string;
                    new_item_album_name = QString::fromUtf8(a_string.c_str());
                    break;
                
                case 'asar':
                
                    //
                    //  Artist name
                    //
                    
                    internal_listing >> a_string;
                    new_item_artist_name = QString::fromUtf8(a_string.c_str());
                    break;
                    
                case 'asbt':
                
                    //
                    //  Beats per minute
                    //
                    
                    internal_listing >> a_u16_variable;
                    new_item_bpm = a_u16_variable;
                    break;
                    
                case 'asbr':
                
                    //
                    //  Bitrate (probably not used to calculate anything)
                    //    
                    
                    internal_listing >> a_u16_variable;
                    new_item_bitrate = a_u16_variable;
                    break;
                    
                case 'ascm':
                
                    //
                    //  Comment
                    //
                    
                    internal_listing >> a_string;
                    new_item_comment = QString::fromUtf8(a_string.c_str());
                    break;
                    
                case 'asco':
                
                    //
                    //  Part of a compilation (this is probably a bool flag)
                    //
                    
                    internal_listing >> a_u8_variable;
                    new_item_compilation = (bool) a_u8_variable;
                    break;
                
                case 'ascp':
                
                    //
                    //  Composer's name
                    //
                    
                    internal_listing >> a_string;
                    new_item_composer_name = QString::fromUtf8(a_string.c_str());
                    break;
                    
                case 'asda':
                
                    //
                    //  Date added
                    //
                    
                    internal_listing >> a_u32_variable;
                    new_item_date_added = a_u32_variable;
                    break;
                    
                case 'asdm':
                
                    //
                    //  Date modified
                    //
                    
                    internal_listing >> a_u32_variable;
                    new_item_date_modified = a_u32_variable;
                    break;
                    
                case 'asdc':
                
                    //
                    //  Number of discs in the collection this item came
                    //  from
                    //   
                    
                    internal_listing >> a_u16_variable;
                    new_item_disc_count = a_u16_variable;
                    break;
                    
                case 'asdn':
                
                    //
                    //  The disc number (out of disc count) the item is/was
                    //  on
                    //
                
                    internal_listing >> a_u16_variable;
                    new_item_disc_number = a_u16_variable;
                    break;
                    
                case 'asdb':
                
                    //
                    //  Song is disabled (ie. not checked for playing in
                    //  iTunes)
                    //

                    internal_listing >> a_u8_variable;
                    new_item_disabled = a_u8_variable;
                    break;
                    
                case 'aseq':
                
                    //
                    //  Song eq preset (e.g. "Bass Booster", "Spoken Word")
                    //
                    
                    internal_listing >> a_string;
                    new_item_eqpreset = QString::fromUtf8(a_string.c_str());
                    break;
                
                case 'asfm':
                
                    //
                    //  Song format (wav, ogg, etc.)
                    //
                    
                    internal_listing >> a_string;
                    new_item_format = QString::fromUtf8(a_string.c_str());
                    break;
                    
                case 'asgn':

                    //
                    //  Genre
                    //      
                    
                    internal_listing >> a_string;
                    new_item_genre = QString::fromUtf8(a_string.c_str());
                    break;

                case 'asdt':
                
                    //
                    //  Description
                    //
                    
                    internal_listing >> a_string;
                    new_item_description = QString::fromUtf8(a_string.c_str());
                    break; 
                    
                case 'asrv':
                
                    //
                    //  relative volume (probably -100 to +100, or 0-256 ?)
                    //
                    
                    internal_listing >> a_u8_variable;
                    new_item_relative_volume = a_u8_variable;
                    break;
                    
                case 'assr':
                
                    //
                    //  Sample rate (44.1, etc.)
                    //
                    
                    internal_listing >> a_u32_variable;
                    new_item_sample_rate = a_u32_variable;
                    break;

                case 'assz':
                
                    //
                    //  Song size (bytes ?)
                    //
                    
                    internal_listing >> a_u32_variable;
                    new_item_size = a_u32_variable;
                    break;
                    
                case 'asst':
                
                    //
                    //  Start time (I guess you could set to always skip
                    //  over something)
                    //
                    
                    internal_listing >> a_u32_variable;
                    new_item_start_time = a_u32_variable;
                    break;
                    
                case 'assp':
                
                    //
                    //  Stop time
                    //
                    
                    internal_listing >> a_u32_variable;
                    new_item_stop_time = a_u32_variable;
                    break;
                    
                case 'astm':
                
                    //
                    //  Total time
                    //
                    
                    internal_listing >> a_u32_variable;
                    new_item_total_time = a_u32_variable;
                    break;
                    
                    
                case 'astc':
                
                    //
                    //  Track count (how many in total)
                    //
                    
                    internal_listing >> a_u16_variable;
                    new_item_track_count = a_u16_variable;
                    break;
                    
                case 'astn':
                
                    //
                    //  Track number
                    //
                    
                    internal_listing >> a_u16_variable;
                    new_item_track_number = a_u16_variable;
                    break;
                    
                case 'asur':
                
                    //
                    //  User rating (0 to 100)
                    //
                    
                    internal_listing >> a_u8_variable;
                    new_item_rating = a_u8_variable;
                    break;

                case 'asyr':
                
                    //
                    //  Year song was released (?)
                    //
                    
                    internal_listing >> a_u16_variable;
                    new_item_year = a_u16_variable;
                    break;

                case 'asul':
                
                    //
                    //  A url for the song (link to band home page? I have
                    //  no idea)
                    //
                    
                    internal_listing >> a_string;
                    new_item_url = QString::fromUtf8(a_string.c_str());
                    break;

                default:
                    
                    warning("unknown tag while parsing database item");
                    internal_listing >> emergency_throwaway_chunk;
            }
            internal_listing >> end;
        }
        
        //
        //  If we have enough data, make a Metadata object
        //

        if(
                new_item_format == "ogg"  ||
                new_item_format == "wav"  ||
                new_item_format == "flac" ||
                new_item_format == "fla"  ||
#ifdef AAC_AUDIO_SUPPORT
                new_item_format == "m4a"  ||
#endif

                new_item_format == "mp3")
        {
            if(new_item_id > -1 && new_item_name.length() > 0)
            {
                //
                //  Make new metadata with a rather special URL (instead of a
                //  filename)
                //  
            
                QString new_url = QString("daap://%1:%2/databases/%3/items/%4.%5?session-id=%6")
                                  .arg(host_address)
                                  .arg(host_port)
                                  .arg(daap_id)
                                  .arg(new_item_id)
                                  .arg(new_item_format)
                                  .arg(session_id);
                                  
                AudioMetadata *new_item = new AudioMetadata
                                    (
                                        new_url,
                                        new_item_artist_name,
                                        new_item_album_name,
                                        new_item_name,
                                        new_item_genre,
                                        new_item_year,
                                        new_item_track_number,
                                        new_item_total_time
                                    );

                //
                //  Add in whatever else the daap server told us about
                //            
            
                new_item->setCollectionId(container_id);
                new_item->setId(new_item_id);

                QDateTime when;
                when.setTime_t(new_item_date_added);
                new_item->setDateAdded(when);
            
                when.setTime_t(new_item_date_modified);
                new_item->setDateModified(when);

                new_item->setBpm(new_item_bpm);
                new_item->setBitrate(new_item_bitrate);
                new_item->setComment(new_item_comment);
                new_item->setCompilation(new_item_compilation);
                new_item->setComposer(new_item_composer_name);
                new_item->setDiscCount(new_item_disc_count);            
                new_item->setDiscNumber(new_item_disc_number);
                new_item->setEqPreset(new_item_eqpreset);
                new_item->setFormat(new_item_format);
                new_item->setDescription(new_item_description);
                new_item->setRelativeVolume(new_item_relative_volume);
                new_item->setSampleRate(new_item_sample_rate);
                new_item->setSize(new_item_size);
                new_item->setStartTime(new_item_start_time);
                new_item->setStopTime(new_item_stop_time);
                new_item->setTrackCount(new_item_track_count);
                new_item->setRating(new_item_rating);
                if(new_item_myth_digest.length() > 0)
                {
                    new_item->setMythDigest(new_item_myth_digest);
                }
            
                new_metadata->insert(new_item->getId(), new_item);

            }
            else
            {
                warning("got incomplete data for an item, going to ignore it");
            }        
        }
        else
        {
            log(QString("got an unsupported music format (%1), ignoring item")
                        .arg(new_item_format), 5);
                
        }
    }
    
    double elapsed_time = loading_time.elapsed() / 1000.0;
    log(QString("parsed and loaded metadata for %1 "
                "items in %2 seconds")
                .arg(how_many_now)
                .arg(elapsed_time), 4);
}


void Database::parseDeletions(TagInput& dmap_data)
{
    if(full_data_update)
    {
        warning("got deltas (deletions), but "
                "am in full data mode (?)");
    }

    while(!dmap_data.isFinished())
    {
        u32 a_u32_variable;

        Tag a_tag;
        Chunk emergency_throwaway_chunk;
        dmap_data >> a_tag;
        
        switch(a_tag.type)
        {
            //
            //  Just store a list of deletions
            //

            case 'miid':
                
                    //
                    //  item id, the thing that has been deleted
                    //

                    dmap_data >> a_u32_variable;
                    metadata_deletions.push_back(a_u32_variable);
                    break;

            default:
                    
                warning("unknown tag while parsing database item deletion");
                dmap_data >> emergency_throwaway_chunk;
        }
        dmap_data >> end;
    }
}

void Database::doDatabaseListPlaylistsResponse(TagInput &dmap_data, int new_generation)
{
    //
    //  Figure out how many playlists there are and instantiate an object
    //  for each.
    //

    Tag a_tag;
    Chunk a_chunk;

    bool database_playlist_list_status = false;
    int new_numb_playlists = -1;
    int new_numb_received_playlists = -1;

    while(!dmap_data.isFinished())
    {
        //
        //  parse responses to a /database/x/containers request
        //

        dmap_data >> a_tag;

        u32 a_u32_variable;
        u8  a_u8_variable;

        switch(a_tag.type)
        {
            case 'mstt':

                //
                //  status of request
                //
                
                dmap_data >> a_u32_variable;
                if(a_u32_variable == 200)    // like HTTP 200 (OK!)
                {
                    database_playlist_list_status = true;
                }
                else
                {
                    warning("got bad status for playlists list");
                }
                break;

            case 'muty':
            
                //
                //  Update type
                //  0 = full
                //  1 = partial
                //
                
                dmap_data >> a_u8_variable;
                checkUpdate(a_u8_variable);
                break;
                
            case 'mtco':
                
                //
                //  number of total playlists
                //
                
                dmap_data >> a_u32_variable;
                new_numb_playlists = a_u32_variable;
                break;
                
            case 'mrco':
            
                //
                //  received number of items
                //                
                
                dmap_data >> a_u32_variable;
                new_numb_received_playlists = a_u32_variable;
                break;
                
            case 'mlcl':

                //
                //  This is "listing" tag, saying there's a list of other tags to come
                //
                
                dmap_data >> a_chunk;
                {
                    TagInput re_rebuilt_internal(a_chunk);
                    parseContainers(re_rebuilt_internal, new_numb_received_playlists);
                }
                break;
            
            case 'mudl':
            
                //
                //  This is listings tag saying there's a list of deletions to come
                //
                
                dmap_data >> a_chunk;
                {
                    TagInput re_rebuilt_internal(a_chunk);
                    parseDeletedContainers(re_rebuilt_internal, new_generation);
                }
                break;

            default:
                warning("got an unknown tag type "
                        "while doing doDatabaseListPlaylistsResponse()");
                dmap_data >> a_chunk;
        }

        dmap_data >> end;

    }

    have_playlist_list = true;
    if(new_numb_playlists == 0)
    {
        //
        //  There are no containers (playlists) to get
        //
        
        have_playlists = true;
        doTheMetadataSwap(new_generation);
    }
}

void Database::parseContainers(TagInput& dmap_data, int how_many)
{

    Tag a_tag;

    u8  a_u8_variable;
    u32 a_u32_variable;
    std::string a_string;
    Chunk listing;

    QTime loading_time;
    loading_time.start();    
    
    for(int i = 0; i < how_many; i++)
    {
        Chunk emergency_throwaway_chunk;

        dmap_data >> a_tag >> listing >> end;
    
        if(a_tag.type != 'mlit')
        {
            //
            //  this should not happen
            //
            warning("got a non mlit tag "
                    "that I really really wanted.");
        }
        
        int     new_playlist_id = -1;
        QString new_playlist_name = "";
        int     new_playlist_expected_count = 1;
        
        TagInput internal_listing(listing);
        while(!internal_listing.isFinished())
        {
    
            internal_listing >> a_tag;
        
            switch(a_tag.type)
            {

                case 'miid':
            
                    //
                    //  playlist id !
                    //
                
                    internal_listing >> a_u32_variable;
                    new_playlist_id = a_u32_variable;
                    break;
                    
                case 'minm':
            
                    //
                    //   Item name
                    //
                
                    internal_listing >> a_string;
                    new_playlist_name = QString::fromUtf8(a_string.c_str());
                    break;
                
                case 'mimc':
                
                    //
                    //  Count of playlist
                    //
                    
                    internal_listing >> a_u32_variable;
                    new_playlist_expected_count = a_u32_variable;
                    break;

                case 'abpl':
                    
                    //
                    //  Base playlist (have no idea what this means, it's a
                    //  single byte, so it may be a flag saying "this is a
                    //  base playlist"?)
                    //
                    
                    internal_listing >> a_u8_variable;
                    break;
                    
                default:
                    
                    warning("unknown tag while parsing "
                            "database playlist container");
                            
                    internal_listing >> emergency_throwaway_chunk;
            }
            internal_listing >> end;
        }
        
        //
        //  If we have enough data, make a Metadata object
        //


        if(new_playlist_id > -1 && new_playlist_name.length() > 0)
        {

            //
            //  Make a playlist
            //
        
            Playlist *new_playlist = new Playlist(
                                                    container_id,
                                                    new_playlist_name,
                                                    "",
                                                    new_playlist_id
                                                 );
            new_playlist->waitingForList(true);
            new_playlists->insert(new_playlist->getId(), new_playlist);
            
        }
        else
        {
            warning("got incomplete data for a playlist, "
                    "going to try to ignore it");
        }        
        
    }
    
    double elapsed_time = loading_time.elapsed() / 1000.0;
    log(QString("parsed and loaded metadata for %1 "
                "containers (playlists) in %2 seconds")
                .arg(how_many)
                .arg(elapsed_time), 4);


}

void Database::parseDeletedContainers(TagInput& dmap_data, int new_generation)
{

    if(full_data_update)
    {
        log("got deltas (deletions) for playlists/containers, but "
                "am in full data mode, will deal with it", 10);
    }

    while(!dmap_data.isFinished())
    {
        u32 a_u32_variable;

        Tag a_tag;
        Chunk emergency_throwaway_chunk;
        dmap_data >> a_tag;
        
        switch(a_tag.type)
        {
            //
            //  Just store a list of deletions
            //

            case 'miid':
                
                    //
                    //  item id, the thing that has been deleted
                    //

                    dmap_data >> a_u32_variable;
                    playlist_deletions.push_back(a_u32_variable);
                    break;

            default:
                    
                warning("unknown tag while parsing database "
                        "playlist/container deletion");
                dmap_data >> emergency_throwaway_chunk;
        }
        dmap_data >> end;
    }

    //
    //  Check and see if all playlists are filled
    //
    

    bool all_filled = true;

    QIntDictIterator<Playlist> it( *new_playlists ); 
    for ( ; it.current(); ++it )
    {
        if(it.current()->waitingForList())
        {
            all_filled = false;
            break;
        }
    }

    if(all_filled)
    {
        have_playlists = true;
        doTheMetadataSwap(new_generation);        
    }


}

void Database::doDatabasePlaylistResponse(TagInput &dmap_data, int which_playlist, int new_generation)
{
    //
    //  First, find the playlist
    //
    
    Playlist *the_playlist = new_playlists->find(which_playlist);
    if(!the_playlist)
    {
        warning("can't do doDatabasePlaylistResponse() "
                "with bad playlist id");
        return;
    }

    //
    //  Put the tracks in the playlist
    //

    Tag a_tag;
    Chunk a_chunk;

    bool list_status = false;
    int  numb_list_items = 0;
    int  numb_received_list_items;
    int  update_type = -1;

    while(!dmap_data.isFinished())
    {
        //
        //  parse responses to a /database/x/containers request
        //

        dmap_data >> a_tag;

        u32 a_u32_variable;
        u8  a_u8_variable;

        switch(a_tag.type)
        {
            case 'mstt':

                //
                //  status of request
                //
                
                dmap_data >> a_u32_variable;
                if(a_u32_variable == 200)    // like HTTP 200 (OK!)
                {
                    list_status = true;
                }
                else
                {
                    warning("got bad status for playlist");
                }
                break;

            case 'muty':
            
                //
                //  0 - full update
                //  1 - partial update
                //
                
                dmap_data >> a_u8_variable;
                update_type = (int) a_u8_variable;
                checkUpdate(a_u8_variable);
                break;
                
            case 'mtco':
                
                //
                //  number of items on the list
                //
                
                dmap_data >> a_u32_variable;
                numb_list_items = a_u32_variable;
                break;
                
            case 'mrco':
            
                //
                //  received number of items
                //                
                
                dmap_data >> a_u32_variable;
                numb_received_list_items = a_u32_variable;
                break;
                
            case 'mlcl':

                //
                //  This is "listing" tag, saying there's a list of other tags to come
                //
                
                dmap_data >> a_chunk;
                {
                    TagInput re_rebuilt_internal(a_chunk);
                    parsePlaylist(re_rebuilt_internal, numb_received_list_items, the_playlist, update_type);
                }
                break;
                
            case 'mudl':
            
                //
                //  list of individual items removed from a playlist (delta)
                //

                dmap_data >> a_chunk;
                {
                    TagInput re_rebuilt_internal(a_chunk);
                    parsePlaylistItemDeletions(re_rebuilt_internal, the_playlist, update_type);
                }
                break;
                

            default:
                warning("got an unknown tag type "
                        "while doing doDatabasePlaylistResponse()");
                dmap_data >> a_chunk;
        }

        dmap_data >> end;

    }

    //
    //  Mark this playlist as "filled"
    //


    the_playlist->waitingForList(false);

    //
    //  Check and see if all playlists are filled
    //
    
    bool all_filled = true;

    QIntDictIterator<Playlist> it( *new_playlists ); 
    for ( ; it.current(); ++it )
    {
        if(it.current()->waitingForList())
        {
            all_filled = false;
            break;
        }
    }

    if(all_filled)
    {
        have_playlists = true;
        doTheMetadataSwap(new_generation);        
    }
    
}

void Database::parsePlaylist(TagInput &dmap_data, int how_many, Playlist *which_playlist, int update_type)
{
    if(update_type == 1)
    {
        //
        //  This happens occassionaly and under mysterious circumstances
        //  from iTunes. It's sending us a list of songs, but assumes they
        //  are in addition to what is already on the playlist. That means
        //  we have to prefill the playlist with what was on there before.
        //
        
        metadata_server->lockMetadata();
            Playlist *existing_playlist = metadata_server->getPlaylistByContainerAndId(container_id, which_playlist->getId());
            if(existing_playlist)
            {
                QValueList<int> existing_tracks = existing_playlist->getList();
                QValueList<int>::iterator it;
                for(it = existing_tracks.begin(); it != existing_tracks.end(); ++it)
                {
                    which_playlist->addToList((*it));
                }
            }
        metadata_server->unlockMetadata();
        
        
    }
    
    Tag a_tag;

    u8  a_u8_variable;
    u32 a_u32_variable;
    std::string a_string;


    Chunk listing;
    QString new_name;

    for(int i = 0; i < how_many; i++)
    {
        Chunk emergency_throwaway_chunk;

        dmap_data >> a_tag >> listing >> end;
    
        if(a_tag.type != 'mlit')
        {
            //
            //  this should not happen
            //
            warning("got a non mlit tag "
                    "and I really really wanted one.");
        }

        u32 items_real_id = 0;
        u32 items_playlist_id = 0;
        
        TagInput internal_listing(listing);
        while(!internal_listing.isFinished())
        {
    
            internal_listing >> a_tag;


            switch(a_tag.type)
            {

                case 'mikd':
            
                    //
                    //  item kind
                    //
                
                    internal_listing >> a_u8_variable;
                    if(a_u8_variable != 2)
                    {
                        warning("got an item kind in a playlist "
                                "list that I don't understand");
                    }
                    break;
                    
                case 'miid':
            
                    //
                    //   Item id
                    //
                
                    internal_listing >> a_u32_variable;
                    which_playlist->addToList(a_u32_variable);
                    items_real_id = a_u32_variable;
                    break;
                
                case 'mcti':
                
                    //
                    //  Another id ... ignore it
                    //
                    
                    internal_listing >> a_u32_variable;
                    items_playlist_id = a_u32_variable;
                    break;
                    
                case 'minm':
                
                    //
                    //  Why are we getting names here ?
                    //  We already know everything about this song. 
                    //  Stupid iTunes
                    //
                    
                    internal_listing >> a_string;
                    break;
                
                case 'asdk':
                
                    //
                    //  Why are we getting a data type here ?
                    //  Silly iTunes
                    //
                    
                    internal_listing >> a_u8_variable;
                    break;
                
                default:
                    
                    warning("unknown tag while parsing "
                            "database playlist");
                    internal_listing >> emergency_throwaway_chunk;
            }

            internal_listing >> end;
        }
        
        which_playlist->addToIndirectMap(items_playlist_id, items_real_id);
    }
}

void Database::parsePlaylistItemDeletions(TagInput &dmap_data, Playlist *which_playlist, int update_type)
{


    if(update_type != 1)
    {
        warning("deleting from a non delta update make no sense");
    }


    //
    //  Lock the metadata and get a reference to the existing playlist up in
    //  the metadata server
    //

    metadata_server->lockMetadata();

    Playlist *existing_playlist = metadata_server->getPlaylistByContainerAndId(container_id, which_playlist->getId());
    if(!existing_playlist)
    {
        warning("can't delete items from a playlist that doesn't yet exist");
        metadata_server->unlockMetadata();
        return;
    }



/*
    metadata_server->lockMetadata();
        Playlist *existing_playlist = metadata_server->getPlaylistByContainerAndId(container_id, which_playlist->getId());
        if(existing_playlist)
        {
            QValueList<int> existing_tracks = existing_playlist->getList();
            QValueList<int>::iterator it;
            for(it = existing_tracks.begin(); it != existing_tracks.end(); ++it)
            {
                which_playlist->addToList((*it));
            }
                
            QMap <int, int> *stupid_itunes_indirection = existing_playlist->getIndirectMap();
            QMap<int, int>::Iterator map_it;
            for ( map_it = stupid_itunes_indirection->begin(); map_it != stupid_itunes_indirection->end(); ++map_it)
            {
                which_playlist->addToIndirectMap(map_it.key(), map_it.data());
            } 
        }
        else
        {
            warning("you want deletions from something that doesn't exist");
            metadata_server->unlockMetadata();
            return;
        }
            
    metadata_server->unlockMetadata();
*/


    while(!dmap_data.isFinished())
    {
    
        u32 a_u32_variable;

        Tag a_tag;
        Chunk emergency_throwaway_chunk;
        dmap_data >> a_tag;
        
        switch(a_tag.type)
        {
            //
            //  Just parse a list of deletions
            //

            case 'miid':
                
                    //
                    //  item id, the thing that has been deleted
                    //

                    dmap_data >> a_u32_variable;
                    {
                        int real_item_id = existing_playlist->getFromIndirectMap(a_u32_variable);
                        if(!which_playlist->removeFromList(real_item_id))
                        {
                            warning(QString("tried to delete an item from a "
                                    "playlist that did not contain the item "
                                    "(item #%1, indirect #%2, playlist #%3)")
                                            .arg(real_item_id) 
                                            .arg(a_u32_variable)
                                            .arg(which_playlist->getId()));
                        }
                    }
                    break;

            default:
                    
                warning("unknown tag while parsing playlist item deletion");
                dmap_data >> emergency_throwaway_chunk;
        }
        dmap_data >> end;
    }

    metadata_server->unlockMetadata();

}

void Database::doTheMetadataSwap(int new_generation)
{
    //
    //  This gets called when everything is up to date.  (ie. we've made all
    //  the /database/whatever calls, and have got all the way to sitting in
    //  a hanging update)
    //

    if(full_data_update)
    {

        //
        //  find new metadata (additions)
        //

        QIntDictIterator<Metadata> iter(*new_metadata);
        for (; iter.current(); ++iter)
        {
            int an_integer = iter.currentKey();
            if(previous_metadata.find(an_integer) == previous_metadata.end())
            {
                metadata_additions.push_back(an_integer);
            }
        }

        //
        //  find old metadata (deletions)
        //

        QValueList<int>::iterator other_iter;
        for ( other_iter = previous_metadata.begin(); other_iter != previous_metadata.end(); ++other_iter )
        {
            int an_integer = (*other_iter);
            if(!new_metadata->find(an_integer))
            {
                metadata_deletions.push_back(an_integer);
            }
        }


        //
        //  find new playlists (additions)
        //

        playlist_additions.clear();
        QIntDictIterator<Playlist> apl_iter(*new_playlists);
        for (; apl_iter.current(); ++apl_iter)
        {
            int an_integer = apl_iter.currentKey();
            if(previous_playlists.find(an_integer) == previous_playlists.end())
            {
                playlist_additions.push_back(an_integer);
            }
        }

        //
        //  find old playlists (deletions)
        //

        playlist_deletions.clear();
        QValueList<int>::iterator ysom_iter;
        for ( ysom_iter = previous_playlists.begin(); ysom_iter != previous_playlists.end(); ++ysom_iter )
        {
            int an_integer = (*ysom_iter);
            if(!new_playlists->find(an_integer))
            {
                playlist_deletions.push_back(an_integer);
            }
        }

        //
        //  Make copies for next time through
        //    
    
        previous_metadata.clear();
        QIntDictIterator<Metadata> yano_iter(*new_metadata);
        for (; yano_iter.current(); ++yano_iter)
        {
            int an_integer = yano_iter.currentKey();
            previous_metadata.push_back(an_integer);
        }
            
    
        previous_playlists.clear();
        QIntDictIterator<Playlist> syano_iter(*new_playlists);
        for (; syano_iter.current(); ++syano_iter)
        {
            int an_integer = syano_iter.currentKey();
            previous_playlists.push_back(an_integer);
        }

        metadata_server->doAtomicDataSwap(
                                            metadata_container, 
                                                  new_metadata, 
                                            metadata_additions,
                                            metadata_deletions,
                                                 new_playlists,
                                            playlist_additions,
                                            playlist_deletions
                                         );
        
        //
        //  The swap (above) returns these NULL in any case, but can't hurt
        //  to be redundant
        //

        new_metadata = NULL;
        new_playlists = NULL;
    }
    else    //  this was a partial update from the daap server
    {

        //
        //  Additions _are_ the metadata
        //

        QIntDictIterator<Metadata> iter(*new_metadata);
        for (; iter.current(); ++iter)
        {
            int an_integer = iter.currentKey();
            metadata_additions.push_back(an_integer);
        }

        //
        //  Deletions are already set
        //

        

        //
        //  find new playlists (additions)
        //

        QIntDictIterator<Playlist> an_iter(*new_playlists);
        for (; an_iter.current(); ++an_iter)
        {
            int an_integer = an_iter.currentKey();
            playlist_additions.push_back(an_integer);
        }

        //
        //  Deletions are already set
        //


        //
        //  Try and keep the previous stuff up to date (add new metadata,
        //  and all playlists, since pl don't delta)
        //    
    
        QIntDictIterator<Metadata> yano_iter(*new_metadata);
        for (; yano_iter.current(); ++yano_iter)
        {
            int an_integer = yano_iter.currentKey();
            previous_metadata.push_back(an_integer);
        }
            
    
        //previous_playlists.clear();
        QIntDictIterator<Playlist> syano_iter(*new_playlists);
        for (; syano_iter.current(); ++syano_iter)
        {
            int an_integer = syano_iter.currentKey();
            previous_playlists.push_back(an_integer);
        }

        metadata_server->doAtomicDataDelta(
                                            metadata_container, 
                                                  new_metadata, 
                                            metadata_additions,
                                            metadata_deletions,
                                                 new_playlists,
                                            playlist_additions,
                                            playlist_deletions
                                         );

        //
        //  The delta (above) returns these NULL already
        //

        new_metadata = NULL;
        new_playlists = NULL;


    }
    

    //
    //  The container now owns the metadata and playlists. We need some new
    //  ones (in case we get an /update response)
    //
    
    new_metadata = new QIntDict<Metadata>;
    new_metadata->setAutoDelete(true);
    new_playlists = new QIntDict<Playlist>;
    new_playlists->setAutoDelete(true);
    
    generation_delta = new_generation;
    update_type_set = false;


    //
    //  Something changed. Fire off an event (this will tell the
    //  mfd to tell all the plugins (that care) that it's time
    //  to update).
    //
                
    MetadataChangeEvent *mce = new MetadataChangeEvent(container_id);
    QApplication::postEvent(the_mfd, mce);    
}

void Database::beIgnorant()
{
    //
    //  When our parent (daapinstance) gets a hanging update request, we
    //  have to say that we are not up to speed on the latest generation of
    //  data.
    //
    
    have_items = false;
    have_playlist_list = false;
    have_playlists = false;
    update_type_set = false;
    
    //
    //  We also need to get rid of any metadata and playlists if we have any 
    //
    
    
    
    if(new_playlists)
    {
        new_playlists->setAutoDelete(true);
        new_playlists->clear();
    }
    
    if(new_metadata)
    {
        new_metadata->setAutoDelete(true);
        new_metadata->clear();
    }
}

void Database::log(const QString &log_message, int verbosity)
{
    parent->log(log_message, verbosity);
}

void Database::warning(const QString &warning_message)
{
    parent->warning(warning_message);
}

Database::~Database()
{

    //
    //  However I got wiped out, I need to tell the mfd any
    //  metadata/playlists I own no longer exist
    //
            
    metadata_server->deleteContainer(container_id);    
    
    if(new_metadata)
    {
        delete new_metadata;
        new_metadata = NULL;
    }
    if(new_playlists)
    {
        delete new_playlists;
        new_playlists = NULL;
    }
}

