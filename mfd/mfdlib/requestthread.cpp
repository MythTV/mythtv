/*
	requestthread.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Little threads that make some of the plugin classes multi-threaded

*/

#include <qthread.h>

#include "requestthread.h"
#include "mfd_plugin.h"

ServiceRequestThread::ServiceRequestThread(MFDServicePlugin *owner)
{
    //
    //  Set our parent and organize ourselves so that when run(), we just
    //  sit in a QWaitCondition until asked to do something
    //

    parent = owner;
    keep_going = true;
    do_stuff = false;
    client_socket = NULL;
}

void ServiceRequestThread::handleIncoming(MFDServiceClientSocket *socket)
{
    //
    //  Set which socket we are supposed to interact with, then set us loose
    //  to process the incoming data on that socket
    //
    
    client_socket = socket;
    do_stuff = true;
    wait_condition.wakeOne();
}

void ServiceRequestThread::killMe()
{
    //
    //  Turn off the keep_going flag, so we will exit.
    //
    
    keep_going = false;
    wait_condition.wakeOne();
}

void ServiceRequestThread::run()
{
    //
    //  Deceptively simple. Only because this is very closely based on the
    //  thread pool from mythbackend mainserver.cpp, which has been well
    //  though out.
    //

    while (keep_going)
    {
        if (!do_stuff)
        {
            wait_condition.wait();
        }
        if (do_stuff)
        {
            parent->processRequest(client_socket);
            do_stuff = false;
            client_socket = NULL;
            parent->markUnused(this);
            parent->wakeUp();
        }
    }
}

