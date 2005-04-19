/*
	speakers.cpp

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Methods for the speakers plugin
*/

#include "speakers.h"
#include "rtspin.h"

Speakers::Speakers(MFD *owner, int identity)
      :MFDServicePlugin(owner, identity, 2347, "speakers (maop)")
{
    current_url = "";
    rtsp_in = NULL;
}

void Speakers::run()
{
    //
    //  Get the name of the host we are running on
    //

    char my_hostname[2049];
    QString local_hostname = "unknown";

    if (gethostname(my_hostname, 2048) < 0)
    {
        warning("could not call gethostname()");
    }
    else
    {
        local_hostname = my_hostname;
    }

    //
    //  Have our maop service (Myth Audio Output Protocol) broadcast on
    //  the local lan
    //

    Service *maop_service = new Service(
                                        QString("Myth Audio Output on %1").arg(local_hostname),
                                        QString("maop"),
                                        local_hostname,
                                        SLT_HOST,
                                        (uint) port_number
                                       );
 
    ServiceEvent *se = new ServiceEvent( true, true, *maop_service);
    QApplication::postEvent(parent, se);
    delete maop_service;

    //
    //  Init our sockets
    //
    
    if ( !initServerSocket())
    {
        fatal("could not initialize core server socket");
        return;
    }
    
    while(keep_going)
    {
        //
        //  Update the status of our sockets and wait for something to happen
        //
        
        updateSockets();
        waitForSomethingToHappen();
        checkInternalMessages();
    }

}

void Speakers::doSomething(const QStringList &tokens, int socket_identifier)
{
    bool ok = true;

    if (tokens.count() < 1)
    {
        ok = false;
    }
    else if (tokens.count() == 1)
    {
        if (tokens[0] == "status")
        {
            announceStatus();
        }
        else
        {
            ok = false;
        }
    }
    else 
    {
        if (tokens[0] == "close")
        {
            closeStream(tokens[1]);
        }
        else if (tokens[0] == "open")
        {
            openStream(tokens[1]);
        }
        else
        {
            ok = false;
        }
    }

    if (!ok)
    {
        warning(QString("did not understand or had "
               "problems with these tokens: %1")
               .arg(tokens.join(" ")));
        huh(tokens, socket_identifier);
    }
}

void Speakers::openStream(QString stream_url)
{
    //
    //  If we're told to listen to something we're already listening to,
    //  ignore it
    //

    if (stream_url != current_url)
    {
        log(QString("attempting to open stream on %1").arg(stream_url), 5);
        current_url = stream_url;
        
        if(rtsp_in)
        {
            //
            //  It exists, tell it to switch to new stream
            //
        }
        else
        {
            rtsp_in = new RtspIn(this, current_url);
            rtsp_in->start();
        }
        announceStatus();
    }
}

void Speakers::closeStream(QString stream_url)
{
    //
    //  Only close a connection if we were listening to it
    //
    
    if (stream_url == current_url)
    {
        log("closing connection", 5);
        current_url = "";
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
        else
        {
            warning("asked to close a stream, but don't have one open");
        }
        announceStatus();
    }
}

void Speakers::announceStatus()
{
    if (current_url.length() > 0)
    {
        sendMessage(QString("open %1").arg(current_url));
    }
    else
    {
        sendMessage("closed");
    }
}

Speakers::~Speakers()
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
