/*
	main.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
    entry point for audio plugin

*/

#include "audio.h"
#include "../../mfd.h"

AudioPlugin *audio_plugin = NULL;

extern "C" {
bool         mfdplugin_init(MFD*, int);
bool         mfdplugin_run();
bool         mfdplugin_stop();
bool         mfdplugin_can_unload();
}

bool mfdplugin_init(MFD *owner, int identifier)
{
    audio_plugin = new AudioPlugin(owner, identifier);
    return true;
}

bool mfdplugin_run()
{
    if(audio_plugin)
    {
        audio_plugin->start();
        return true;
    }
    return false;
}

bool mfdplugin_stop()
{
    if(audio_plugin)
    {
        audio_plugin->stop();
        audio_plugin->wakeUp();
    }
    return true;
}

bool mfdplugin_can_unload()
{
    if(audio_plugin)
    {
        if(!audio_plugin->running())
        {
            delete audio_plugin;
            audio_plugin = NULL;
        }
        return false;
    }
    return true;
}
