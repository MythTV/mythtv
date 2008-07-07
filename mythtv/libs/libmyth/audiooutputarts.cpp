#include <cstdio>
#include <cstdlib>

using namespace std;

#include "mythcontext.h"
#include "audiooutputarts.h"

AudioOutputARTS::AudioOutputARTS(const AudioSettings &settings) :
    AudioOutputBase(settings),
    pcm_handle(NULL), buff_size(-1), can_hw_pause(false)
{
    // Set everything up
    Reconfigure(settings);
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

bool AudioOutputARTS::OpenDevice()
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
                        .arg(audio_main_device));
    err = arts_init();
    if (err < 0)
    {
        Error(QString("Opening ARTS sound device failed, the error was: %1")
              .arg(arts_error_text(err)));
        return false;
    }
    stream = arts_play_stream(audio_samplerate, audio_bits, audio_channels, 
                              "mythtv");

    buff_size = arts_stream_get(stream, ARTS_P_BUFFER_SIZE) + 
                (arts_stream_get(stream, ARTS_P_SERVER_LATENCY) * 
                 audio_bits * audio_channels * audio_samplerate / 8);

    pcm_handle = stream;

    // Device opened successfully
    return true;
}

void AudioOutputARTS::WriteAudio(unsigned char *aubuf, int size)
{
    int err;
    
    if (pcm_handle == NULL)
        return;

    err = arts_write(pcm_handle, aubuf, size);
    if (err < 0)
    {
        // FIXME: Fatal?
        VERBOSE(VB_IMPORTANT, QString("Write error: %1")
                              .arg(arts_error_text(err)));
        return;
    }
}

int AudioOutputARTS::GetBufferedOnSoundcard(void) const
{
    return buff_size - GetSpaceOnSoundcard();
}

int AudioOutputARTS::GetSpaceOnSoundcard(void) const
{
    return arts_stream_get(pcm_handle, ARTS_P_BUFFER_SIZE);
}

int AudioOutputARTS::GetVolumeChannel(int /*channel*/) const
{
    // Do nothing
    return 100;
}
void AudioOutputARTS::SetVolumeChannel(int /*channel*/, int /*volume*/)
{
    // Do nothing
}

