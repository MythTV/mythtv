/*
	service.cpp

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
    Little object to describe available services

*/

#include "service.h"

Service::Service()
{
    name = "";
    type = "";
    location = SLT_UNKNOWN;
    hostname = "";

    ip_address_one   = 0;
    ip_address_two   = 0;
    ip_address_three = 0;
    ip_address_four  = 0;
    
    port_number = 0;
}



Service::Service(
                    const QString &l_name, 
                    const QString &l_type,
                    ServiceLocationDescription  l_location,
                    uint l_ipone,
                    uint l_iptwo,
                    uint l_ipthree,
                    uint l_ipfour,
                    uint l_port_number
                )
{
    name = l_name;
    type = l_type;
    location = l_location;
    hostname = "";

    ip_address_one   = l_ipone;
    ip_address_two   = l_iptwo;
    ip_address_three = l_ipthree;
    ip_address_four  = l_ipfour;
    
    port_number = l_port_number;
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

    ip_address_one   = 0;
    ip_address_two   = 0;
    ip_address_three = 0;
    ip_address_four  = 0;
    
    port_number = l_port_number;
}

QString Service::getAddress()
{
    QString the_address = QString("%1.%2.%3.%4")
                                .arg(ip_address_one)
                                .arg(ip_address_two)
                                .arg(ip_address_three)
                                .arg(ip_address_four);
    
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

    return_string = QString("services %1 %2 %3 %4.%5.%6.%7 %8 %9")
               .arg(found_string)
               .arg(location_string)
               .arg(type)
               .arg(ip_address_one)
               .arg(ip_address_two)
               .arg(ip_address_three)
               .arg(ip_address_four)
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

