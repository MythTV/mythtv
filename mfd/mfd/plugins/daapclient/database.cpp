/*
	database.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	This is nothing to do with SQL or MYSQL A large collection of items
	(songs) and containers (playlists) is called a "database" in DAAP.
	That's what this is.

*/

#include <iostream>
using namespace std;

#include "database.h"

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
                    int l_host_port
                  )
{
    //
    //  When I start out, I have no data
    //

    have_items = false;
    have_playlist_list = false;
    have_playlists = false;

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

    //
    //  Ask the mfd for the metadata server, then ask the metadata server to
    //  give me a container (where I'll store music and playlist data)
    //

    the_mfd = my_mfd;
    metadata_server = the_mfd->getMetadataServer();
    metadata_container = metadata_server->createContainer(MCCT_audio, MCLT_lan);
    container_id = metadata_container->getIdentifier();
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
                break;

            case 'muty':
            
                //
                //  update type ... only ever seen 0 here ... dunno ?
                //
                
                dmap_data >> a_u8_variable;
                break;
                
            case 'mtco':
                
                //
                //  number of total items
                //
                
                dmap_data >> a_u32_variable;
                new_numb_items = a_u32_variable;
                break;
                
            case 'mrco':
            
                //
                //  received number of items
                //                
                
                dmap_data >> a_u32_variable;
                new_received_numb_items = a_u32_variable;
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

            default:
                warning("got an unknown tag type "
                        "while doing doDatabaseItemsResponse()");
                dmap_data >> a_chunk;
        }

        dmap_data >> end;

    }
}    

void Database::parseItems(TagInput& dmap_data, int how_many)
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
                    //  data kind ... I think this is file vs. network
                    //  stream or something (1 = radio, 2 = file) ... dunno
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


        if(new_item_id > -1 && new_item_name.length() > 0)
        {


            //
            //  Make new metadata with a rather special URL (instead of a
            //  filename)
            //  
            
            QString new_url = QString("daap://%1:%2/databases/%3/items/%4.%5?revision-number=0&session-id=%6")
                              .arg(host_address)
                              .arg(host_port)
                              .arg(daap_id)
                              .arg(new_item_id)
                              .arg(new_item_format)
                              .arg(session_id);


            AudioMetadata *new_metadata = new AudioMetadata
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

            QDateTime when;
            when.setTime_t(new_item_date_added);
            new_metadata->setDateAdded(when);
            
            when.setTime_t(new_item_date_modified);
            new_metadata->setDateModified(when);

            new_metadata->setBpm(new_item_bpm);
            new_metadata->setBitrate(new_item_bitrate);
            new_metadata->setComment(new_item_comment);
            new_metadata->setCompilation(new_item_compilation);
            new_metadata->setComposer(new_item_composer_name);
            new_metadata->setDiscCount(new_item_disc_count);            
            new_metadata->setDiscNumber(new_item_disc_number);
            new_metadata->setEqPreset(new_item_eqpreset);
            new_metadata->setFormat(new_item_format);
            new_metadata->setDescription(new_item_description);
            new_metadata->setRelativeVolume(new_item_relative_volume);
            new_metadata->setSampleRate(new_item_sample_rate);
            new_metadata->setSize(new_item_size);
            new_metadata->setStartTime(new_item_start_time);
            new_metadata->setStopTime(new_item_stop_time);
            new_metadata->setTrackCount(new_item_track_count);
            new_metadata->setRating(new_item_rating);

            
            
        }
        else
        {
            warning("got incomplete data for an item, going to ignore it");
        }        
        
        
    }
    
    double elapsed_time = loading_time.elapsed() / 1000.0;
    log(QString("parsed and loaded metadata for %1 "
                "items in %2 seconds")
                .arg(how_many)
                .arg(elapsed_time), 4);
    have_items = true;    
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
}

