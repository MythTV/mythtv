#ifndef DATABASE_H_
#define DATABASE_H_
/*
	database.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	This is nothing to do with SQL or MYSQL A large collection of items
	(songs) and containers (playlists) is called a "database" in DAAP.
	That's what this is.

*/

#include <qstring.h>

#include "../../mfd.h"
#include "mdcontainer.h"
#include "../../mdserver.h"

#include "../daapserver/daaplib/taginput.h"
#include "../daapserver/daaplib/tagoutput.h"

#include "server_types.h"

class DaapClient;

class Database
{

  public:
  
    Database(
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
            );
    ~Database();

    bool    hasItems(){return have_items;}
    bool    hasPlaylistList(){return have_playlist_list;}
    bool    hasPlaylists(){return have_playlists;}
    
    int     getDaapId(){return daap_id;}
    int     getFirstPlaylistWithoutList();

    void    doDatabaseItemsResponse(TagInput &dmap_data, bool ripable);
    void    parseItems(TagInput &dmap_data, int how_many_now);
    void    parseDeletions(TagInput &dmap_data);    

    void    doDatabaseListPlaylistsResponse(TagInput &dmap_data, int new_generation);    
    void    parseContainers(TagInput &dmap_data, int how_many);
    void    parseDeletedContainers(TagInput &dmap_data, int new_generation);

    void    doDatabasePlaylistResponse(TagInput &dmap_data, int which_playlist, int new_generation);
    void    parsePlaylist(TagInput &dmap_data, int how_many, Playlist *which_playlist, int update_type);
    void    parsePlaylistItemDeletions(TagInput &dmap_data, Playlist *which_playlist, int update_type);

    void    doTheMetadataSwap(int new_generation);
    
    int     getKnownGeneration(){return generation_delta;}
    void    beIgnorant();
    void    checkUpdate(u8 update_type);
    //void    checkUpdateType(int new_numb_items, int new_received_numb_items);
         
  private:   

    DaapClient *parent;
    void    log(const QString &log_message, int verbosity);
    void    warning(const QString &warning_message);  
    int     daap_id;
    QString name;
    int     expected_numb_items;
    int     expected_numb_playlists;
    
    MFD                 *the_mfd;
    MetadataServer      *metadata_server;
    MetadataContainer   *metadata_container;
    int                 container_id;

    bool    have_items;
    bool    have_playlist_list;
    bool    have_playlists;
    
    int session_id;
    QString host_address;
    int host_port;

    QIntDict<Metadata>      *new_metadata;
    QIntDict<Playlist>      *new_playlists;
    
    QValueList<int>         metadata_additions;
    QValueList<int>         metadata_deletions;
    
    QValueList<int>         playlist_additions;
    QValueList<int>         playlist_deletions;

    QValueList<int>         previous_metadata;
    QValueList<int>         previous_playlists;

    int                     generation_delta;
    bool                    full_data_update;
    bool                    update_type_set;
    DaapServerType          daap_server_type;
};
#endif  // database_h_
