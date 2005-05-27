/*
	main.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
    entry point for transcoding plugin

*/

#include "transcoder.h"
#include "../../mfd.h"

extern Transcoder *transcoder;

extern "C" {
bool         mfdplugin_init(MFD*, int);
bool         mfdplugin_run();
bool         mfdplugin_stop();
bool         mfdplugin_can_unload();
}

bool mfdplugin_init(MFD *owner, int identifier)
{
    transcoder = new Transcoder(owner, identifier);
    return true;
}

bool mfdplugin_run()
{
    if(transcoder)
    {
        transcoder->start();
        return true;
    }
    return false;
}

bool mfdplugin_stop()
{
    if(transcoder)
    {
        transcoder->stop();
        transcoder->wakeUp();
    }
    return true;
}

bool mfdplugin_can_unload()
{
    if(transcoder)
    {
        if(!transcoder->running())
        {
            delete transcoder;
            transcoder = NULL;
            return true;
        }
        return false;
    }
    return true;
}

