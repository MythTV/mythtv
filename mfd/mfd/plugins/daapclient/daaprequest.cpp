/*
	daaprequest.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
    A little object for making daap requests

*/

#include "../../../config.h"

#include <vector>
#include <iostream>
using namespace std;

#include "daaprequest.h"

DaapRequest::DaapRequest(
                            DaapInstance *owner,
                            const QString& l_base_url, 
                            const QString& l_host_address,
                            DaapServerType l_server_type
                        )
            :HttpOutRequest(l_base_url, l_host_address)
{
    parent = owner;
    server_type = l_server_type;


    //
    //  Add "standard" daap headers
    //
    
    addHeader("Cache-Control: no-cache");


    //
    //  If the server is an actual mfd, tell it precisely what formats we
    //  understand (it will try and convert other to wav if we ask for them)
    //

    if(server_type == DAAP_SERVER_MYTH)
    {
        QString accept_string = "Accept: audio/wav,audio/mpg,audio/ogg,audio/flac";
#ifdef AAC_AUDIO_SUPPORT
        accept_string.append(",audio/m4a");
#endif
        addHeader(accept_string);
    }
    else
    {
        addHeader("Accept: */*");
    }

    //
    //  More standard headers
    //
    
    addHeader("Client-DAAP-Version: 2.0");
    addHeader("User-Agent: MythTV/1.0 (Probably Linux)");

    //
    //  Add the server address (which the HTTP 1.1 spec is fairly adamant
    //  *must* be in there)
    // 
   
    QString host_line = QString("Host: %1").arg(host_address);
    addHeader(host_line);


}

void DaapRequest::warning(const QString &warn_text)
{
    if(parent)
    {
        parent->warning(warn_text);
    }
    else
    {
        cerr << "WARNING: daaprequest.o: " << warn_text << endl;
    }
}

DaapRequest::~DaapRequest()
{
}
