/*
	signalthread.cpp

	(c) 2003, 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Methods for a thread that does nothing but look for INT and TERM signals
	and then ask the core mfd to initiate a shutdown

*/

#include <signal.h>
#include <iostream>
using namespace std;

#include "../mfdlib/mfd_events.h"

#include "signalthread.h"

#include "mfd.h"


SignalThread *signal_thread = NULL;

extern "C" void signal_thread_catch_signal(int signal_number)
{
    if(signal_thread)
    {
        signal_thread->handleSignal(signal_number);
    }
    else
    {
        cerr << "signal thread catcher can't find the external SignalThread object" << endl;
    }    
}

/*
---------------------------------------------------------------------
*/

SignalThread::SignalThread(MFD *owner)
             :MFDServicePlugin(owner, -2, 0, "signal thread", false)
{
    the_mfd = owner;
}

void SignalThread::run()
{
    //
    //  Set up this thread to handle INT and TERM signals
    //


    sigset_t signal_mask;
    struct sigaction action, old_action;
    
    //
    //  Unlike all other threads, this thread does _not_ ignore Interrupt
    //  and Terminate signals
    //

    sigprocmask(0, 0, &signal_mask);
    sigdelset(&signal_mask, SIGINT);
    sigdelset(&signal_mask, SIGTERM);
    sigprocmask(SIG_SETMASK, &signal_mask, 0);
    
    //
    //  When these signals occur, jump to an extern "C" function that will,
    //  in turn, call this objects handleSignal() method
    //

    sigemptyset(&action.sa_mask);
    action.sa_handler = signal_thread_catch_signal;
    action.sa_flags = 0;
    sigaction(SIGINT, &action, &old_action);
    sigaction(SIGTERM, &action, &old_action);
    
    //
    //  Wait for a signal to show up
    //

    while(keep_going)
    {
        sleep(2);
    }
}

void SignalThread::handleSignal(int signal_number)
{
    if(signal_number == SIGINT)
    {
        log("user hit CTRL-C (too lazy to run \"mfdctl stop\"?), will call mfd shutdown", 1);
        //the_mfd->shutDown();
        ShutdownEvent *se = new ShutdownEvent();
        QApplication::postEvent(the_mfd, se);
    }
    else if(signal_number == SIGTERM)
    {
        log("we got a SIGTERM (box is shutting down?), will call mfd shutdown", 1);
        ShutdownEvent *se = new ShutdownEvent();
        QApplication::postEvent(the_mfd, se);
    }
    else
    {
        warning("getting signals I don't understand (ie. not SIGINT or SIGTERM)");
    }
}

SignalThread::~SignalThread()
{
}



