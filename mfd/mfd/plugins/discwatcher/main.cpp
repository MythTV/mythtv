/*
	main.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
    entry point for http server

*/

#include "discwatcher.h"
#include "../../mfd.h"

extern DiscWatcher *disc_watcher;

extern "C" {
bool         mfdplugin_init(MFD*, int);
bool         mfdplugin_run();
bool         mfdplugin_stop();
bool         mfdplugin_can_unload();
}

bool mfdplugin_init(MFD *owner, int identifier)
{
    disc_watcher = new DiscWatcher(owner, identifier);
    return true;
}

bool mfdplugin_run()
{
    if(disc_watcher)
    {
        disc_watcher->start();
        return true;
    }
    return false;
}

bool mfdplugin_stop()
{
    if(disc_watcher)
    {
        disc_watcher->stop();
        disc_watcher->wakeUp();
    }
    return true;
}

bool mfdplugin_can_unload()
{
    if(disc_watcher)
    {
        if(!disc_watcher->running())
        {
            delete disc_watcher;
            disc_watcher = NULL;
        }
        return false;
    }
    return true;
}

