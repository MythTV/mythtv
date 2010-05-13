#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>
#include <cerrno>
#include <cstring>

#include <iostream>

using namespace std;

#include "mythcorecontext.h"
#include "audiooutputjack.h"
#include "util.h"

extern "C"
{
#include "bio2jack.h"
}

AudioOutputJACK::AudioOutputJACK(const AudioSettings &settings) :
    AudioOutputBase(settings), audioid(-1)
{
    // Initialise the Jack output layer
    JACK_Init();

    // Set everything up
    Reconfigure(settings);
}

vector<int> AudioOutputJACK::GetSupportedRates()
{
    const int srates[] = { 8000, 11025, 16000, 22050, 32000, 44100, 48000 };
    vector<int> rates(srates, srates + sizeof(srates) / sizeof(int) );
    unsigned long jack_port_flags = 0;
    unsigned int jack_port_name_count = 1;
    const char *jack_port_name = audio_main_device.toAscii();
    int err = -1;
    audioid = -1;
    vector<int>::iterator it = rates.begin();

    while (it != rates.end())
    {
        unsigned long lrate = (unsigned long) *it;
        err = JACK_OpenEx(&audioid, 16, &lrate,
                          2, 2, &jack_port_name, jack_port_name_count,
                          jack_port_flags);

        if (err == ERR_OPENING_JACK)
        {
            Error(QString("Error connecting to jackd: %1. Is it running?")
                  .arg(audio_main_device));
            rates.clear();
            return rates;
        }
        else
        {
            if (err == ERR_RATE_MISMATCH)
                it = rates.erase(it);
            else
                it++;
        }
        if (err == ERR_SUCCESS)
            JACK_Close(audioid);
        audioid = -1;
    }
    return rates;
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

    if (!audio_main_device.isEmpty())
    {
        QByteArray main_device = audio_main_device.toAscii();
        jack_port_flags = 0;
        jack_port_name_count = 1;
        jack_port_name = main_device.constData();
    }

    int err = -1;
    audioid = -1;
    while (timer.elapsed() < 2000 && audioid == -1)
    {
        unsigned long audio_samplerate_long = audio_samplerate;
        err = JACK_OpenEx(&audioid, 16, &audio_samplerate_long,
                          audio_channels, audio_channels, &jack_port_name,
                          jack_port_name_count, jack_port_flags);
        audio_samplerate = audio_samplerate_long;
        if (err == 1) {
            Error(QString("Error connecting to jackd:%1.  Is it running?")
                  .arg(audio_main_device));
            return false;
        } else if (err == 2) {
            // need to resample
            VERBOSE(VB_AUDIO, QString("Failed to open device at"
                                      " requested samplerate.  Retrying"));
            unsigned long audio_samplerate_long = audio_samplerate;
            err = JACK_OpenEx(&audioid, 16, &audio_samplerate_long,
                              audio_channels, audio_channels, &jack_port_name,
                              jack_port_name_count, jack_port_flags);
            audio_samplerate = audio_samplerate_long;
        } else if (err == ERR_PORT_NOT_FOUND) {
            VERBOSE(VB_IMPORTANT, QString("Error opening audio device (%1), "
                    " Port not found.").arg(audio_main_device) + ENO);
        }

        if (err != 0)
        {
            VERBOSE(VB_IMPORTANT, QString("Error opening audio device (%1)")
                    .arg(audio_main_device) + ENO);
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


int AudioOutputJACK::GetBufferedOnSoundcard(void) const
{
    return  JACK_GetBytesStored(audioid) + fragment_size * 2;
}

int AudioOutputJACK::GetSpaceOnSoundcard(void) const
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
        volume = gCoreContext->GetNumSetting("MasterMixerVolume", 80);

    JACK_SetAllVolume(audioid, volume);
}

int AudioOutputJACK::GetVolumeChannel(int channel) const
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

