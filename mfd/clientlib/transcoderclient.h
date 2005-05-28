#ifndef TRANSCODERCLIENT_H_
#define TRANSCODERCLIENT_H_
/*
	transcoderclient.h

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	client object to talk to an mfd's transcoder server

*/

#include "serviceclient.h"

class TranscoderClient : public ServiceClient
{

  public:

    TranscoderClient(
                    MfdInterface *the_mfd,
                    int an_mfd,
                    const QString &l_ip_address,
                    uint l_port
                  );
    ~TranscoderClient();

    void executeCommand(QStringList new_command);
    void handleIncoming();
    void requestRipAudioCd(int collection_id);

/*
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
*/


};

#endif
