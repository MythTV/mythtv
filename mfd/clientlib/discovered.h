#ifndef DISCOVERED_H_
#define DISCOVERED_H_
/*
	discovered.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Small object to keep track of the availability of mfd's

*/

#include <qstring.h>
#include <qsocketdevice.h>


class DiscoveredMfd
{

  public:

    DiscoveredMfd(QString l_full_service_name);
    ~DiscoveredMfd();

    bool    connect();
    
    QString getFullServiceName(){return full_service_name;}
    
    void    setTimeToLive(int an_int){time_to_live = an_int;}
    int     getTimeToLive(){return time_to_live;}

    bool    isPortResolved(){return port_resolved;}
    void    isPortResolved(bool y_or_n){port_resolved = y_or_n;}   

    bool    isIpResolved(){return ip_resolved;}
    void    isIpResolved(bool y_or_n){ip_resolved = y_or_n;}   

    QString getHostName(){return hostname;}
    void    setHostName(QString a_host){hostname = a_host;}
    
    QString getAddress(){return address;}
    void    setAddress(QString an_address){ address = an_address;}

    int     getPort(){return port;}
    void    setPort(int a_port){port = a_port;}

    int     getSocket();
    QSocketDevice* getSocketDevice(){return client_socket_to_mfd;}
    
  private:
  
    QString       address;
    QString       full_service_name;
    QString       hostname;
    bool          port_resolved;
    bool          ip_resolved;
    int           time_to_live;
    int           port;
    QSocketDevice *client_socket_to_mfd;

};

#endif
