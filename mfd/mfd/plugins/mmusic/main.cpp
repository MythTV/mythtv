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
        mmusic_watcher->start(QThread::LowestPriority);
        return true;
    }
    return false;
}

bool mfdplugin_stop()
{
    if(mmusic_watcher)
    {
        mmusic_watcher->stop();
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
        }
        return false;
    }
    return true;
}
