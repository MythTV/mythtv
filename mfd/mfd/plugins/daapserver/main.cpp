/*
	main.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
    entry point for daap server

*/

#include "daapserver.h"
#include "../../mfd.h"

DaapServer *daap_server = NULL;

extern "C" {
bool         mfdplugin_init(MFD*, int);
bool         mfdplugin_run();
bool         mfdplugin_stop();
bool         mfdplugin_can_unload();
}

bool mfdplugin_init(MFD *owner, int identifier)
{
    daap_server = new DaapServer(owner, identifier);
    return true;
}

bool mfdplugin_run()
{
    if(daap_server)
    {
        daap_server->start();
        return true;
    }
    return false;
}

bool mfdplugin_stop()
{
    if(daap_server)
    {
        daap_server->stop();
    }
    return true;
}

bool mfdplugin_can_unload()
{
    if(daap_server)
    {
        if(!daap_server->running())
        {
            delete daap_server;
            daap_server = NULL;
        }
        return false;
    }
    return true;
}

