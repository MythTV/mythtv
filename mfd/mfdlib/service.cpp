/*
	service.cpp

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
    Little object to describe available services

*/

#include <iostream>
using namespace std;

#include "service.h"

Service::Service()
{
    name = "";
    type = "";
    location = SLT_UNKNOWN;
    hostname = "";
    hostaddress.setAddress("0.0.0.0");
    port_number = 0;
}



Service::Service(
                    const QString &l_name, 
                    const QString &l_type,
                    ServiceLocationDescription  l_location,
                    const QString &l_hostaddress,
                    uint l_port_number
                )
{
    name = l_name;
    type = l_type;
    location = l_location;
    hostname = "";
    port_number = l_port_number;

    //
    //  Set the host address
    //
    
    if ( !hostaddress.setAddress(l_hostaddress))
    {
        cerr << "Service::Service() could not create a new "
             << "service called \"" 
             << name
             << "\" with a host address of "
             << l_hostaddress
             << endl;
    }
}


Service::Service(
                    const QString &l_name, 
                    const QString &l_type,
                    const QString &l_hostname,
                    ServiceLocationDescription  l_location,
                    uint l_port_number
                )
{
    name = l_name;
    type = l_type;
    hostname = l_hostname;
    location = l_location;
    port_number = l_port_number;
    hostaddress.setAddress("0.0.0.0");
}

QString Service::getAddress()
{

    if (hostaddress.toIPv4Address() == 0)
    {
        cerr << "Service::getAddress() is about to return "
             << "an address of 0.0.0.0 for a service called \""
             << name
             << "\""
             << endl;
    }
 
    QString the_address = hostaddress.toString();
    return the_address;
}

QString Service::getFormalDescription(bool found_or_lost)
{
    QString return_string;
    
    //
    //  Build a string that can get passed to a client using the mfd
    //  protocol.
    //
    
    QString location_string;
    QString found_string = "found";
    if(!found_or_lost)
    {
        found_string = "lost";
    }
    
    switch(location)
    {
        case SLT_HOST:
            location_string = "host";
            break;

        case SLT_LAN:
            location_string = "lan";
            break;

        case SLT_NET:
            location_string = "net";
            break;

        default:
            location_string = "unknown";
    }

    return_string = QString("services %1 %2 %3 %4 %5 %6")
               .arg(found_string)
               .arg(location_string)
               .arg(type)
               .arg(hostaddress.toString())
               .arg(port_number)
               .arg(name);
               

    return return_string;
}

bool Service::hostKnown()
{
    if(hostname.length() > 0)
    {
        return true;
    }
    return false;
}

bool Service::operator==(const Service &other)
{
    //
    //  We ignore the name (what with utf8 and ascii, that can get a bit
    //  mangled)
    //
    
    if( type        == other.type     &&
        hostname    == other.hostname &&
        location    == other.location &&
        port_number == other.port_number )
    {
        return true;
    }
    return false;
}

Service::~Service()
{
}

