#ifndef MFDINTERFACE_H_
#define MFDINTERFACE_H_
/*
	mfdinterface.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Core entry point for the facilities available in libmfdclient

*/

#include <qobject.h>

class DiscoveryThread;

class MfdInterface : public QObject
{

  public:

    MfdInterface();
    ~MfdInterface();

  protected:
  
    void customEvent(QCustomEvent *ce);
 
  private:
  
    DiscoveryThread *discovery_thread;
    
};

#endif
