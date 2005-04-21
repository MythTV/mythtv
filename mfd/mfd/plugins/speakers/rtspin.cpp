/*
	rtspin.cpp

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
    Methods for the thread that actually receives rtsp and outputs to audio
	device
*/

#include <iostream>
using namespace std;

#include <liveMedia.hh>
#include <GroupsockHelper.hh>
#include <BasicUsageEnvironment.hh>


#include "speakers.h"
#include "rtspin.h"
#include "livemedia.h"

extern "C" void afterPlayingCallback(void* clientData)
{
    RtspIn *rtsp_in_object = (RtspIn *)clientData;
    rtsp_in_object->handleAfterPlaying();
}

extern "C" void subsessionByeCallback(void* clientData)
{
    RtspIn *rtsp_in_object = (RtspIn *)clientData;
    rtsp_in_object->handleByeCallback();
}

/*
---------------------------------------------------------------------
*/

RtspIn::RtspIn(Speakers *owner, const QString &l_rtsp_url)
{
    parent= owner;
    keep_going = true;
    rtsp_url = l_rtsp_url;
    
    scheduler = NULL;
    env = NULL;
    rtsp_client = NULL;
    media_session = NULL;
    sub_session = NULL;
    audio_output_sink = NULL;
}


void RtspIn::run()
{
    //
    //  Most of this is just lifted from liveMedia test programs,
    //  specifically playCommon and openRTSP
    //


    //
    //  Setup the liveMedia task scheduler and usage environment. Note, this
    //  things all run in a single thread (ie. this QThread).
    //

    scheduler = LiveTaskScheduler::createNew();
    env = BasicUsageEnvironment::createNew(*scheduler);

    //
    //  Create the liveMedia RTSP client
    //

    rtsp_client = RTSPClient::createNew(
                                        *env, 
                                        0,  // Change to 1 for console debugging
                                        "MythTV speakers"
                                       );

    //
    //  Do a DESCRIBE request, which sets some internal stuff in the
    //  liveMedia RTSPClient and returns a char* description of the SDP
    //

    char *sdp_description = rtsp_client->describeURL(rtsp_url);

    //
    //  Using the returned sdp description, create a liveMedia MediaSession
    //    
    
    media_session = MediaSession::createNew(*env, sdp_description);
    delete[] sdp_description;
    
    if (!media_session)
    {
        warning(QString("failed to create a valid media session from this url: %1")
                .arg(rtsp_url));
        cleanUp();
        return;
    }
      
    //
    //  There should be only one subsession (16 by 2 by 44100 PCM data
    //  coming out of the rtspout object in the audio plugin).
    //  
    
    
    MediaSubsessionIterator iter(*media_session);
    sub_session = iter.next();

    if (!sub_session)
    {
        warning(QString("no subsession in SDP from %1, giving up")
                .arg(rtsp_url));
        cleanUp();
        return;
    }

    if (iter.next() != NULL)
    {
        warning(QString("more than one subsession available from %1, "
                        "hoping the first one works").arg(rtsp_url));
    }
    
    //
    //  Initiate the sub session
    //
    
    if (!sub_session->initiate())
    {
        warning(QString("failed to initiate first subsession from "
                        "SDP in %1, giving up")
                        .arg(rtsp_url));
        cleanUp();
        return;
    }

    int socket_fd = sub_session->rtpSource()->RTPgs()->socketNum();
    increaseReceiveBufferTo( *env, socket_fd, 100000 );
    
    //
    //  Setup the subsession
    //

    if(!rtsp_client->setupMediaSubsession(*sub_session, false, true))
    {
        warning(QString("failed to setup first subsession from "
                        "SDP in %1, givinp up")
                        .arg(rtsp_url));
        cleanUp();
        return;
    }
 
    //
    //  Explain what's about to happen, and on which ports
    //
    
    log(QString("about to tune to %1 with media type %2 on ports %3-%4")
        .arg(rtsp_url)
        .arg(sub_session->codecName())
        .arg(sub_session->clientPortNum())
        .arg(sub_session->clientPortNum() + 1)
        , 6);

    //
    //  Make an AudioOutputSink and attach it to the chain (so PCM bits end
    //  up being delivered to it).
    //
    
    audio_output_sink = AudioOutputSink::createNew(*env);
    sub_session->sink = audio_output_sink;
    sub_session->sink->startPlaying(*(sub_session->readSource()), afterPlayingCallback, this);
    sub_session->rtcpInstance()->setByeHandler(subsessionByeCallback, this);
                                  

    //
    //  Ensure playing will occur in the liveMedia event loop
    //
    
    if( ! rtsp_client->playMediaSession(*media_session))
    {
        warning(QString("failed to initiate playback of %1, giving up")
                .arg(rtsp_url));
        cleanUp();
        return;
    }
    
    QTime schedule_timer;
    schedule_timer.start();
    
    while(keep_going)
    {
        //
        //  We time how long it takes for the scheduler to step. If it takes
        //  too long, we know something is wrong (e.g. the source has gone
        //  away). There should be a more elegant way of doing this (?). We
        //  could check the socket_fd I suppose. Anyway, the following does
        //  work, and is perfectly acceptable given this is all supposed to
        //  be happening on a local LAN.
        //
        
        schedule_timer.restart();
        scheduler->stepOnce(5000000);   // at most 5,000,000 microseconds = 5 seconds
        if (schedule_timer.elapsed() > 4000)
        {
            //
            //  Nothing happend for more than 4 seconds, give up
            //
            
            warning("no rtsp activity for more than 5 seconds (source gone?). Giving up.");
            keep_going_mutex.lock();
                keep_going = false;
            keep_going_mutex.unlock();
        }
    }

    cleanUp();
    
    //
    //  Wake up the parent (Speaker object). That will make the parent check
    //  if we are still running, and since we're now quitting, it can delete
    //  us.
    //

    parent->wakeUp();
}

void RtspIn::stop()
{
    keep_going_mutex.lock();
        keep_going = false;
    keep_going_mutex.unlock();
}

void RtspIn::handleAfterPlaying()
{
    //
    //  Whatever was being streamed to us has apparently ended
    //
    
    warning("RtspIn got handleAfterPlaying(), will clean up and unload");
    keep_going_mutex.lock();
        keep_going = false;
    keep_going_mutex.unlock();
}

void RtspIn::handleByeCallback()
{
    //
    //  Server said BYE
    //
    
    warning("RtspIn got BYE from server, will clean up and unload");
    keep_going_mutex.lock();
        keep_going = false;
    keep_going_mutex.unlock();
}

void RtspIn::cleanUp()
{
    if(audio_output_sink)
    {
        Medium::close(audio_output_sink);
        //Medium::close() deletes it
        sub_session->sink = NULL;
        audio_output_sink = NULL;
    }
    if(media_session && rtsp_client)
    {
        rtsp_client->teardownMediaSession(*media_session);
    }
    if(media_session)
    {
        Medium::close(media_session);
        media_session = NULL;
    }
    if(rtsp_client)
    {
        Medium::close(rtsp_client);
    }
    
    if(env)
    {
        env->reclaim();
        env = NULL;
    }
    
    if(scheduler)
    {
        delete scheduler;
        scheduler = NULL; 
    }

}

void RtspIn::warning(const QString &warn_message)
{
    if (parent)
    {
        parent->warning(warn_message);
    }
}

void RtspIn::log(const QString &log_message, int verbosity)
{
    if (parent)
    {
        parent->log(log_message, verbosity);
    }
}

RtspIn::~RtspIn()
{
}
