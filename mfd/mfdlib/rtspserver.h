#ifndef RTSPSERVER_H_
#define RTSPSERVER_H_
/*
	rtspserver.h

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	An rtsp server (as an mfd plugin).

*/

#include "mfd_plugin.h"

class RtspInRequest;
class RtspOutResponse;

class MFDRtspPlugin : public MFDServicePlugin
{

  public:
  
    MFDRtspPlugin(
                    MFD *owner, 
                    int identifier, 
                    uint port, 
                    const QString &a_name = "unkown",
                    int l_minimum_thread_pool_size = 0
                 );
    virtual ~MFDRtspPlugin();


    virtual void    processRequest(MFDServiceClientSocket *a_client);
    virtual void    sendResponse(int client_id, RtspOutResponse *rtsp_response);

    //
    //  Standard RTSP request types (default implementations just send 501
    //  Not Implemented responses)
    //
    
    virtual void    handleDescribeRequest(RtspInRequest *in_request, int client_id);
    virtual void    handleGetParameterRequest(RtspInRequest *in_request, int client_id);
    virtual void    handleOptionsRequest(RtspInRequest *in_request, int client_id);
    virtual void    handlePauseRequest(RtspInRequest *in_request, int client_id);
    virtual void    handlePlayRequest(RtspInRequest *in_request, int client_id);
    virtual void    handlePingRequest(RtspInRequest *in_request, int client_id);
    virtual void    handleRedirectRequest(RtspInRequest *in_request, int client_id);
    virtual void    handleSetupRequest(RtspInRequest *in_request, int client_id);
    virtual void    handleSetParameterRequest(RtspInRequest *in_request, int client_id);
    virtual void    handleTeardownRequest(RtspInRequest *in_request, int client_id);
};

#endif

