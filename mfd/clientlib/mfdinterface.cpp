/*
	mfdinterface.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Core entry point for the facilities available in libmfdclient

*/

#include <iostream>
using namespace std;

#include "mfdinterface.h"
#include "discoverythread.h"
#include "events.h"

MfdInterface::MfdInterface()
{
    //
    //  Create a thread which just sits around looking for mfd's to appear or dissappear
    //

    discovery_thread = new DiscoveryThread(this);
    
    //
    //  run the discovery thread
    //
    
    discovery_thread->start();
}

void MfdInterface::customEvent(QCustomEvent *ce)
{
    if(ce->type() == MFD_CLIENTLIB_EVENT_DISCOVERY)
    {
        //
        //  It's an mfd discovery event
        //

        MfdDiscoveryEvent *de = (MfdDiscoveryEvent*)ce;
        if(de->getLostOrFound())
        {
            cout << "found this mfd: "
                 << de->getHost()
                 << " ("
                 << de->getAddress()
                 << ":"
                 << de->getPort()
                 << ")"
                 << endl;
        }
        else
        {
            cout << " lost this mfd: "
                 << de->getHost()
                 << " ("
                 << de->getAddress()
                 << ":"
                 << de->getPort()
                 << ")"
                 << endl;
        }
    }
}

MfdInterface::~MfdInterface()
{
    //
    //  Shut down and delete the discovery thread
    //
    
    discovery_thread->stop();
    discovery_thread->wait();
    delete discovery_thread;
}
