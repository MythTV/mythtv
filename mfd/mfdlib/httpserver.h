#ifndef HTTPSERVER_H_
#define HTTPSERVER_H_
/*
	httpserver.h

	(c) 2003-2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	An http server (as an mfd plugin). Multithreaded 'n everything

*/

#include "mfd_plugin.h"

class MFDHttpPlugin : public MFDServicePlugin
{

  public:
  
    MFDHttpPlugin(
                    MFD *owner, 
                    int identifier, 
                    uint port, 
                    const QString &a_name = "unkown",
                    int l_minimum_thread_pool_size = 0
                 );
    virtual ~MFDHttpPlugin();

    virtual void    processRequest(MFDServiceClientSocket *a_client);
    virtual void    handleIncoming(HttpInRequest *request, int client_id);
    virtual void    sendResponse(int client_id, HttpOutResponse *http_response);
};

#endif

