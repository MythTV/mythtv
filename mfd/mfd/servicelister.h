#ifndef SERVICELISTER_H_
#define SERVICELISTER_H_
/*
	servicelister.h

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Small object that maintains a list of all services (eg. http, daap,
	etc.) that are available
	
*/

#include <qmutex.h>
#include <qvaluelist.h>

#include "service.h"

class ServiceEvent;
class MFD;

class ServiceLister
{
  public:
  
    typedef     QValueList<Service> ServiceList;

    ServiceLister(MFD *owner);
    ~ServiceLister();
    void                    lockDiscoveredList();
    void                    unlockDiscoveredList();
    void                    lockBroadcastList();
    void                    unlockBroadcastList();
    void                    handleServiceEvent(ServiceEvent *se);
    void                    addDiscoveredService(Service *service);
    void                    removeDiscoveredService(Service *service);
    void                    addBroadcastService(Service *service);
    void                    removeBroadcastService(Service *service);
    Service*                findBroadcastService(const QString &name);
    Service*                findDiscoveredService(const QString &name);
    QValueList<Service>*    getBroadcastList();
    QValueList<Service>*    getDiscoveredList();
    void                    log(const QString &log_text, int verbosity);
    void                    warning(const QString &warning_text);
    
    
  private:
  
    MFD         *parent;
    QMutex      discovered_list_mutex;
    QMutex      broadcast_list_mutex;
    ServiceList discovered_service_list;
    ServiceList broadcast_service_list;
};


#endif
