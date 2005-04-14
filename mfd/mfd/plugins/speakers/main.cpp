/*
	main.cpp

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
    entry point for speakers plugin

*/

#include "speakers.h"
#include "../../mfd.h"

Speakers *speakers = NULL;

extern "C" {
bool         mfdplugin_init(MFD*, int);
bool         mfdplugin_run();
bool         mfdplugin_stop();
bool         mfdplugin_can_unload();
}

bool mfdplugin_init(MFD *owner, int identifier)
{
    speakers = new Speakers(owner, identifier);
    return true;
}

bool mfdplugin_run()
{
    if(speakers)
    {
        speakers->start();
        return true;
    }
    return false;
}

bool mfdplugin_stop()
{
    if(speakers)
    {
        speakers->stop();
        speakers->wakeUp();
    }
    return true;
}

bool mfdplugin_can_unload()
{
    if(speakers)
    {
        if(!speakers->running())
        {
            delete speakers;
            speakers = NULL;
            return true;
        }
        return false;
    }
    return true;
}

