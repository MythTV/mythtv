#ifndef ZC_SUPERVISOR_H_
#define ZC_SUPERVISOR_H_
/*
	zc_supervisor.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project

    !! This code is covered the the Apple Public Source License !!
    
    See ./apple/APPLE_LICENSE
	
*/

#include "mfd_plugin.h"

#include "zc_responder.h"
#include "zc_client.h"

class ZeroConfigSupervisor: public MFDCapabilityPlugin
{
    //
    //  This is a thread that manages the 
    //  ZeroConfig Clients and ZeroConfig Responder
    //

  public:

    ZeroConfigSupervisor(MFD *owner, int identity, 
                         ZeroConfigClient *a_client,
                         ZeroConfigResponder *a_responder);
    ~ZeroConfigSupervisor();

    void    runOnce();

  private:
  
    ZeroConfigClient    *zero_config_client;
    ZeroConfigResponder *zero_config_responder;

};

#endif  // zc_supervisor_h_

