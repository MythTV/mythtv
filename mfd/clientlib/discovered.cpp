/*
	discovered.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Small object to keep track of the availability of mfd's

*/


#include <unistd.h>
#include <iostream>
using namespace std;
#include <netdb.h>

#include "discovered.h"

DiscoveredMfd::DiscoveredMfd(QString l_full_service_name)
{
    full_service_name = l_full_service_name;
    hostname = "";
    port_resolved = false;
    ip_resolved = false;
    time_to_live = 0;
    client_socket_to_mfd = NULL;
    port = 0;
    address = "";
}

int DiscoveredMfd::getSocket()
{
    if(client_socket_to_mfd)
    {
        return client_socket_to_mfd->socket();
    }
    return -1;   
}

bool DiscoveredMfd::connect()
{


    QHostAddress this_address;

    if(!this_address.setAddress(getAddress()))
    {
        cerr << "discovered.o: can't set host address" << endl;
        return false;
    }

    client_socket_to_mfd = new QSocketDevice(QSocketDevice::Stream);
    client_socket_to_mfd->setBlocking(false);
    
    int connect_tries = 0;
    
    while(! (client_socket_to_mfd->connect(this_address, port)))
    {
        //
        //  We give this a few attempts. It can take a few on non-blocking sockets.
        //

        ++connect_tries;
        if(connect_tries > 10)
        {
            cerr << "could not connect to mfd on \""
                 << hostname
                 << "\""
                 << endl;
                            
            return false;
        }
        usleep(3000);
    }
    
    return true;
}

DiscoveredMfd::~DiscoveredMfd()
{
    if(client_socket_to_mfd)
    {
        delete client_socket_to_mfd;
        client_socket_to_mfd = NULL;
    }
}
