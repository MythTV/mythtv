/*
	main.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
    entry point for daapclient

*/

#include "daapclient.h"
#include "../../mfd.h"

DaapClient *daap_client = NULL;

extern "C" {
bool         mfdplugin_init(MFD*, int);
bool         mfdplugin_run();
bool         mfdplugin_stop();
bool         mfdplugin_can_unload();
void         mfdplugin_services_change();
}


bool mfdplugin_init(MFD *owner, int identifier)
{
    daap_client = new DaapClient(owner, identifier);
    return true;
}

bool mfdplugin_run()
{
    if(daap_client)
    {
        daap_client->start();
        return true;
    }
    return false;
}

bool mfdplugin_stop()
{
    if(daap_client)
    {
        daap_client->stop();
        daap_client->wakeUp();
    }
    return true;
}






bool mfdplugin_can_unload()
{
    if(daap_client)
    {
        if(!daap_client->running())
        {
            delete daap_client;
            daap_client = NULL;
        }
        return false;
    }
    return true;
}

void mfdplugin_services_change()
{
    if(daap_client)
    {
        daap_client->servicesChanged();
    }
}
