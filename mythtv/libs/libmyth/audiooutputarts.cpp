#include <cstdio>
#include <cstdlib>


using namespace std;

#include "mythcontext.h"
#include "audiooutputarts.h"

AudioOutputARTS::AudioOutputARTS(QString audiodevice, int audio_bits, 
                                 int audio_channels, int audio_samplerate)
               : AudioOutput()
{
    this->audiodevice = audiodevice;
    pcm_handle = NULL;
    effdsp = 0;
    paused = false;
    Reconfigure(audio_bits, audio_channels, audio_samplerate);
}

void AudioOutputARTS::SetBlocking(bool blocking)
{
    (void)blocking;
    // FIXME: kedl: not sure what else could be required here?
}

void AudioOutputARTS::Reconfigure(int audio_bits, 
                                  int audio_channels, int audio_samplerate)
{
    arts_stream_t stream;
    int err;

    if (pcm_handle)
    {
        arts_close_stream(pcm_handle);
        arts_free();
        pcm_handle=NULL;
    }

    VERBOSE(VB_GENERAL, QString("Opening ARTS audio device '%1'.")
                        .arg(audiodevice));
    err = arts_init();
    if (err < 0)
    {
        Error(QString("Opening ARTS sound device failed, the error was: %1")
              .arg(arts_error_text(err)));
        return;
    }
    stream = arts_play_stream(audio_samplerate, audio_bits, audio_channels, 
                              "mythtv");

    effdsp = audio_samplerate * 100;
    pcm_handle = stream;
    this->audio_bits = audio_bits;
    this->audio_channels = audio_channels;
}

AudioOutputARTS::~AudioOutputARTS()
{
    if (pcm_handle != NULL)
        arts_close_stream(pcm_handle);
}

void AudioOutputARTS::Reset(void)
{
    if (pcm_handle == NULL)
        return;
    audbuf_timecode = 0;
    // FIXME: kedl: not sure what else could be required here?
}

void AudioOutputARTS::AddSamples(char *buffer, int frames, long long timecode)
{
    int err;
    
    if (pcm_handle == NULL)
        return;

    err = arts_write(pcm_handle, buffer, 
                     frames * audio_bits / 8 * audio_channels);
    if (err < 0)
    {
        // FIXME: Fatal?
        VERBOSE(VB_IMPORTANT, QString("Write error: %1")
                              .arg(arts_error_text(err)));
        return;
    }

    if (timecode < 0) 
        timecode = audbuf_timecode; // add to current timecode
    
    /* we want the time at the end -- but the file format stores
       time at the start of the chunk. */
    audbuf_timecode = timecode + (int)((frames*100000.0) / effdsp);
}


void AudioOutputARTS::AddSamples(char *buffers[], int frames, long long timecode)
{
    int err;
    int waud=0;
    int audio_bytes = audio_bits / 8;
    int audbufsize = frames*audio_bytes*audio_channels;
    char *audiobuffer = (char *) calloc(1,audbufsize);

    if (audiobuffer==NULL)
    {
        // FIXME: Fatal?
        VERBOSE(VB_IMPORTANT, "Couldn't get memory to write audio to artsd");
        return;
    }

    if (pcm_handle == NULL)
    {
    	free(audiobuffer);
        return;
    }

    for (int itemp = 0; itemp < frames*audio_bytes; itemp+=audio_bytes)
    {
        for(int chan = 0; chan < audio_channels; chan++)
        {
            audiobuffer[waud++] = buffers[chan][itemp];
            if (audio_bits == 16)
                audiobuffer[waud++] = buffers[chan][itemp+1];
            if (waud >= audbufsize)
                waud -= audbufsize;
        }
    }

    err = arts_write(pcm_handle, audiobuffer, frames*4);
    free(audiobuffer);

    if (err < 0)
    {
        // FIXME: Fatal?
        VERBOSE(VB_IMPORTANT, QString("Write error: %1").arg(arts_error_text(err)));
        return;
    }

    if (timecode < 0) 
        timecode = audbuf_timecode; // add to current timecode
    
    /* we want the time at the end -- but the file format stores
       time at the start of the chunk. */
    audbuf_timecode = timecode + (int)((frames*100000.0) / effdsp);
}

void AudioOutputARTS::SetTimecode(long long timecode)
{
    audbuf_timecode = timecode;
}
void AudioOutputARTS::SetEffDsp(int dsprate)
{
    effdsp = dsprate;
}

bool AudioOutputARTS::GetPause(void)
{
    return paused;
}

void AudioOutputARTS::Pause(bool paused)
{
    this->paused = paused;
    // FIXME: kedl: not sure what else could be required here?
}

int AudioOutputARTS::GetAudiotime(void)
{
    // FIXME: kedl: not sure what else could be required here?
    return 0;
}
