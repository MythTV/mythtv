#ifndef MFDINTERFACE_H_
#define MFDINTERFACE_H_
/*
	mfdinterface.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Core entry point for the facilities available in libmfdclient

*/

#include <qobject.h>
#include <qptrlist.h>

class DiscoveryThread;
class MfdInstance;

class MfdInterface : public QObject
{

  public:

    MfdInterface();
    ~MfdInterface();


  protected:
  
    void customEvent(QCustomEvent *ce);
 

  private:

    MfdInstance*    findMfd(
                            const QString &a_host,
                            const QString &an_ip_addesss,
                            int a_port
                           );
  
    DiscoveryThread       *discovery_thread;
    QPtrList<MfdInstance> *mfd_instances;
};

#endif
