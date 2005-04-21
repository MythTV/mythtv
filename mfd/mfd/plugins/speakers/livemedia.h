#ifndef LIVEMEDIA_H_
#define LIVEMEDIA_H_
/*
    livemedia.h

    (c) 2005 Thor Sigvaldason and Isaac Richards
    Part of the mythTV project
    
    This is based on example code from the live media library which is:
    
    Copyright (c) 1996-2000 Live Networks, Inc.  All rights reserved.
    
*/

#include <BasicUsageEnvironment.hh>
#include <MediaSink.hh>

#include <mythtv/audiooutput.h>

//
//  We subclass liveMedia's Basic Task Scheduler so that we can SingleStep()
//  it from out plugin execution loop
//

class LiveTaskScheduler : public BasicTaskScheduler
{
  public:
  
    static      LiveTaskScheduler* createNew();
    virtual    ~LiveTaskScheduler();
    void        stepOnce(unsigned t);
    void        doEventLoop(char* watchVariable);
    TaskToken   scheduleDelayedTask(
                                    int microseconds, 
                                    TaskFunc* proc, 
                                    void* clientData
                                   );
    void        unscheduleDelayedTask(TaskToken& prevTask);
                                                      

  protected:
  
    LiveTaskScheduler();

};


//
//  This is the class that actually gets PCM audio data (after's in come
//  across the wire and been assembled by the liveMedia library). It just
//  pumps it out the soundcard.
//

class AudioOutputSink : public MediaSink
{
  public:
  
    static AudioOutputSink* createNew(
                                        UsageEnvironment& env, 
                                        unsigned buffer_size = 20000
                                     );
    
  protected:
  
    AudioOutputSink(UsageEnvironment& env, unsigned buffer_size);
    virtual ~AudioOutputSink();
    
    static void afterGettingFrame(
                                    void* clientData, 
                                    unsigned frameSize,
                                    unsigned numTruncatedBytes,
                                    struct timeval presentationTime,
                                    unsigned durationInMicroseconds
                                 );

    virtual void afterGettingFrame1(
                                    unsigned frameSize,
                                    struct timeval presentationTime
                                   );
                                                                    
    Boolean continuePlaying();

  private:
  
    unsigned char* fBuffer;
    unsigned fBufferSize;
    AudioOutput *audio_output;
                                      
};


#endif
