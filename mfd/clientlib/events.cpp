/*
	events.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
    events that get passed to mfdinterface

*/

#include "events.h"

MfdDiscoveryEvent::MfdDiscoveryEvent(
                                     bool  is_it_lost_or_found,
                                     const QString &a_hostname, 
                                     const QString &an_ip_address,
                                     int a_port
                                    )
                  :QCustomEvent(MFD_CLIENTLIB_EVENT_DISCOVERY)
{
    lost_or_found = is_it_lost_or_found;
    hostname = a_hostname;
    ip_address = an_ip_address;
    port = a_port;
}

bool MfdDiscoveryEvent::getLostOrFound()
{
    return lost_or_found;
}

const QString& MfdDiscoveryEvent::getHost()
{
    return hostname;
}

const QString& MfdDiscoveryEvent::getAddress()
{
    return ip_address;
}

int MfdDiscoveryEvent::getPort()
{
    return port;
}

