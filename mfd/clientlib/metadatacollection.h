#ifndef METADATACOLLECTION_H_
#define METADATACOLLECTION_H_
/*
	metadatacollection.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Object to hold metadata collections sent over the wire

*/

#include <stdint.h>

#include <qstring.h>

class MdcapInput;

#include "../mfdlib/metadata.h"

class MetadataCollection
{

  public:

     MetadataCollection(
                        int an_id,
                        int new_collection_count,
                        QString new_collection_name,
                        int new_collection_generation,
                        MetadataType l_metadata_type
                       );

    ~MetadataCollection();

    int          getId(){ return id;}
    int          getGeneration(){ return current_combined_generation; }
    int          getPending(){ return pending_generation; }
    void         setPending(int an_int){pending_generation = an_int; }
    void         setExpectedCount(int an_int){expected_count = an_int;}
    bool         itemsUpToDate();
    bool         listsUpToDate();
    void         beUpToDate();
    QString      getItemsRequest(uint32_t session_id);
    QString      getListsRequest(uint32_t session_id);
    void         addItem(MdcapInput &mdcap_input);
    void         addList(MdcapInput &mdcap_input);
    void         setMetadataGeneration(int an_int){current_metadata_generation = an_int;}
    void         setPlaylistGeneration(int an_int){current_playlist_generation = an_int;}
    void         clearAllMetadata(){metadata.clear();}
    void         clearAllPlaylists(){playlists.clear();}
    QString      getName(){return name;}
    MetadataType getType(){return metadata_type;}
    void         deleteItem(uint which_item);
    void         deleteList(uint which_list);
    void         printMetadata();   // Debugging


    //
    //  Get at the contents
    //

    QIntDict<Metadata>* getMetadata(){ return &metadata;}
    QIntDict<Playlist>* getPlaylists(){ return &playlists;}
    
  private:
  
    int          id;
    int          expected_count;
    QString      name;
    int          pending_generation;
    MetadataType metadata_type;

    int          current_metadata_generation;
    int          current_playlist_generation;    
    int          current_combined_generation;

    QIntDict<Metadata>  metadata;    
    QIntDict<Playlist>  playlists;
};

#endif
