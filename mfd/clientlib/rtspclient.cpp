/*
	rtspclient.cpp

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	client object to talk to an mfd's music rtsp server

*/

#include <iostream>
using namespace std;

#include <unistd.h>

#include "rtspclient.h"
#include "rtspin.h"
#include "mfdinterface.h"

RtspClient::RtspClient(
                            MfdInterface *the_mfd,
                            int an_mfd,
                            const QString &l_ip_address,
                            uint l_port
                        )
            :ServiceClient(
                            the_mfd,
                            an_mfd,
                            MFD_SERVICE_RTSP_CLIENT,
                            l_ip_address,
                            l_port,
                            "rtsp"
                          )
{
    rtsp_in = NULL;
}

bool RtspClient::connect()
{
    //
    //  We do nothing here, as we don't want to connect to anything unless
    //  there is actually some audio playing. That's handled by the
    //  audioclient object, that sees messages from the mfd audio plugin
    //  (e.g. "speakerstream on"), fires off an MfdSpeakerStreamEvent, and
    //  then the mfdinterface catches that and asks this object to connect
    //  (ie. do turnOn()).
    //
    return true;
}

void RtspClient::turnOn()
{
    if(! rtsp_in)
    {
        rtsp_in = new RtspIn(
                                mfd_interface, 
                                mfd_id, 
                                QString("rtsp://%1:%2/").arg(ip_address).arg(port), 
                                mfd_interface->getRegisteredVisualization()
                            );
        rtsp_in->start();
    }
}

void RtspClient::turnOff()
{
    if(rtsp_in)
    {
        while(rtsp_in->running())
        {
            rtsp_in->stop();
            usleep(200);
        }
        delete rtsp_in;
        rtsp_in = NULL;
    }
}

void RtspClient::executeCommand(QStringList new_command)
{
    if (new_command.count() < 1)
    {
        cerr << "rtspclient.o: asked to executeCommand(), "
             << "but got no commands."
             << endl;
        return;
    }
    
    if (new_command[0] == "on")
    {
        turnOn();
    }
    else if (new_command[0] == "off")
    {
        turnOff();
    }
    else
    {
        cerr << "rtspclient.o: I don't understand this "
             << "command: \""
             << new_command.join(" ")
             << "\""
             << endl;
    }
    
}


RtspClient::~RtspClient()
{
    if(rtsp_in)
    {
        while(rtsp_in->running())
        {
            rtsp_in->stop();
            usleep(200);
        }
        delete rtsp_in;
        rtsp_in = NULL;
    }
}




