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
    //  Init our sockets
    //
    
    if ( !initServerSocket())
    {
        fatal("could not initialize core server socket");
        return;
    }
    
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

    while(keep_going)
    {
        //
        //  Update the status of our sockets and wait for something to happen
        //
        
        updateSockets();
        waitForSomethingToHappen();
        checkInternalMessages();
        
        //
        //  Check the rtsp_in object, as it might have detected a problem
        //  (e.g. closed RTSP source) and shut itse;f down.
        //
        
        rtsp_in_mutex.lock();

        if (rtsp_in)
        {
            if(! rtsp_in->running())
            {
                warning(QString("low level RTSP/rtp communications appears to "
                                "have stopped. Will stop listening to  %1")
                                .arg(current_url));
                rtsp_in_mutex.unlock();
                closeStream();
                rtsp_in_mutex.lock();
            }
        }
        
        
        rtsp_in_mutex.unlock();
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
        else if (tokens[0] == "close")
        {
            closeStream();
        }
        else
        {
            ok = false;
        }
    }
    else 
    {
        if (tokens[0] == "open")
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
    rtsp_in_mutex.lock();

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
            //  It exists (listening to something else), delete it and make a new one
            //
            
            while(rtsp_in->running())
            {
                rtsp_in->stop();
                usleep(200);
            }
            
            delete rtsp_in;
            rtsp_in = NULL;
        }

        rtsp_in = new RtspIn(this, current_url);
        rtsp_in->start();
        announceStatus();
    }
    rtsp_in_mutex.unlock();
}

void Speakers::closeStream()
{
    rtsp_in_mutex.lock();
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
    announceStatus();
    rtsp_in_mutex.unlock();
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
