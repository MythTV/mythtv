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
    // do_stuff = false;
    client_socket = NULL;
}

void ServiceRequestThread::handleIncoming(MFDServiceClientSocket *socket)
{
    //
    //  Set which socket we are supposed to interact with, then set us loose
    //  to process the incoming data on that socket
    //
    
    prt_mutex.lock();
        client_socket = socket;
        //do_stuff = true;
    prt_mutex.unlock();
    wait_condition.wakeOne();
}

void ServiceRequestThread::killMe()
{
    //
    //  Turn off the keep_going flag, so we will exit.
    //
    
    prt_mutex.lock();
        keep_going_mutex.lock();
            keep_going = false;
        keep_going_mutex.unlock();
    prt_mutex.unlock();

    wait_condition.wakeOne();
}

void ServiceRequestThread::run()
{
    //
    //  Deceptively simple. Only because this is very closely based on the
    //  thread pool from mythbackend mainserver.cpp, which has been well
    //  thought out.
    //

    keep_going_mutex.lock();
        keep_going = true;
    keep_going_mutex.unlock();
    
    prt_mutex.lock();

    while (true)
    {
        wait_condition.wait(&prt_mutex);
        if(!keep_going)
        {
            break;
        }

        parent->processRequest(client_socket);
        //do_stuff = false;
        client_socket = NULL;
        parent->markUnused(this);
        parent->wakeUp();
    }
    
    prt_mutex.unlock();
}

