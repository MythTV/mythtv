/*
	serviceclient.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Base class for client lib objects that talk to mfd services

*/

#include <unistd.h>
#include <iostream>
using namespace std;

#include "serviceclient.h"
#include "mfdinterface.h"

ServiceClient::ServiceClient(
                                MfdInterface *the_mfd,
                                int an_mfd,
                                MfdServiceType l_service_type,
                                const QString &l_ip_address,
                                uint l_port
                            )
{
    mfd_interface = the_mfd;
    mfd_id = an_mfd;
    service_type = l_service_type;
    ip_address = l_ip_address;
    port = l_port;
    
    //
    //  We start out unconnected
    //
    
    client_socket_to_service = NULL;
}

bool ServiceClient::connect()
{

    QHostAddress service_address;

    if(!service_address.setAddress(ip_address))
    {
        cerr << "serviceclient.o: can't set address to mfd service" << endl;
        return false;
    }

    client_socket_to_service = new QSocketDevice(QSocketDevice::Stream);
    client_socket_to_service->setBlocking(false);
    
    int connect_tries = 0;
    
    while(! (client_socket_to_service->connect(service_address, port)))
    {
        //
        //  We give this a few attempts. It can take a few on non-blocking sockets.
        //

        ++connect_tries;
        if(connect_tries > 10)
        {
            cerr << "serviceclient.o: could not connect to mfd service "
                 << endl;
            return false;
        }
        usleep(30000);
    }

    return true;
}

int ServiceClient::getSocket()
{
    if(client_socket_to_service)
    {
        return client_socket_to_service->socket();
    }
    return -1;
}

int ServiceClient::bytesAvailable()
{
    if(client_socket_to_service)
    {
        return client_socket_to_service->bytesAvailable();
    }
    return 0;
}

void ServiceClient::handleIncoming()
{
    cerr << "something is calling base class ServiceClient::handleIncoming()"
         << endl;
}

ServiceClient::~ServiceClient()
{
    if(client_socket_to_service)
    {
        delete client_socket_to_service;
        client_socket_to_service = NULL;
    }
}

