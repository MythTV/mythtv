/*
	main.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
    entry point for http server

*/

#include "chttpserver.h"
#include "../../mfd.h"

extern ClientHttpServer *http_server;

extern "C" {
bool         mfdplugin_init(MFD*, int);
bool         mfdplugin_run();
bool         mfdplugin_stop();
bool         mfdplugin_can_unload();
}

bool mfdplugin_init(MFD *owner, int identifier)
{
    http_server = new ClientHttpServer(owner, identifier);
    return true;
}

bool mfdplugin_run()
{
    if(http_server)
    {
        http_server->start();
        return true;
    }
    return false;
}

bool mfdplugin_stop()
{
    if(http_server)
    {
        http_server->stop();
        http_server->wakeUp();
    }
    return true;
}

bool mfdplugin_can_unload()
{
    if(http_server)
    {
        if(!http_server->running())
        {
            delete http_server;
            http_server = NULL;
        }
        return false;
    }
    return true;
}

