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

RtspIn::RtspIn(Speakers *owner, const QString &l_rtsp_url)
{
    parent= owner;
    keep_going = true;
    rtsp_url = l_rtsp_url;
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

    LiveTaskScheduler *scheduler = LiveTaskScheduler::createNew();
    UsageEnvironment *env = BasicUsageEnvironment::createNew(*scheduler);

    //
    //  Make note of what time it is now
    //

    struct timeval startTime;
    gettimeofday(&startTime, NULL);
      
    //
    //  Create the liveMedia RTSP client
    //

    RTSPClient *rtsp_client = RTSPClient::createNew(*env);

    //
    //  Do a DESCRIBE request, which sets some internal stuff in the
    //  liveMedia RTSPClient and returns a char* description of the SDP
    //

    char *sdp_description = rtsp_client->describeURL(rtsp_url);

    //
    //  Using the returned sdp description, create a liveMedia MediaSession
    //    
    
    MediaSession *media_session = MediaSession::createNew(*env, sdp_description);
    delete[] sdp_description;
    
    if (!media_session)
    {
        warning(QString("failed to create a valid media session from this url: %1")
                .arg(rtsp_url));
        return;
    }
      
    //
    //  There should be only one subsession (16 by 2 by 44100 PCM data
    //  coming out of the rtspout object in the audio plugin).
    //  
    
    
    MediaSubsessionIterator iter(*media_session);
    MediaSubsession *sub_session;
    
    sub_session = iter.next();

    if (!sub_session)
    {
        warning(QString("no subsession in SDP from %1, giving up")
                .arg(rtsp_url));
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
        return;
    }

    int fd = sub_session->rtpSource()->RTPgs()->socketNum();
    increaseReceiveBufferTo( *env, fd, 100000 );
    
   
    //
    //  Setup the subsession
    //

    if(!rtsp_client->setupMediaSubsession(*sub_session, false, true))
    {
        warning(QString("failed to setup first subsession from "
                        "SDP in %1, givinp up")
                        .arg(rtsp_url));
        return;
    }
    else
    {
                                        
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
    
    AudioOutputSink *audio_output_sink = AudioOutputSink::createNew(*env);
    sub_session->sink = audio_output_sink;
    sub_session->sink->startPlaying(*(sub_session->readSource()), NULL, NULL);

    //
    //  Ensure playing will occur in the liveMedia event loop
    //
    
    if( ! rtsp_client->playMediaSession(*media_session))
    {
        warning(QString("failed to initiate playback of %1, giving up")
                .arg(rtsp_url));
        return;
    }
    
    while(keep_going)
    {
        scheduler->stepOnce(2000000);   // at most 2,000,000 microseconds = 2 seconds
    }

}

void RtspIn::stop()
{
    keep_going_mutex.lock();
        keep_going = false;
    keep_going_mutex.unlock();
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
