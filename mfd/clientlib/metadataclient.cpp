/*
	metadataclient.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	client object to talk to an mfd's metadata service

*/

#include <iostream>
using namespace std;

#include "metadataclient.h"


MetadataClient::MetadataClient(
                            const QString &l_ip_address,
                            uint l_port
                        )
            :ServiceClient(
                            MFD_SERVICE_METADATA,
                            l_ip_address,
                            l_port
                          )
{
}

void MetadataClient::handleIncoming()
{
    cout << "metadata client is getting something on handleIncoming() " << endl;
}

MetadataClient::~MetadataClient()
{
}




