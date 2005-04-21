/*
    livemedia.cpp

    (c) 2005 Thor Sigvaldason and Isaac Richards
    Part of the mythTV project
    
    This is based on example code from the live media library which is:
    
    Copyright (c) 1996-2000 Live Networks, Inc.  All rights reserved.
    
*/

#include <iostream>
using namespace std;

#include "livemedia.h"

LiveTaskScheduler* LiveTaskScheduler::createNew()
{
    return new LiveTaskScheduler();
}

LiveTaskScheduler::LiveTaskScheduler()
{
}
        
void LiveTaskScheduler::stepOnce(unsigned t)
{
    BasicTaskScheduler::SingleStep(t);
}

void LiveTaskScheduler::doEventLoop(char* watchVariable)
{
    BasicTaskScheduler::doEventLoop(watchVariable);
}

TaskToken LiveTaskScheduler::scheduleDelayedTask(
                                                    int microsecs, 
                                                    TaskFunc* proc, 
                                                    void* clientd
                                                )
{
    return BasicTaskScheduler::scheduleDelayedTask(microsecs, proc, clientd);
}
                                                      
void LiveTaskScheduler::unscheduleDelayedTask(TaskToken& prevTask)
{
    BasicTaskScheduler::unscheduleDelayedTask(prevTask);
}

LiveTaskScheduler::~LiveTaskScheduler()
{
}

        
/*
---------------------------------------------------------------------
*/

AudioOutputSink* AudioOutputSink::createNew(
                                            UsageEnvironment& env,
                                            unsigned buffer_size
                                           )
{
    return new AudioOutputSink(env, buffer_size);
}

AudioOutputSink::AudioOutputSink(UsageEnvironment& env, unsigned buffer_size)
               : MediaSink(env), fBufferSize(buffer_size)
{
    fBuffer = new unsigned char[buffer_size];
    audio_output = AudioOutput::OpenAudio("/dev/dsp", 16, 2, 44100, AUDIOOUTPUT_MUSIC, true );
    audio_output->setBufferSize(256 * 1024);
    audio_output->SetBlocking(false);
}

void AudioOutputSink::afterGettingFrame(
                                        void* clientData, 
                                        unsigned frameSize,
                                        unsigned /*numTruncatedBytes*/,
                                        struct timeval presentationTime,
                                        unsigned /*durationInMicroseconds*/
                                       ) 
{
    AudioOutputSink *sink = (AudioOutputSink*)clientData;
    sink->afterGettingFrame1(frameSize, presentationTime);
}

void AudioOutputSink::afterGettingFrame1(
                                            unsigned frameSize,
                                            struct timeval /* presentationTime */
                                        ) 
{


    //
    //  RTP uses network byte ordering (big endian) in all cases. But PCM
    //  should be little endian, so we swap it around
    //  

    unsigned numValues = frameSize/2;
    short* value = (short*)fBuffer;
    for (unsigned i = 0; i < numValues; ++i)
    {
        short const orig = value[i];
        value[i] = ((orig&0xFF)<<8) | ((orig&0xFF00)>>8);
    }


    bool result = audio_output->AddSamples( (char *) fBuffer, frameSize / 4, -1);
    int numb_times_blocking = 0;
    
    while(!result)
    {
        usleep(100000);
        result = audio_output->AddSamples( (char *) fBuffer, frameSize / 4, -1);
        if (numb_times_blocking % 10 == 0)
        {
            warning("blocking on writing RTSP/RTP audio to soundcard, change buffer?");
        }
        numb_times_blocking++;
    }
    
    continuePlaying();
}
                                                       
                                                       

Boolean AudioOutputSink::continuePlaying()
{
    if (fSource == NULL)
    {
        return false;
    }
    
    fSource->getNextFrame(
                            fBuffer, 
                            fBufferSize,
                            afterGettingFrame, 
                            this,
                            onSourceClosure, 
                            this
                         );
                              
    return true;
}

AudioOutputSink::~AudioOutputSink()
{
    delete [] fBuffer;
    delete audio_output;
    audio_output = NULL;
}
