#ifndef DISCOVERYTHREAD_H_
#define DISCOVERYTHREAD_H_
/*
	discoverythread.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Little thread that keeps looking for (dis)appearance of mfd's

*/

#include <unistd.h>

#include <qthread.h>
#include <qmutex.h>
#include <qptrlist.h>

#include "mfdinterface.h"
#include "mdnsd/mdnsd.h"
#include "discovered.h"



class DiscoveryThread : public QThread
{

  public:

    DiscoveryThread(MfdInterface *the_interface);
    ~DiscoveryThread();
    
    void run();
    void stop();
    void wakeUp();
    int  createMulticastSocket();
    void handleMdnsdCallback(mdnsda answer);
    void cleanDeadMfds();
    
    
  private:
  
    QMutex keep_going_mutex;
    bool   keep_going;

    QMutex  u_shaped_pipe_mutex;
    int     u_shaped_pipe[2];

    mdnsd   mdns_daemon;
    mdnsd   mdns_ipaddr_daemon;
    
    QPtrList<DiscoveredMfd> *discovered_mfds;
    
    MfdInterface    *mfd_interface;
};

#endif
