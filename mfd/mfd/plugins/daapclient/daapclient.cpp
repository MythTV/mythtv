/*
	daapclient.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Methods for a daap content monitor

*/

#include <qhostaddress.h>
#include <qregexp.h>

#include "daapclient.h"

DaapClient::DaapClient(MFD *owner, int identity)
      :MFDServicePlugin(owner, identity, 3689)
{
    client_socket_to_mfd = NULL;
}

void DaapClient::run()
{
    //
    //  Create a client socket and connect to the mfd just like a "normal"
    //  client (although the only data we care about that the mfd will send
    //  us are service announcements). Why do it this way? Well, someone
    //  could write another plugin (not part of this tree) that finds daap
    //  services in another way (say, further away than a zeroconfig hop).
    //  All that plugin has to do is make a ServiceEvent announcement, and
    //  this code here will still be able to find it and figure out what
    //  data/metadata is on offer.
    //

    QHostAddress this_address;
    if(!this_address.setAddress("127.0.0.1"))
    {
        fatal("daap client could not set address for 127.0.0.1");
        return;
    }
    client_socket_to_mfd = new QSocketDevice(QSocketDevice::Stream);
    client_socket_to_mfd->setBlocking(false);
    
    int connect_tries = 0;
    
    while(! (client_socket_to_mfd->connect(this_address, 2342)))
    {
        ++connect_tries;
        if(connect_tries > 10)
        {
            fatal("daap client could not connect to the core mfd as a regular client");
            return;
        }
    }
    

    QString first_and_only_command = "services list";
    client_socket_to_mfd->writeBlock(first_and_only_command.ascii(), first_and_only_command.length());
    
    while(keep_going)
    {

        //
        //  Handle anything incoming on the socket
        //

        readSocket();

        sleep(3);
    }
}

void DaapClient::readSocket()
{

    char incoming[2049];

    int length = client_socket_to_mfd->readBlock(incoming, 2048);
    if(length > 0)
    {
        if(length >= 2048)
        {
            // oh crap
            warning("daap client socket to mfd is getting too much data");
            length = 2048;
        }
        incoming[length] = '\0';
        QString incoming_data = incoming;
        
        incoming_data.simplifyWhiteSpace();

        QStringList line_by_line = QStringList::split("\n", incoming_data);        

        for(uint i = 0; i < line_by_line.count(); i++)
        {
            QStringList tokens = QStringList::split(" ", line_by_line[i]);
            if(tokens.count() >= 7)
            {
                if(tokens[0] == "services" &&
                   tokens[2] == "lan" &&
                   tokens[3] == "daap")
                {
                    QString service_name = tokens[6];
                    for(uint i = 7; i < tokens.count(); i++)
                    {
                        service_name += " ";
                        service_name += tokens[i];
                    }
                    
                    if(tokens[1] == "found")
                    {
                        addDaapServer(tokens[4], tokens[5].toUInt(), service_name);
                    }
                    else if(tokens[1] == "lost")
                    {
                        removeDaapServer(tokens[4], tokens[5].toUInt(), service_name);
                    }
                    else
                    {
                        warning("daap client got a services update where 2nd token was neither \"found\" nor \"lost\"");
                    }
                }
                   
            }
        }
    }
        
}

void DaapClient::addDaapServer(QString server_address, uint server_port, QString service_name)
{
    log(QString("daapclient will add service \"%1\" (%2:%3)")
        .arg(service_name)
        .arg(server_address)
        .arg(server_port), 10);
}

void DaapClient::removeDaapServer(QString server_address, uint server_port, QString service_name)
{
    log(QString("daapclient will remove service \"%1\" (%2:%3)")
        .arg(service_name)
        .arg(server_address)
        .arg(server_port), 10);
}

DaapClient::~DaapClient()
{
    if(client_socket_to_mfd)
    {
        delete client_socket_to_mfd;
        client_socket_to_mfd = NULL;
    }
}

