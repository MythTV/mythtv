#ifndef EVENTS_H_
#define EVENTS_H_
/*
	events.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
    events that get passed to mfdinterface

*/

#include <qevent.h>

#define MFD_CLIENTLIB_EVENT_DISCOVERY 65432


class MfdDiscoveryEvent: public QCustomEvent
{
    //
    //  Sent by discoverythread when it finds a new mfd
    //

  public:
  
    MfdDiscoveryEvent(
                        bool  is_it_lost_or_found,
                        const QString &a_hostname, 
                        const QString &an_ip_address,
                        int a_port
                     );

    bool            getLostOrFound();
    const QString&  getHost();
    const QString&  getAddress();
    int             getPort();
    
  private:
  
    bool    lost_or_found;
    QString hostname;
    QString ip_address;
    int     port;
    
};



#endif
