#ifndef SIGNALTHREAD_H_
#define SIGNALTHREAD_H_
/*
	signalthread.h

	(c) 2003, 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for a thread that does nothing but look for INT and TERM signals
	and then ask the core mfd to initiate a shutdown

*/

#include "../mfdlib/mfd_plugin.h"

class MFD;

class SignalThread : public MFDServicePlugin
{
    //
    //  Don't be confused by the fact that this inherits from
    //  MFDServicePlugin. It is not dynamically loaded.
    //

  public:
  
    SignalThread(MFD *owner);
    ~SignalThread();

    void    run();
    void    handleSignal(int signal_number);
    
  private:
  
    MFD *the_mfd;

};


#endif
