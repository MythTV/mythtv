#ifndef AUDIOOUTPUT
#define AUDIOOUTPUT

#include <iostream>
using namespace std;

#include "mythcontext.h"

class AudioOutput
{
 public:
    // opens one of the concrete subclasses
    static AudioOutput *OpenAudio(QString audiodevice, int audio_bits, 
                                 int audio_channels, int audio_samplerate);

    AudioOutput() { lastError = QString::null; };
    virtual ~AudioOutput() { };

    // reconfigure sound out for new params
    virtual void Reconfigure(int audio_bits, 
                             int audio_channels, int audio_samplerate) = 0;
    
    // do AddSamples calls block?
    virtual void SetBlocking(bool blocking) = 0;
    
    // dsprate is in 100 * samples/second
    virtual void SetEffDsp(int dsprate) = 0;

    virtual void Reset(void) = 0;

    // timecode is in milliseconds.
    virtual void AddSamples(char *buffer, int samples, long long timecode) = 0;
    virtual void AddSamples(char *buffers[], int samples, long long timecode) = 0;

    virtual void SetTimecode(long long timecode) = 0;
    virtual bool GetPause(void) = 0;
    virtual void Pause(bool paused) = 0;
 
    virtual int GetAudiotime(void) = 0;
//  virtual int WaitForFreeSpace(int bytes) = 0;

    QString GetError() { return lastError; };

 protected:
    void Error(QString msg) 
     { lastError = msg; VERBOSE(VB_ALL, lastError); };

 private:
    QString lastError;
};

#endif

