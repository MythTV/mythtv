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
    resolved = false;
    time_to_live = 0;
    client_socket_to_mfd = NULL;
    port = 0;
}

int DiscoveredMfd::getSocket()
{
    if(client_socket_to_mfd)
    {
        return client_socket_to_mfd->socket();
    }
    return -1;   
}

QString DiscoveredMfd::getAddress()
{

    if(hostname.length() < 1)
    {
        cerr << "discovered.o: can't calculate address with no hostname"
             << endl;
        return QString("");

    }

    hostent *host_address = gethostbyname(hostname.ascii());

    if(host_address->h_length == 4)
    {
        int ip1 = (int)( (unsigned char) host_address->h_addr[0]);
        int ip2 = (int)( (unsigned char) host_address->h_addr[1]);
        int ip3 = (int)( (unsigned char) host_address->h_addr[2]);
        int ip4 = (int)( (unsigned char) host_address->h_addr[3]);
        
        QString ip_address = QString("%1.%2.%3.%4")
                                    .arg(ip1)
                                    .arg(ip2)
                                    .arg(ip3)
                                    .arg(ip4);
        return ip_address;
    }
    else
    {
        cerr << "discovered.o: got weird size for host address"
             << endl;
    }
    return QString("");
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
