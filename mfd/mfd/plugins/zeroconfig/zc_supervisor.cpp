/*
	zc_supervisor.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
    !! This code is covered the the Apple Public Source License !!
    
    See ./apple/APPLE_LICENSE

*/

#include <qapplication.h>

#include "zc_supervisor.h"

ZeroConfigSupervisor::ZeroConfigSupervisor(MFD *owner, int identity, 
                                           ZeroConfigClient *a_client,
                                           ZeroConfigResponder *a_responder)
      :MFDCapabilityPlugin(owner, identity, "zeroconfig supervisor")
{
    zero_config_client = a_client;
    zero_config_responder = a_responder;
}

void ZeroConfigSupervisor::runOnce()
{
    zero_config_client->start();
    zero_config_responder->start();
}

ZeroConfigSupervisor::~ZeroConfigSupervisor()
{
}
