/*****************************************************************************
 * = NAME
 * audiooutputca.cpp
 *
 * = DESCRIPTION
 * Core Audio glue for Mac OS X.
 * This plays MythTV audio through the default output device on OS X.
 *
 * = REVISION
 * $Id$
 *
 * = AUTHORS
 * Jeremiah Morris
 *****************************************************************************/

#include <CoreServices/CoreServices.h>
#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>

using namespace std;

#include "mythcontext.h"
#include "audiooutputca.h"

// this holds Core Audio member variables
struct CoreAudioData {
    AudioUnit output_unit;
};

// This friend class holds our callback.
class AudioOutputCA_Friend
{
public:
    static OSStatus AURender(void *inRefCon,
                             AudioUnitRenderActionFlags *ioActionFlags,
                             const AudioTimeStamp *inTimeStamp,
                             UInt32 inBusNumber,
                             UInt32 inNumberFrames,
                             AudioBufferList *ioData);
};


AudioOutputCA::AudioOutputCA(QString audiodevice, int laudio_bits, 
                             int laudio_channels, int laudio_samplerate)
             : AudioOutput()
{
    pthread_mutex_init(&audio_buflock, NULL);
    pthread_mutex_init(&avsync_lock, NULL);
    pthread_cond_init(&audio_bufsig, NULL);
    audio_bits = -1;
    audio_channels = -1;
    audio_samplerate = -1;
    coreaudio_data = new CoreAudioData();
    coreaudio_data->output_unit = NULL;
    
    // Get default output device
    ComponentDescription desc;
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_DefaultOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;
    
    Component comp = FindNextComponent(NULL, &desc);
    if (comp == NULL)
    {
        Error(QString("FindNextComponent failed"));
        return;
    }
    
    OSStatus err = OpenAComponent(comp, &coreaudio_data->output_unit);
    if (err)
    {
        Error(QString("OpenAComponent returned %1").arg((long)err));
        return;
    }
    
    // Attach callback to default output
    AURenderCallbackStruct input;
    input.inputProc = AudioOutputCA_Friend::AURender;
    input.inputProcRefCon = this;
    
    err = AudioUnitSetProperty(coreaudio_data->output_unit,
                               kAudioUnitProperty_SetRenderCallback,
                               kAudioUnitScope_Input,
                               0,
                               &input,
                               sizeof(input));
    if (err)
    {
        Error(QString("AudioUnitSetProperty (callback) returned %1")
              .arg((long)err));
        return;
    }
  
    Reconfigure(laudio_bits, laudio_channels, laudio_samplerate);
}

AudioOutputCA::~AudioOutputCA()
{
    KillAudio();
    
    CloseComponent(coreaudio_data->output_unit);

    pthread_mutex_destroy(&audio_buflock);
    pthread_mutex_destroy(&avsync_lock);
    pthread_cond_destroy(&audio_bufsig);
    
    delete coreaudio_data;
}

