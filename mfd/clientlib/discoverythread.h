#ifndef DISCOVERYTHREAD_H_
#define DISCOVERYTHREAD_H_
/*
	discoverythread.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Little thread that keeps looking for (dis)appearance of mfd's

*/

#include <qthread.h>
#include <qmutex.h>

class DiscoveryThread : public QThread
{

  public:

    DiscoveryThread();
    ~DiscoveryThread();
    
    void run();
    void stop();

  private:
  
    QMutex keep_going_mutex;
    bool   keep_going;
    
};

#endif
