/*
	main.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
    entry point for a very simple plugin

*/

#include "zc_supervisor.h"
#include "../../mfd.h"


ZeroConfigSupervisor *zero_config_supervisor = NULL;
extern ZeroConfigClient     *zero_config_client;
extern ZeroConfigResponder  *zero_config_responder;

extern "C" {
bool         mfdplugin_init(MFD*, int);
QStringList  mfdplugin_capabilities();
bool         mfdplugin_run();
void         mfdplugin_parse_tokens(const QStringList &tokens, int socket_identifier);
bool         mfdplugin_stop();
bool         mfdplugin_can_unload();
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

QStringList mfdplugin_capabilities()
{
    if(zero_config_supervisor)
    {
        return zero_config_supervisor->getCapabilities();
    }
    
    return NULL;
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

void mfdplugin_parse_tokens(const QStringList &tokens, int socket_identifier)
{
    if(zero_config_supervisor)
    {
        zero_config_supervisor->parseTokens(tokens, socket_identifier);
    }
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


