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
                uint l_ipone,
                uint l_iptwo,
                uint l_ipthree,
                uint l_ipfour,
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
    uint                        getIpOne(){ return ip_address_one; }
    uint                        getIpTwo(){ return ip_address_two; }
    uint                        getIpThree(){ return ip_address_three; }
    uint                        getIpFour(){ return ip_address_four; }
    uint                        getPort(){ return port_number; }

    bool    hostKnown();

    // Service(const Service &other);
    
    bool operator==(const Service &other);
           
    ~Service();


    QString                     name;       // eg. "Steve Job's Music"
    QString                     type;       // eg. daap, http, etc.
    QString                     hostname;   // eg. frontendbox
    ServiceLocationDescription  location;   // eg. host, lan, etc.
    uint                        ip_address_one;     
    uint                        ip_address_two;
    uint                        ip_address_three;
    uint                        ip_address_four;
    uint                        port_number;
};

#endif  // service_h_

