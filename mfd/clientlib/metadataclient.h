#ifndef METADATACLIENT_H_
#define METADATACLIENT_H_
/*
	metadataclient.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	client object to talk to an mfd's metadata server

*/

#include "serviceclient.h"
#include "../mdcaplib/mdcapinput.h"

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
    //  Methods for handling different responses from the server
    //
    
    void parseServerInfo(MdcapInput &mdcap_input);
};

#endif
