#ifndef RTSPOUT_H_
#define RTSPOUT_H_

#include "../../../config.h"

#ifdef MFD_RTSP_SUPPORT

/*
	rtspout.h

	(c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for an rtsp/rtp server that sends out WAV/PCM data of whatever
	is currently being played by the audio plugin.
*/

#include "rtspserver.h"

class LiveSubsession;
class UniversalPCMSource;
class AudioOutput;

class RtspOut: public MFDRtspPlugin
{

  public:

    RtspOut(MFD *owner, int identity, AudioOutput *ao);
    ~RtspOut();

    void run();
    void stop();
    QString getUrl(){ return serving_url;  }

  private:

    char            watch_variable;
    LiveSubsession *live_subsession;
    AudioOutput    *audio_output;
    QString         serving_url;
};

#endif  // MFD_RTSP_SUPPORT
#endif  // rtspout_h_
