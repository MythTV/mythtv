#ifndef METADATACLIENT_H_
#define METADATACLIENT_H_
/*
	metadataclient.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	client object to talk to an mfd's metadata server

*/

#include <qptrlist.h>

#include <mythtv/uilistbtntype.h>

#include "serviceclient.h"
#include "../mdcaplib/mdcapinput.h"
#include "../mdcaplib/mdcapoutput.h"
#include "metadatacollection.h"

class MdcapResponse;

class MetadataClient : public ServiceClient
{

  public:

    MetadataClient(
                    MfdInterface *the_mfd,
                    int an_mfd,
                    const QString &l_ip_address,
                    uint l_port
                  );
    ~MetadataClient();

    void handleIncoming();
    void processResponse(MdcapResponse *mdcap_response);
    void sendFirstRequest();

    //
    //  Methods for sending things _to_ the server
    //
    
    void commitListEdits(
                            UIListGenericTree *playlist_tree,
                            bool new_playlist,
                            QString playlist_name
                        );

    void executeCommand(QStringList new_command);


    //
    //  Methods for handling different responses from the server
    //
    
    void parseServerInfo(MdcapInput &mdcap_input);
    void parseLogin(MdcapInput &mdcap_input);
    void parseUpdate(MdcapInput &mdcap_input);
    void parseItems(MdcapInput &mdcap_input);
    void parseLists(MdcapInput &mdcap_input);

    int  parseCollection(MdcapInput &mdcap_input);

    void parseNewItemData(
                            MetadataCollection *which_collection,
                            int  collection_generation,
                            bool update_type,
                            int  total_count,
                            int  add_count,
                            int  del_count,
                            MdcapInput &mdcap_input
                         );
    
    void parseDeletedItemData(
                                MetadataCollection *which_collection,
                                MdcapInput &mdcap_input
                            );
    
    void parseNewListData(
                            MetadataCollection *which_collection,
                            int  collection_generation,
                            bool update_type,
                            int  total_count,
                            int  add_count,
                            int  del_count,
                            MdcapInput &mdcap_input
                         );

    void parseDeletedListData(
                                MetadataCollection *which_collection,
                                MdcapInput &mdcap_input
                             );

    MetadataCollection* findCollection(int collection_id);
    void printMetadata();   // Debugging
    void buildTree();
        
  private:
  
    uint32_t session_id;
    QPtrList<MetadataCollection>    metadata_collections;
};

#endif
