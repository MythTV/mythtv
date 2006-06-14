#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>
#include <cerrno>
#include <cstring>

#include <iostream>
#include <qdatetime.h>

using namespace std;

#include "mythcontext.h"
#include "audiooutputjack.h"
#include "util.h"

extern "C"
{
#include "bio2jack.h"
}

AudioOutputJACK::AudioOutputJACK(
    QString laudio_main_device, QString           laudio_passthru_device,
    int     laudio_bits,        int               laudio_channels,
    int     laudio_samplerate,  AudioOutputSource lsource,
    bool    lset_initial_vol,   bool              laudio_passthru) :
    AudioOutputBase(laudio_main_device, laudio_passthru_device,
                    laudio_bits,        laudio_channels,
                    laudio_samplerate,  lsource,
                    lset_initial_vol,   laudio_passthru),
    audioid(-1)
{
    // Initialise the Jack output layer
    JACK_Init();

    // Set everything up
    Reconfigure(laudio_bits, laudio_channels,
                laudio_samplerate, laudio_passthru);
}

AudioOutputJACK::~AudioOutputJACK()
{
    // Close down all audio stuff
    KillAudio();
}

bool AudioOutputJACK::OpenDevice()
{
    MythTimer timer;
    timer.start();

    unsigned long jack_port_flags=JackPortIsPhysical;
    unsigned int jack_port_name_count=0;
    const char *jack_port_name=NULL;

    VERBOSE(VB_GENERAL, QString("Opening JACK audio device '%1'.")
            .arg(audio_main_device));

    if (!audio_main_device.isEmpty()) {
        jack_port_flags = 0;
        jack_port_name_count = 1;
        jack_port_name = audio_main_device.ascii();
    }

    int err = -1;
    audioid = -1;
    while (timer.elapsed() < 2000 && audioid == -1)
    {
        err = JACK_OpenEx(&audioid, 16, (unsigned long *) &audio_samplerate,
                          audio_channels, audio_channels, &jack_port_name,
                          jack_port_name_count, jack_port_flags);
        if (err == 1) {
            Error(QString("Error connecting to jackd:%1.  Is it running?")
                  .arg(audio_main_device));
            return false;
        } else if (err == 2) {
            // need to resample
            VERBOSE(VB_AUDIO, QString("Failed to open device at"
                                      " requested samplerate.  Retrying"));
            err = JACK_OpenEx(&audioid, 16, (unsigned long *) &audio_samplerate,
                              audio_channels, audio_channels, &jack_port_name,
                              jack_port_name_count, jack_port_flags);
        } else if (err == ERR_PORT_NOT_FOUND) {
            VERBOSE(VB_IMPORTANT, QString("Error opening audio device (%1), "
                    " Port not found.").arg(audio_main_device));
            perror(audio_main_device.ascii());
        }

        if (err != 0)
        {
            VERBOSE(VB_IMPORTANT, QString("Error opening audio device (%1), the"
                    " error num was: %2").arg(audio_main_device).arg(err));
            perror(audio_main_device.ascii());
        }
        if (audioid < 0)
            usleep(50);
    }

    if (audioid == -1)
    {
        Error(QString("Error opening audio device (%1), the error num was: %2")
              .arg(audio_main_device).arg(err));
        return false;
    }
    
    fragment_size = JACK_GetJackBufferedBytes(audioid);

    // Ensure blocking layer has a small amount of buffered data
    JACK_SetMaxBufferedBytes(audioid, 4 * fragment_size);

    VERBOSE(VB_AUDIO, QString("Audio fragment size: %1")
                                 .arg(fragment_size));

    audio_buffer_unused = JACK_GetBytesFreeSpace(audioid);
    JACK_SetPosition(audioid, BYTES, 0);

    // Setup volume control
    if (internal_vol)
        VolumeInit();

    // Device opened successfully
    return true; 
}

void AudioOutputJACK::CloseDevice()
{
    int err = -1;
    if (audioid != -1) 
    {
        err = JACK_Close(audioid);
        if (err != 0)
            Error(QString("Error closing Jack output device"));
    }

    audioid = -1;
}

void AudioOutputJACK::WriteAudio(unsigned char *aubuf, int size)
{
    if (audioid < 0)
        return;

    unsigned char *tmpbuf;
    int written = 0, lw = 0;

    tmpbuf = aubuf;

    while ((written < size) &&
           ((lw = JACK_Write(audioid, tmpbuf, size - written)) > 0))
    {
        written += lw;
        tmpbuf += lw;
    }
}


inline int AudioOutputJACK::getBufferedOnSoundcard(void)
{
    return  JACK_GetBytesStored(audioid) + fragment_size * 2;
}

inline int AudioOutputJACK::getSpaceOnSoundcard(void)
{
    int space = 0;

    space = JACK_GetBytesFreeSpace(audioid);

    if (space < 0)
    {
        VERBOSE(VB_IMPORTANT, "Jack returned negative number for Free Space...");
    }

    return space;
}

void AudioOutputJACK::VolumeInit(void)
{
    int volume = 100;
    if (set_initial_vol)
        volume = gContext->GetNumSetting("MasterMixerVolume", 80);

    JACK_SetAllVolume(audioid, volume);
}

int AudioOutputJACK::GetVolumeChannel(int channel)
{
    unsigned int vol = 0;
    
    if (!internal_vol)
        return 100;

    JACK_GetVolumeForChannel(audioid, channel, &vol);
    return vol;
}

void AudioOutputJACK::SetVolumeChannel(int channel, int volume)
{
    if (internal_vol)
        JACK_SetVolumeForChannel(audioid, channel, volume);
}

