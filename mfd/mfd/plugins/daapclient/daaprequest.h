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
                MFDPluginManager *l_plugin_manager = NULL
               );
               
    ~DaapRequest();

    bool send(QSocketDevice *where_to_send, bool add_validation = false);
    void warning(const QString &warn_text);
    
  private:

    DaapInstance        *parent;
    DaapServerType      server_type;
    MFDPluginManager    *plugin_manager;    
    
};

#endif  // daaprequest_h_
