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
                            DaapServerType l_server_type,
                            MFDPluginManager *l_plugin_manager,
                            int l_request_id
                        )
            :HttpOutRequest(l_base_url, l_host_address)
{
    parent = owner;
    server_type = l_server_type;
    plugin_manager = l_plugin_manager;
    request_id = l_request_id;

    //
    //  Add the server address (which the HTTP 1.1 spec is fairly adamant
    //  *must* be in there)
    // 
   
    QString host_line = QString("Host: %1").arg(host_address);
    addHeader(host_line);


    //
    //  user agent header and daap version headers, depending on the server
    //  type
    //
    
    if(server_type == DAAP_SERVER_ITUNES60)
    {
        addHeader("User-Agent: iTunes/6.0 (Windows; N)");
        addHeader("Accept-Language: en-us, en;q=5.0");
        addHeader("Client-DAAP-Version: 3.0");
    }
    else if(server_type == DAAP_SERVER_ITUNES50)
    {
        addHeader("User-Agent: iTunes/5.0 (Windows; N)");
        addHeader("Accept-Language: en-us, en;q=5.0");
        addHeader("Client-DAAP-Version: 3.0");
    }
    else if(server_type == DAAP_SERVER_ITUNES49)
    {
        addHeader("User-Agent: iTunes/4.9 (Windows; N)");
        addHeader("Accept-Language: en-us, en;q=5.0");
        addHeader("Client-DAAP-Version: 3.0");
    }
    else if(server_type == DAAP_SERVER_ITUNES48)
    {
        addHeader("User-Agent: iTunes/4.8 (Windows; N)");
        addHeader("Accept-Language: en-us, en;q=5.0");
        addHeader("Client-DAAP-Version: 3.0");
    }
    else if(server_type == DAAP_SERVER_ITUNES47)
    {
        addHeader("User-Agent: iTunes/4.7 (Windows; N)");
        addHeader("Accept-Language: en-us, en;q=5.0");
        addHeader("Client-DAAP-Version: 3.0");
    }
    else if(server_type == DAAP_SERVER_ITUNES46)
    {
        addHeader("User-Agent: iTunes/4.6 (Windows; N)");
        addHeader("Accept-Language: en-us, en;q=5.0");
        addHeader("Client-DAAP-Version: 3.0");
    }
    else if(server_type == DAAP_SERVER_ITUNES45)
    {
        addHeader("User-Agent: iTunes/4.5 (Windows; N)");
        addHeader("Accept-Language: en-us, en;q=5.0");
        addHeader("Client-DAAP-Version: 3.0");
    }
    else if(server_type == DAAP_SERVER_MYTH)
    {
        addHeader("User-Agent: MythTV/1.0 (Probably Linux)");
        QString accept_string = "Accept: audio/wav,audio/mpg,audio/ogg,audio/flac";
#ifdef AAC_AUDIO_SUPPORT
        accept_string.append(",audio/m4a");
#endif
        addHeader(accept_string);
    }
    else
    {
        addHeader("User-Agent: MythTV/1.0 (Probably Linux)");
    }

    //
    //  Add more "standard" daap headers
    //
    
    //addHeader("Cache-Control: no-cache");


    //
    //  iTunes 4.5, 4.6, etc. have an incredibly annoying feature that you
    //  sometimes have to use a different URL than the one you are actually
    //  requesting. But only in some cases. This will get used if it's set.
    //
    
    hashing_url = "";
}

bool DaapRequest::send(QSocketDevice *where_to_send, bool add_validation)
{

    if(!add_validation || server_type == DAAP_SERVER_MYTH)
    {
        return HttpOutRequest::send(where_to_send);
    }

    //
    //  If we can manage to do so, try and add a Client-DAAP-Validation header
    //


    if(plugin_manager)
    {
        if(plugin_manager->haveLibOpenDaap())
        {
            unsigned char hash[33] = {0};
        
            int daap_version_major = 2;
            if(parent)
            {
                daap_version_major = parent->getDaapVersionMajor();
            }
            else
            {
                if(server_type == DAAP_SERVER_ITUNES60 ||
                   server_type == DAAP_SERVER_ITUNES50 ||
                   server_type == DAAP_SERVER_ITUNES49 ||
                   server_type == DAAP_SERVER_ITUNES48 ||
                   server_type == DAAP_SERVER_ITUNES47 ||
                   server_type == DAAP_SERVER_ITUNES46 ||
                   server_type == DAAP_SERVER_ITUNES45 )
                {
                    daap_version_major = 3;
                }
            }
        
            QString request_string = getRequestString();
            if(hashing_url.length() > 0)
            {
                request_string = hashing_url;
            }

            //
            //  Debugging output
            //    

            /*
            cout << "################################################################### " << endl;
            cout << "daap validation header inputs:" << endl;
            cout << "       daap version major is " << daap_version_major << endl;
            cout << "       GET request string is \"" << request_string.ascii() << "\"" << endl;
            cout << "               session id is " << request_id << endl;
            cout << "################################################################### " << endl;
            */
        
        
            //
            //  Ask the plugin manager to ask libopendaap to generate the
            //  validation hash
            //
        
            plugin_manager->fillValidationHeader(
                                                    daap_version_major, 
                                                    request_string.ascii(), 
                                                    hash,
                                                    request_id
                                                );
            QString validation_header = QString("Client-DAAP-Validation: %1").arg((char *)hash);
            addHeader(validation_header);
            addHeader("Client-DAAP-Access-Index: 2");
        }
    }

    return HttpOutRequest::send(where_to_send);
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
