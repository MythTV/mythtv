/*
	rtspout.cpp

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Methods for an rtsp/rtp server that sends out WAV/PCM data of whatever
	is currently being played by the audio plugin. Intended for client side
	visualization, but could conceivably be used to make clients that tune
	into the "Live" RTP broadcast.
*/

#include "../../../config.h"
#ifdef MFD_RTSP_SUPPORT

#include <mythtv/audiooutput.h>

#include <liveMedia.hh>
#include <GroupsockHelper.hh>
#include <BasicUsageEnvironment.hh>

#include "rtspout.h"
#include "livemedia.h"
#include "settings.h"

RtspOut::RtspOut(MFD *owner, int identity, AudioOutput *ao)
      :MFDRtspPlugin(owner, identity, mfdContext->getNumSetting("mfd_audiortspcontrol_port"), "audio rtsp server")
{
    log("rtsp sub-plugin came into existence", 9);
    watch_variable = 0;
    live_subsession = NULL;
    audio_output = ao;
}

void RtspOut::stop()
{
    keep_going_mutex.lock();
        keep_going = false;
        watch_variable = 1;
        
        //
        //  There is a bug in some versions of live media where timing on
        //  the select is mucked up. If we poke the server socket now
        //  (having set watch_variable to non-zero), it will exit
        //

        bool output_warning = false;

        QHostAddress this_host;
        if(this_host.setAddress("127.0.0.1"))
        {
            QSocketDevice poker;
            poker.setBlocking(false);
            if(!poker.connect(this_host, mfdContext->getNumSetting("mfd_audiortsp_port")))
            {
                output_warning = true;
            }
        }
        else
        {
            output_warning = true;
        }

        if(output_warning)
        {
            cerr << "rtspout.o: Failed to workaround a shutdown bug in "
                 << "liveMedia. You'll probably have to kill -KILL this "
                 << "mfd by hand."
                 << endl;
        }

    keep_going_mutex.unlock();
}


void RtspOut::run()
{

    //
    //  Register this (rtsp) service
    //

    int actual_rtsp_port = mfdContext->getNumSetting("mfd_audiortsp_port");

    serving_url = QString("rtsp://%1:%2/").arg(hostname).arg(actual_rtsp_port);

    Service *rtsp_service = new Service(
                                        QString("Myth Audio Streaming on %1").arg(hostname),
                                        QString("rtsp"),
                                        hostname,
                                        SLT_HOST,
                                        (uint) actual_rtsp_port
                                       );
 
    ServiceEvent *se = new ServiceEvent( true, true, *rtsp_service);
    QApplication::postEvent(parent, se);
    delete rtsp_service;

    BasicTaskScheduler* scheduler = BasicTaskScheduler::createNew();
    UsageEnvironment *env = BasicUsageEnvironment::createNew(*scheduler);

    RTSPServer* rtspServer = RTSPServer::createNew(*env, actual_rtsp_port, NULL);
    if (rtspServer == NULL)
    {
        warning(QString("failed to create an RTSP server: %1")
                .arg(env->getResultMsg()));
        return;
    }

    ServerMediaSession *sms = ServerMediaSession::createNew(*env);
    live_subsession = LiveSubsession::createNew(*env, *audio_output);
    sms->addSubsession(live_subsession);
    rtspServer->addServerMediaSession(sms);

    //
    //  See stop() (above) for how we get out of this
    //

    env->taskScheduler().doEventLoop(&watch_variable);
}


RtspOut::~RtspOut()
{
    log("rtsp sub-plugin is being snuffed out", 9);
}

#endif