void AudioOutputCA::Reconfigure(int laudio_bits, int laudio_channels, 
                                int laudio_samplerate)
{
    if (laudio_bits == audio_bits && laudio_channels == audio_channels &&
        laudio_samplerate == audio_samplerate)
        return;

//  printf("Starting reconfigure\n");
    KillAudio();
    
    pthread_mutex_lock(&audio_buflock);
    pthread_mutex_lock(&avsync_lock);

    waud = raud = 0;
    
    audio_channels = laudio_channels;
    audio_bits = laudio_bits;
    audio_samplerate = laudio_samplerate;
    audio_bytes_per_sample = audio_channels * audio_bits / 8;
    
    audbuf_timecode = 0;
    audiotime = 0;
    effdsp = audio_samplerate * 100;
    gettimeofday(&audiotime_updated, NULL);
    
    // Set up the audio output unit
    AudioStreamBasicDescription conv_in_desc;
    bzero(&conv_in_desc, sizeof(AudioStreamBasicDescription));
    conv_in_desc.mSampleRate       = audio_samplerate;
    conv_in_desc.mFormatID         = kAudioFormatLinearPCM;
    conv_in_desc.mFormatFlags      = kLinearPCMFormatFlagIsSignedInteger
                                     | kLinearPCMFormatFlagIsBigEndian;
    conv_in_desc.mBytesPerPacket   = audio_bytes_per_sample;
    conv_in_desc.mFramesPerPacket  = 1;
    conv_in_desc.mBytesPerFrame    = audio_bytes_per_sample;
    conv_in_desc.mChannelsPerFrame = audio_channels;
    conv_in_desc.mBitsPerChannel   = audio_bits;

    OSStatus err = AudioUnitSetProperty(coreaudio_data->output_unit,
                                        kAudioUnitProperty_StreamFormat,
                                        kAudioUnitScope_Input,
                                        0,
                                        &conv_in_desc,
                                        sizeof(AudioStreamBasicDescription));
    if (err)
    {
        Error(QString("AudioUnitSetProperty returned %1").arg((long)err));
        return;
    }
    
    // We're all set up - start the audio output unit
    ComponentResult res = AudioUnitInitialize(coreaudio_data->output_unit);
    if (res)
    {
        Error(QString("AudioUnitInitialize returned %1").arg((long)res));
        return;
    }
    
    err = AudioOutputUnitStart(coreaudio_data->output_unit);
    if (err)
    {
        Error(QString("AudioOutputUnitStart returned %1").arg((long)err));
        return;
    }

    pthread_mutex_unlock(&avsync_lock);
    pthread_mutex_unlock(&audio_buflock);
    killaudio = false;
    pauseaudio = false;
    VERBOSE(VB_AUDIO, "Ending reconfigure");
}

void AudioOutputCA::KillAudio()
{
    if (!killaudio)
    {
        VERBOSE(VB_AUDIO, "Killing AudioOutput");
        AudioOutputUnitStop(coreaudio_data->output_unit);
        AudioUnitUninitialize(coreaudio_data->output_unit);
        AudioUnitReset(coreaudio_data->output_unit,
                       kAudioUnitScope_Input, NULL);
    }
    killaudio = true;
    pauseaudio = false;
}

bool AudioOutputCA::GetPause(void)
{
    return pauseaudio;
}

void AudioOutputCA::Pause(bool paused)
{
    pauseaudio = paused;
}

void AudioOutputCA::Reset()
{
    pthread_mutex_lock(&audio_buflock);
    pthread_mutex_lock(&avsync_lock);

    raud = waud = 0;
    audbuf_timecode = 0;
    audiotime = 0;
    gettimeofday(&audiotime_updated, NULL);
        
    pthread_mutex_unlock(&avsync_lock);
    pthread_mutex_unlock(&audio_buflock);
}

void AudioOutputCA::SetTimecode(long long timecode)
{
    pthread_mutex_lock(&audio_buflock);
    audbuf_timecode = timecode;
    pthread_mutex_unlock(&audio_buflock);
}

void AudioOutputCA::SetEffDsp(int dsprate)
{
    VERBOSE(VB_AUDIO, QString("SetEffDsp: %1").arg(dsprate));
    effdsp = dsprate;
}

void AudioOutputCA::SetBlocking(bool blocking)
{
    this->blocking = blocking;
}

int AudioOutputCA::audiolen(bool use_lock)
{
    /* Thread safe, returns the number of valid bytes in the audio buffer */
    int ret;
    
    if (use_lock) 
        pthread_mutex_lock(&audio_buflock);

    if (waud >= raud)
        ret = waud - raud;
    else
        ret = AUDBUFSIZE - (raud - waud);

    if (use_lock)
        pthread_mutex_unlock(&audio_buflock);

    return ret;
}

int AudioOutputCA::audiofree(bool use_lock)
{
    return AUDBUFSIZE - audiolen(use_lock) - 1;
    /* There is one wasted byte in the buffer. The case where waud = raud is
       interpreted as an empty buffer, so the fullest the buffer can ever
       be is AUDBUFSIZE - 1. */
}

