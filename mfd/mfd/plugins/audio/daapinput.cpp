/*
	daapinput.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Magical little class that makes remote daap resources look like local
	direct access files
*/

#include <time.h>
#include <iostream>
using namespace std;

#include <qhostaddress.h>

#include "daapinput.h"


//
//  We need to do a lot here 
//

DaapInput::DaapInput(QUrl a_url)
          :QSocketDevice(QSocketDevice::Stream)
{

    cout << "I got as far as existing" << endl;
    all_is_well = true;
    my_url = a_url;

    connected = false;

    //
    //  parse out my url
    //
    
    if(my_url.protocol() != "daap")
    {
        all_is_well = false;
    }

    my_host = my_url.host();
    my_port = my_url.port();
    
    setBlocking(false);
}

bool DaapInput::open(int)
{

    QHostAddress host_address;
    if(!host_address.setAddress(my_host))
    {
        all_is_well = false;
        return false;
    }
    
    int connect_tries = 0;
    
    while(! (connect(host_address, my_port)))
    {
        //
        //  We give this a few attempts. It can take a few on non-blocking sockets.
        //

        ++connect_tries;
        if(connect_tries > 10)
        {
            all_is_well = false;
            return false;
        }
        
        //
        //  Sleep for a bit
        //

        struct timespec timeout;
        timeout.tv_sec = 0;
        timeout.tv_nsec = 1000000;
        nanosleep(&timeout, NULL);
        
    }
    
    cout << "okey dokey I connected!" << endl;

    return true;
}

int DaapInput::readBlock( char *data, uint maxlen )
{
    return 1;
}

/*
bool DaapInput::isDirectAccess()
{
    return true;
}
*/

uint DaapInput::size()
{
    return 4;
}

int DaapInput::at()
{
    return 5;
}

bool DaapInput::at( int )
{
    return true;
}

void DaapInput::close()
{
}

bool DaapInput::isOpen()
{
    return false;
}

int DaapInput::status()
{
    return 1;
}

DaapInput::~DaapInput()
{
}