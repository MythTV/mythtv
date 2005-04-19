/*
    livemedia.cpp

    (c) 2005 Thor Sigvaldason and Isaac Richards
    Part of the mythTV project
    
    This is based on example code from the live media library which is:
    
      Copyright (c) 1996-2000 Live Networks, Inc.  All rights reserved.
    
*/

#include <iostream>
using namespace std;

#include <mythtv/audiooutput.h>
#include <SimpleRTPSink.hh>
#include <uLawAudioFilter.hh>
#include <GroupsockHelper.hh>
#include "livemedia.h"

UniversalPCMSource* UniversalPCMSource::createNew(UsageEnvironment& env, AudioOutput& ao)
{
    return new UniversalPCMSource(env, ao);
}

void UniversalPCMSource::doGetNextFrame()
{
    if( preferred_frame_size < fMaxSize) 
    {
        fMaxSize = preferred_frame_size;
    }
          
    audio_output_mutex.lock();
        serveOutActualAudio();
    audio_output_mutex.unlock();

    if (fPresentationTime.tv_sec == 0 && fPresentationTime.tv_usec == 0)
    {
        gettimeofday(&fPresentationTime, NULL);
    }
    else
    {
        unsigned uSeconds   = fPresentationTime.tv_usec + fLastPlayTime;
        fPresentationTime.tv_sec += uSeconds/1000000;
        fPresentationTime.tv_usec = uSeconds%1000000;
    }
    
//  double fPlayTimePerSample = 1e6/(double)fSamplingFrequency;
    double fPlayTimePerSample = 1e6/44100.0;
    
    fDurationInMicroseconds = fLastPlayTime
        = (unsigned)((fPlayTimePerSample*fFrameSize)/4);

    nextTask() = envir().taskScheduler().scheduleDelayedTask(0,
            (TaskFunc*)FramedSource::afterGetting, this);        
}

void UniversalPCMSource::serveOutActualAudio()
{
    fFrameSize = audio_output.readOutputData(fTo, fMaxSize);
    if(fFrameSize == 0)
    {
        serveOutSilence();
        return;
    }
}

void UniversalPCMSource::serveOutSilence()
{
    //
    //  Send some noise
    //
    
    fFrameSize = 0;
    for(uint i = 0; i < fMaxSize; i++)
    {
        fTo[i] = 0;
        fFrameSize++;
    }
}

UniversalPCMSource::UniversalPCMSource(UsageEnvironment& env, AudioOutput& ao)
                  :FramedSource(env), audio_output(ao)
{
    fLastPlayTime = 0;

    //
    //  Calculate a preferred frame size. Try to be close to 20 ms, but
    // never greater than 1400 bytes (to ensure that it will fit in a single
    // RTP packet).
    //
    
    unsigned fNumChannels = 2 ;
    unsigned fBitsPerSample = 16;
    unsigned fSamplingFrequency = 44100;

    unsigned maxSamplesPerFrame = (1400*8)/(fNumChannels*fBitsPerSample);
    unsigned desiredSamplesPerFrame = (unsigned)(0.02*fSamplingFrequency);
    unsigned samplesPerFrame = desiredSamplesPerFrame < maxSamplesPerFrame
                               ? desiredSamplesPerFrame : maxSamplesPerFrame;

    preferred_frame_size = (samplesPerFrame*fNumChannels*fBitsPerSample)/8;
}

UniversalPCMSource::~UniversalPCMSource()
{
}

/*
---------------------------------------------------------------------
*/

LiveSubsession* LiveSubsession::createNew(UsageEnvironment& env, AudioOutput& ao)
{
    return new LiveSubsession(env, ao);
}

LiveSubsession::LiveSubsession(UsageEnvironment& env, AudioOutput& ao)
              :OnDemandServerMediaSubsession(env, true), audio_output(ao)
{
    universal_pcm_source = NULL;
}


FramedSource* LiveSubsession::createNewStreamSource(
                                                    unsigned /* clientSessionId */,
                                                    unsigned& estBitrate
                                                   )
{
    //
    //  We calculate a bitrate
    //
    
    unsigned int bits_per_second = 44100 * 2 * 16;
    
    //
    //  Add some overhead, then divide by 1000 to get kbps
    //
    
    estBitrate = (bits_per_second + 500) / 1000;

    UniversalPCMSource *ups = UniversalPCMSource::createNew(envir(), audio_output);
    universal_pcm_source = ups;

    //
    //  Now, we add a filter cause WAV's are little-endian and network byte
    //  ordering is big-endian. By returning that object instead, live media
    //  just builds a chain (ups-> byte swapper -> out as rtp)
    //

    // return ups;
    return EndianSwap16::createNew(envir(), ups);
    

}


RTPSink* LiveSubsession::createNewRTPSink(
                                            Groupsock* rtpGroupsock,
                                            unsigned char /* rtpPTIDynamic */,
                                            FramedSource* /* inputSource */
                                         )
{

    //
    //  If you're trying to shave response time off the user interface ->
    //  decoding -> audiooutput -> rtsp streaming -> play audio chain, you
    //  could try fiddling with the socket buffer. I did, and it didn't seem
    //  to make much difference.
    //

    /*
    setSendBufferTo(envir(), rtpGroupsock->socketNum(), 8192);
    cout << "The RTPSink send buffer's size is now " << getSendBufferSize(envir(), rtpGroupsock->socketNum()) << endl;
    */


    return SimpleRTPSink::createNew(
                                    envir(),
                                    rtpGroupsock,
                                    10, //  RTP payload format: 44100, 2 ch
                                    44100,
                                    "audio",
                                    "L16", // Mime type 16bit samples
                                    2  // 2 channels
                                   );                                    
}                   


LiveSubsession::~LiveSubsession()
{
}
