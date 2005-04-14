/*
	maopinstance.cpp

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Methods for an object that knows how to talk maop (to mfd speaker plugins)

*/

#include <iostream>
using namespace std;

#include <qregexp.h>

#include "maopinstance.h"
#include "audio.h"

MaopInstance::MaopInstance(AudioPlugin *owner, QString l_name, QString l_address, uint l_port)
{
    parent = owner;
    name = l_name;
    address = l_address;
    port = l_port;
    client_socket_to_maop_service = NULL;
    socket_fd = -1;
    current_url = "";
    currently_open = false;
    
    all_is_well = true;
    
    //
    //  Get a connection to this maop service
    //
    
    QHostAddress maop_server_address;
    if (!maop_server_address.setAddress(address))
    {
        warning(QString("maop instance could not "
                        "set maop service's address "
                        "of %1")
                        .arg(address));
        all_is_well = false;
        return;
    }

    client_socket_to_maop_service = new QSocketDevice(QSocketDevice::Stream);
    client_socket_to_maop_service->setBlocking(false);


    int connect_tries = 0;
    while(!(client_socket_to_maop_service->connect(maop_server_address, 
                                                  port)))
    {
        //
        //  Try a few times (it's a non-blocking socket)
        //
        ++connect_tries;
        if (connect_tries > 10)
        {
            warning(QString("maop instance could not "
                            "connect to maop service %1:%2")
                            .arg(address)
                            .arg(port));
            all_is_well = false;
            return;
        }
        usleep(100);
    }

    log(QString("maop instance made initial "
                "connection to \"%1\" (%2:%3)")
                .arg(name)
                .arg(address)
                .arg(port), 4);

    //
    //  Prime the pump by sending a status command
    //
    
    sendRequest("status");
    
    //
    //  Keep track of the file descriptor for this socket
    //
    
    socket_fd = client_socket_to_maop_service->socket();
}

bool MaopInstance::isThisYou(QString l_name, QString l_address, uint l_port)
{
    bool same_thing = true;
    
    if (l_name != name)
    {
        same_thing = false;
    }
    
    if (l_address != address)
    {
        same_thing = false;
    }
    
    if (l_port != port)
    {
        same_thing = false;
    }
    
    return same_thing;
}

void MaopInstance::checkIncoming()
{
    //
    //  See if there's anything coming in from the actual maop service
    //
    
    if (client_socket_to_maop_service->bytesAvailable())
    {
        char incoming[4096];
        int length = client_socket_to_maop_service->readBlock(incoming, 4096 - 1);

        if (length > 0)
        {
            if (length >= 4096)
            {
                // oh crap
                warning("maop instance is getting too much data");
                length = 4096;
            }
    
            incoming[length] = '\0';

            QString incoming_data = incoming;
            incoming_data = incoming_data.replace( QRegExp("\n"), "" );
            incoming_data = incoming_data.replace( QRegExp("\r"), "" );
            incoming_data.simplifyWhiteSpace();

            QStringList tokens = QStringList::split(" ", incoming_data);
            
            if (tokens.count() > 0)
            {
                if (tokens[0] == "closed")
                {
                    current_url = "";
                    currently_open = false;
                    log(QString("maop instance noticed that \"%1\" "
                                "is now closed")
                                .arg(name), 9);
                }
                else if (tokens[0] == "open")
                {
                    if(tokens.count() > 1)
                    {
                        currently_open = true;
                        current_url = tokens[1];
                        log(QString("maop instance noticed that \"%1\" "
                                    "is now listening to %2")
                                    .arg(name)
                                    .arg(current_url), 9);
                    }
                    else
                    {
                        warning("maop service said it was open, but gave no URL ???");
                        currently_open = false;
                        current_url = "";
                    }
                }
            }
        }
    }
}

void MaopInstance::sendRequest(const QString &the_request)
{
    QString newlined_request = the_request;
    newlined_request.append("\n");
    client_socket_mutex.lock();
        int length = client_socket_to_maop_service->writeBlock(newlined_request.ascii(), newlined_request.length());
    client_socket_mutex.unlock();
    if (length < 0)
    {
        warning("maop instance could not talk to maop service");
        all_is_well = false;
    }
    else if (length != (int) newlined_request.length())
    {
        warning("maop instance could not send correct amount of data to maop service");
        all_is_well = false;
    }
}


void MaopInstance::log(const QString &log_message, int verbosity)
{
    if (parent)
    {
        parent->log(log_message, verbosity);
    }
}

void MaopInstance::warning(const QString &warn_message)
{
    if (parent)
    {
        parent->warning(warn_message);
    }
}

MaopInstance::~MaopInstance()
{
    if (client_socket_to_maop_service)
    {
        delete client_socket_to_maop_service;
        client_socket_to_maop_service = NULL;
    }
}