int AudioOutputCA::GetAudiotime(void)
{
    /* Returns the current timecode of audio leaving the soundcard, based
       on the 'audiotime' computed earlier, and the delay since it was computed.

       This is a little roundabout...

       The reason is that computing 'audiotime' requires acquiring the audio 
       lock, which the video thread should not do. So, we call 'SetAudioTime()'
       from the audio thread, and then call this from the video thread. */
    int ret;
    struct timeval now;

    if (audiotime == 0)
        return 0;

    pthread_mutex_lock(&avsync_lock);

    gettimeofday(&now, NULL);

    ret = audiotime;
 
    ret += (now.tv_sec - audiotime_updated.tv_sec) * 1000;
    ret += (now.tv_usec - audiotime_updated.tv_usec) / 1000;

    pthread_mutex_unlock(&avsync_lock);
    return ret;
}

void AudioOutputCA::AddSamples(char *buffers[],
                               int samples, long long timecode)
{
    VERBOSE(VB_AUDIO, QString("AddSamples[] %1")
                              .arg(samples * audio_bytes_per_sample));
    pthread_mutex_lock(&audio_buflock);

    int audio_bytes = audio_bits / 8;
    int afree = audiofree(false);
    
    while (samples * audio_bytes_per_sample > afree)
    {
        if (blocking)
        {
            VERBOSE(VB_AUDIO, "Waiting for free space");
            // wait for more space
            pthread_cond_wait(&audio_bufsig, &audio_buflock);
            afree = audiofree(false);
        }
        else
        {
            VERBOSE(VB_IMPORTANT, "Audio buffer overflow, audio data lost!");
            samples = afree / audio_bytes_per_sample;
        }
    }
    
    for (int itemp = 0; itemp < samples*audio_bytes; itemp+=audio_bytes)
    {
        for(int chan = 0; chan < audio_channels; chan++)
        {
            audiobuffer[waud++] = buffers[chan][itemp];
            if(audio_bits == 16)
                audiobuffer[waud++] = buffers[chan][itemp+1];
            
            if (waud >= AUDBUFSIZE)
                waud -= AUDBUFSIZE;
        }
    }

    if (timecode < 0) 
        timecode = audbuf_timecode; // add to current timecode
    
    audbuf_timecode = timecode + (int)((samples * 100000.0) / effdsp);

    pthread_mutex_unlock(&audio_buflock);
    
}

void AudioOutputCA::AddSamples(char *buffer,
                               int samples, long long timecode)
{
    VERBOSE(VB_AUDIO, QString("AddSamples %1")
                              .arg(samples * audio_bytes_per_sample));
    pthread_mutex_lock(&audio_buflock);

    int afree = audiofree(false);

    int len = samples * audio_bytes_per_sample;
    
    while (len > afree)
    {
        if (blocking)
        {
            VERBOSE(VB_AUDIO, "Waiting for free space");
            // wait for more space
            pthread_cond_wait(&audio_bufsig, &audio_buflock);
            afree = audiofree(false);
        }
        else
        {
            VERBOSE(VB_IMPORTANT, "Audio buffer overflow, audio data lost!");
            len = afree;
        }
    }

    int bdiff = AUDBUFSIZE - waud;
    if (bdiff < len)
    {
        memcpy(audiobuffer + waud, buffer, bdiff);
        memcpy(audiobuffer, buffer + bdiff, len - bdiff);
    }
    else
        memcpy(audiobuffer + waud, buffer, len);

    waud = (waud + len) % AUDBUFSIZE;

    if (timecode < 0) 
        timecode = audbuf_timecode; // add to current timecode
    
    /* we want the time at the end -- but the file format stores
       time at the start of the chunk. */
    audbuf_timecode = timecode + (int)((samples * 100000.0) / effdsp);

    pthread_mutex_unlock(&audio_buflock);
}

