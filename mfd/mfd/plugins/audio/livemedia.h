#ifndef LIVEMEDIA_H_
#define LIVEMEDIA_H_
/*
    livemedia.h

    (c) 2005 Thor Sigvaldason and Isaac Richards
    Part of the mythTV project
    
    This is based on example code from the live media library which is:
    
    Copyright (c) 1996-2000 Live Networks, Inc.  All rights reserved.
    
*/

#ifdef MFD_RTSP_SUPPORT

#include <deque>
using namespace std;

#include <OnDemandServerMediaSubsession.hh>
#include <FramedSource.hh>
#include <BasicUsageEnvironment.hh>

class AudioOutput;

//
//  This is where all the action really happens. This thing always outputs
//  PCM audio data when it's asked for the next frame of data. As far as
//  liveMedia is concerned, it is an endless source of PCM data. The rest of
//  the mfd's audio plugin uses this object to get a file/url/cd/whatever to
//  get decoded and thus pumped out the RTSP server (which is only serving
//  PCM!). You might call it a UniversalPCMSource
//


class UniversalPCMSource: public FramedSource
{

  public:

    static UniversalPCMSource* createNew(UsageEnvironment& env, AudioOutput& ao);
    virtual void doGetNextFrame();
    void serveOutActualAudio();
    void serveOutSilence();

  private:

    UniversalPCMSource(UsageEnvironment& env, AudioOutput& ao);
    ~UniversalPCMSource();
    unsigned fLastPlayTime;    

    QMutex       audio_output_mutex;
    AudioOutput &audio_output;
    unsigned     preferred_frame_size;
};


//
//  This is our subsession object, which the LiveMedia calls on when clients
//  do something (connect, push stop, etc.).
//

class LiveSubsession: public OnDemandServerMediaSubsession
{

public:

    static LiveSubsession* createNew(UsageEnvironment& env, AudioOutput& ao);
    UniversalPCMSource* getUniversalPCMSource(){ return universal_pcm_source; }

protected:

    virtual FramedSource* createNewStreamSource(
                                                unsigned clientSessionId, 
                                                unsigned& estBitrate
                                               );
    virtual RTPSink* createNewRTPSink(
                                        Groupsock* rtpGroupsock,
                                        unsigned char rtpPayloadTypeIfDynamic,
                                        FramedSource* inputSource
                                     );
private:

    LiveSubsession(UsageEnvironment& env, AudioOutput& ao);
    ~LiveSubsession();  

    UniversalPCMSource* universal_pcm_source;      
    AudioOutput& audio_output;
};



#endif
#endif
