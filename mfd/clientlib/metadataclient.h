#ifndef METADATACLIENT_H_
#define METADATACLIENT_H_
/*
	metadataclient.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	client object to talk to an mfd's metadata server

*/

#include "serviceclient.h"


class MetadataClient : public ServiceClient
{

  public:

    MetadataClient(
                    const QString &l_ip_address,
                    uint l_port
                  );

    void handleIncoming();

    ~MetadataClient();

};

#endif