/* This SetAudiotime is called from inside the render callback below. */
void AudioOutputCA::SetAudiotime(unsigned long long timestamp, int audlen)
{
    if (audbuf_timecode == 0)
        return;

    /* We want to calculate 'audiotime', which is the timestamp of the audio
       which is leaving the sound card at this instant.
       
       hosttime        - time the first buffered sample is expected to play,
                           provided by Core Audio
       audlen          - bytes of data in buffer
       audbuf_timecode - timestamp for last buffered sample
     */

    pthread_mutex_lock(&avsync_lock);
 
    // audbuf_timecode is the time of our last buffered sample
    audiotime = audbuf_timecode;
    
    // subtract the time it takes to play our current buffer
    audiotime -= (int)(audlen * 100000.0 /
                       (audio_bytes_per_sample * effdsp));

    // now, subtract the latency from the output unit
    //  (hosttime is time the first buffered sample will play)
    UInt64 nanos = AudioConvertHostTimeToNanos(
                      timestamp - AudioGetCurrentHostTime());
    audiotime -= (int)(nanos / 1000000.0);
 
    gettimeofday(&audiotime_updated, NULL);

    pthread_mutex_unlock(&avsync_lock);
}
    
/* Object-oriented part of callback */
bool AudioOutputCA::RenderAudio(char *fragment,
                                int fragment_size,
                                unsigned long long timestamp)
{
    if (pauseaudio || killaudio)
    {
        return false;
    }
    
    /* This callback is called when the sound system requests
       data.  We don't want to block here, because that would
       just cause dropouts anyway, so we always return whatever
       data is available.  If we haven't received enough, either
       because we've finished playing or we have a buffer
       underrun, we play silence to fill the unused space.  */
    
    pthread_mutex_lock(&audio_buflock); // begin critical section

    int audlen = audiolen(false);
    if (!audlen)
    {
        /* no data at all - this is like the paused case above */
        pthread_mutex_unlock(&audio_buflock);
        return false;
    }
    
    /* Variables:
          fragment_size - length of buffer we must fill
          audlen        - total audio data we have
          avail_size    - amount of audio data we can output this time
          silence_size  - data we don't have (need to pad out fragment)
    */
    int avail_size = audlen;
    if (fragment_size < avail_size)
    {
        avail_size = fragment_size;
    }
    int silence_size = fragment_size - avail_size;
    
    int bdiff = AUDBUFSIZE - raud;
    if (avail_size > bdiff)
    {
        memcpy(fragment, audiobuffer + raud, bdiff);
        memcpy(fragment + bdiff, audiobuffer, avail_size - bdiff);
    }
    else
    {
        memcpy(fragment, audiobuffer + raud, avail_size);
    }
    
    if (silence_size)
    {
        // play silence on buffer underrun
        bzero(fragment + avail_size, silence_size);
    }
    
    /* update audiotime */
    SetAudiotime(timestamp, audlen);
    
    /* update raud */
    raud = (raud + avail_size) % AUDBUFSIZE;
    VERBOSE(VB_AUDIO, "Broadcasting free space avail");
    pthread_cond_broadcast(&audio_bufsig);        
    
    pthread_mutex_unlock(&audio_buflock); // end critical section
    return true;
}


/**** AudioOutputCA_Friend ****/

/* This callback provides converted audio data to the default output device. */
OSStatus
AudioOutputCA_Friend::AURender(void *inRefCon,
                               AudioUnitRenderActionFlags *ioActionFlags,
                               const AudioTimeStamp *inTimeStamp,
                               UInt32 inBusNumber,
                               UInt32 inNumberFrames,
                               AudioBufferList *ioData)
{
    AudioOutputCA *inst = (AudioOutputCA *)inRefCon;
    
    if (!inst->RenderAudio((char *)(ioData->mBuffers[0].mData),
                           ioData->mBuffers[0].mDataByteSize,
                           inTimeStamp->mHostTime))
    {
        // play silence if RenderAudio returns false
        bzero(ioData->mBuffers[0].mData, ioData->mBuffers[0].mDataByteSize);
        *ioActionFlags = kAudioUnitRenderAction_OutputIsSilence;
    }
    return noErr;
}
