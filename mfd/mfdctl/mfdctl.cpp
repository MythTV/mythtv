/*
	mfdctl.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	tiny little program to start, stop, restart, 
	reload the mfd

*/

#include <unistd.h>

#include <qregexp.h>

#include "mfdctl.h"

MFDCTL::MFDCTL(int port_number, int logging_verbosity, QString host_name, QString command)
{
    keep_running = true;
    problems = false;
    port = 2342;
    if(port_number > 0)
    {
        port = port_number;
    }
    
    verbosity = logging_verbosity;
    if(verbosity < 1)
    {
        verbosity = 1;
    }
    if(verbosity > 10)
    {
        verbosity = 10;
    }
    host = host_name;
    mode = command;
    if(mode != "reload" &&
       mode != "stop" &&
       mode != "restart")
    {
        cerr << "the mfdctl object does know how to do \"" << mode << "\"" << endl;
        keep_running = false;
    }    

    my_client_socket = new QSocket(this);
    
    //
    //  Connect the 'friggen event driven socket's signals
    //

    connect( my_client_socket, SIGNAL(connected()),
             this, SLOT(socketConnected()) );
    connect( my_client_socket, SIGNAL(error(int)),
             this, SLOT(socketError(int)) );
    connect( my_client_socket, SIGNAL(readyRead()),
             this, SLOT(readFromServer()) );
      
    //
    // connect to the mfd 
    //

    my_client_socket->connectToHost( host, port );

}

void MFDCTL::socketConnected()
{
    if(mode == "stop" || mode =="restart")
    {
        QTextStream os(my_client_socket);
        os << "shutdown \n";
    }
    else if(mode == "reload")
    {
        QTextStream os(my_client_socket);
        os << "reload \n";
    }
}

void MFDCTL::socketError(int)
{
    problems = true;
    keep_running = false;
}

void MFDCTL::readFromServer()
{
    QString incoming;
    if( my_client_socket->canReadLine())
    {
        incoming = my_client_socket->readLine();
        incoming = incoming.replace( QRegExp("\n"), "" );
        incoming = incoming.replace( QRegExp("\r"), "" );
        incoming.simplifyWhiteSpace();

        if(mode == "stop")
        {
            if(incoming == "bye")
            {
                keep_running = false;
                return;
            }
        }
        if(mode == "restart")
        {
            if(incoming == "bye")
            {
                //
                //  sleep for 30 seconds ... a stupid hack (FIX), but
                //  without a .pid somewhere (for which we must be root),
                //  not sure how else to do this
                //
                
                cout << "this will take at least 30 seconds ..." << endl;
                usleep(30000000);

                keep_running = false;
                return;
            }
        }
        else if(mode == "reload")
        {
            if(incoming == "restart")
            {
                keep_running = false;
                return;
            }
        }
    }
    cout << "Wasn't expecting to get this from the server: \"" << incoming << "\"" << endl;
}


MFDCTL::~MFDCTL()
{
    cout << "mfdctl is being delete" << endl;
    if(my_client_socket)
    {
        delete my_client_socket;
        my_client_socket = NULL;
    }
}
