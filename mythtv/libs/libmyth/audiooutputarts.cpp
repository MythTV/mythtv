#include <cstdio>
#include <cstdlib>


using namespace std;

#include "mythcontext.h"
#include "audiooutputarts.h"

AudioOutputARTS::AudioOutputARTS(QString audiodevice, int audio_bits, 
                                 int audio_channels, int audio_samplerate,
				 AudioOutputSource source, bool set_initial_vol)
              : AudioOutputBase(audiodevice, laudio_bits,
                              laudio_channels, laudio_samplerate, source, set_initial_vol)
{
    // our initalisation
    pcm_handle = NULL;

    // Set everything up
    Reconfigure(audio_bits, audio_channels, audio_samplerate);
}

AudioOutputARTS::~AudioOutputARTS()
{
    // Close down all audio stuff
    KillAudio();
}

void AudioOutputARTS::CloseDevice()
{
    if (pcm_handle != NULL)
        arts_close_stream(pcm_handle);

    pcm_handle = NULL;
}

void AudioOutputARTS::OpenDevice()
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

    buff_size = arts_stream_get(stream, ARTS_P_BUFFER_SIZE) + 
		(arts_stream_get(stream, ARTS_P_SERVER_LATENCY) * 
			audio_bits * audio_channels * audio_samplerate / 8);

    pcm_handle = stream;
}

void AudioOutputARTS::Reset(void)
{
    if (pcm_handle == NULL)
        return;
    audbuf_timecode = 0;
    // FIXME: kedl: not sure what else could be required here?
}

void AudioOutputARTS::WriteAudio(unsigned char *aubuf, int size)
{
    int err;
    
    if (pcm_handle == NULL)
        return;

    err = arts_write(pcm_handle, aubuf, 
                     frames * audio_bits / 8 * audio_channels);
    if (err < 0)
    {
        // FIXME: Fatal?
        VERBOSE(VB_IMPORTANT, QString("Write error: %1")
                              .arg(arts_error_text(err)));
        return;
}

inline int AudioOutputARTS::getBufferedOnSoundcard(void)
{
    return buff_size - getSpaceOnSoundcard();
}

inline int AudioOutputARTS::getSpaceOnSoundcard(void)
{
    return arts_stream_get(pcm_handle, ARTS_P_BUFFER_SIZE);
}

int AudioOutputARTS::GetVolumeChannel(int channel)
{
    // Do nothing
    return 100;
}
void AudioOutputARTS::SetVolumeChannel(int channel, int volume)
{
    // Do nothing
}

