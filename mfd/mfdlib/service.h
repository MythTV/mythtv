#ifndef SERVICE_H_
#define SERVICE_H_
/*
	service.h

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
    Little object to describe available services

*/

#include <qstring.h>
#include <qhostaddress.h>

enum ServiceLocationDescription
{
    SLT_UNKNOWN = 0,
    SLT_HOST,
    SLT_LAN,
    SLT_NET
};



class Service
{
  public:
    
    Service();

    Service(
                const QString &l_name, 
                const QString &l_type,
                ServiceLocationDescription  l_location,
                const QString &l_hostaddress,
                uint l_port_number
           );
           
    Service(
                const QString &l_name, 
                const QString &l_type,
                const QString &l_hostname,
                ServiceLocationDescription  l_location,
                uint l_port_number
           );
           
    const QString&              getName(){ return name;}
    const QString&              getType(){ return type;}
    QString                     getHostname(){ return hostname; }
    ServiceLocationDescription  getLocation(){ return location; }
    QString                     getAddress();
    QString                     getFormalDescription(bool found_or_lost);
    uint                        getPort(){ return port_number; }

    bool    hostKnown();

    // Service(const Service &other);
    
    bool operator==(const Service &other);
           
    ~Service();


    QString                     name;           // eg. "Steve Job's Music"
    QString                     type;           // eg. daap, http, etc.
    QString                     hostname;       // eg. frontendbox
    ServiceLocationDescription  location;       // eg. host, lan, etc.
    QHostAddress                hostaddress;    // eg. 192.168.1.2
    uint                        port_number;    // eg. 80
};

#endif  // service_h_

