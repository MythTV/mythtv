#ifndef RTSPIN_H_
#define RTSPIN_H_
/*
	rtspin.h

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for the thread that actually receives rtsp and outputs to audio
	device
*/

#include <qthread.h>
#include <qmutex.h>
#include <qstring.h>

class Speakers;
class LiveTaskScheduler;
class UsageEnvironment;
class RTSPClient;
class MediaSession;
class MediaSubsession;
class AudioOutputSink;

class RtspIn: public QThread
{

  public:

    RtspIn(Speakers *owner, const QString &l_rtsp_url);
    ~RtspIn();

    void run();
    void stop();

  private:

    void    cleanUp();
    void    warning(const QString &warn_message);
    void    log(const QString &log_message, int verbosity_level);
    
    
    bool        keep_going;
    QMutex      keep_going_mutex;
    QString     rtsp_url;
    Speakers   *parent;
    
    //
    //  liveMedia objects
    //
    
    LiveTaskScheduler   *scheduler;
    UsageEnvironment    *env;
    RTSPClient          *rtsp_client;
    MediaSession        *media_session;
    MediaSubsession     *sub_session;
    AudioOutputSink     *audio_output_sink;
};

#endif  // rtspin_h_
