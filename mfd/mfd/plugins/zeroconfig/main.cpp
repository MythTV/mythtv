/*
	main.cpp

	(c) 2003-2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
    entry point for the zeroconfig stuff

*/

#include "zc_supervisor.h"
#include "../../mfd.h"


ZeroConfigSupervisor *zero_config_supervisor = NULL;
extern ZeroConfigClient     *zero_config_client;
extern ZeroConfigResponder  *zero_config_responder;

extern "C" {
bool         mfdplugin_init(MFD*, int);
bool         mfdplugin_run();
bool         mfdplugin_stop();
bool         mfdplugin_can_unload();
void         mfdplugin_services_change();
}


bool mfdplugin_init(MFD *owner, int identifier)
{
    zero_config_client     = new ZeroConfigClient(owner, identifier);
    zero_config_responder  = new ZeroConfigResponder(owner, identifier);
    zero_config_supervisor = new ZeroConfigSupervisor(owner, identifier, 
                                                      zero_config_client,
                                                      zero_config_responder);
    return true;
}

bool mfdplugin_run()
{
    if(zero_config_supervisor)
    {
        zero_config_supervisor->start();
        return true;
    }
    return false;
}

bool mfdplugin_stop()
{
    if(zero_config_client)
    {
        zero_config_client->stop();
        zero_config_client->wakeUp();
    }
    if(zero_config_responder)
    {
        zero_config_responder->stop();
        zero_config_responder->wakeUp();
    }
    if(zero_config_supervisor)
    {
        zero_config_supervisor->stop();
        zero_config_supervisor->wakeUp();
    }
    return true;
}

bool mfdplugin_can_unload()
{
    bool return_value = true;
    if(zero_config_client)
    {
        if(!zero_config_client->running())
        {
            delete zero_config_client;
            zero_config_client = NULL;
        }
        else
        {
            return_value = false;
        }
    }
    if(zero_config_responder)
    {
        if(!zero_config_responder->running())
        {
            delete zero_config_responder;
            zero_config_responder = NULL;
        }
        else
        {
            return_value = false;
        }
    }
    if(zero_config_supervisor)
    {
        if(!zero_config_supervisor->running())
        {
            delete zero_config_supervisor;
            zero_config_supervisor = NULL;
        }
        else
        {
            return_value = false;
        }
    }
    return return_value;
}

void mfdplugin_services_change()
{
    if(zero_config_client)
    {
        zero_config_client->servicesChanged();
    }
    if(zero_config_responder)
    {
        zero_config_responder->servicesChanged();
    }
}
