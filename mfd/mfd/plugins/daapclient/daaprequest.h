#ifndef DAAPREQUEST_H_
#define DAAPREQUEST_H_
/*
	daaprequest.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
    A little object for making daap requests

*/

#include <vector>
using namespace std;

#include <qstring.h>
#include <qsocketdevice.h>
#include <qdict.h>

#include "httpoutrequest.h"

//  class DaapInstance;

#include "daapinstance.h"
#include "../../pluginmanager.h"

class DaapRequest : public HttpOutRequest
{

  public:

    DaapRequest(
                DaapInstance *owner,
                const QString& l_base_url, 
                const QString& l_host_address,
                DaapServerType l_server_type = DAAP_SERVER_UNKNOWN,
                MFDPluginManager *l_plugin_manager = NULL,
                int l_request_id = 0
               );
               
    ~DaapRequest();

    bool send(QSocketDevice *where_to_send, bool daap_validation = false);
    void warning(const QString &warn_text);
    void setHashingUrl(const QString &a_string){hashing_url = a_string;}
    
  private:

    DaapInstance        *parent;
    DaapServerType      server_type;
    MFDPluginManager    *plugin_manager;
    int                 request_id;    
    QString             hashing_url;    
};

#endif  // daaprequest_h_
