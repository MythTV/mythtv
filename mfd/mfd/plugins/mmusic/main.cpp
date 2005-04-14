/*
	main.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
    entry point for MythMusic Watcher

*/

#include "mmusic.h"
#include "../../mfd.h"

MMusicWatcher *mmusic_watcher = NULL;

extern "C" {
bool         mfdplugin_init(MFD*, int);
bool         mfdplugin_run();
bool         mfdplugin_stop();
bool         mfdplugin_can_unload();
void         mfdplugin_metadata_change(int, bool);
}


bool mfdplugin_init(MFD *owner, int identifier)
{
    mmusic_watcher = new MMusicWatcher(owner, identifier);
    return true;
}

bool mfdplugin_run()
{
    if(mmusic_watcher)
    {
        mmusic_watcher->start();
        return true;
    }
    return false;
}

bool mfdplugin_stop()
{
    if(mmusic_watcher)
    {
        mmusic_watcher->stop();
        mmusic_watcher->wakeUp();
    }
    return true;
}

bool mfdplugin_can_unload()
{
    if(mmusic_watcher)
    {
        if(!mmusic_watcher->running())
        {
            delete mmusic_watcher;
            mmusic_watcher = NULL;
            return true;
        }
        return false;
    }
    return true;
}

void mfdplugin_metadata_change(int which_collection, bool external)
{
    if(mmusic_watcher)
    {
        mmusic_watcher->metadataChanged(which_collection, external);
    }
}
