/*
	mfdinterface.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Core entry point for the facilities available in libmfdclient

*/

#include <iostream>
using namespace std;

#include "mfdinterface.h"

MfdInterface::MfdInterface()
{
    //
    //  Create a thread which just sits around looking for mfd's to appear or dissappear
    //

    discovery_thread = new DiscoveryThread();
    
    //
    //  run the discovery thread
    //
    
    discovery_thread->start();
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
