/*
	mdcaprequest.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
    A little object for making mdcap requests

*/

#include "mdcaprequest.h"

MdcapRequest::MdcapRequest(
                            const QString& l_base_url, 
                            const QString& l_host_address
                        )
            :HttpOutRequest(l_base_url, l_host_address)
{


    //
    //  Add mdcap headers
    //
    
    addHeader("Client-MDCAP-Version: 2.0");
    addHeader("User-Agent: MythTV/1.0 (Probably Linux)");

    //
    //  Add the server address (which the HTTP 1.1 spec is fairly adamant
    //  *must* be in there)
    // 
   
    QString host_line = QString("Host: %1").arg(host_address);
    addHeader(host_line);


}

MdcapRequest::~MdcapRequest()
{
}
