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
extern "C"
{
#include "bio2jack.h"
}

AudioOutputJACK::AudioOutputJACK(QString audiodevice, int laudio_bits, 
                               int laudio_channels, int laudio_samplerate)
               : AudioOutputBase(audiodevice) 
{
    // Initialise the Jack output layer
    JACK_Init();

    // our initalisation
    audioid = -1;

    // Set everything up
    Reconfigure(laudio_bits, laudio_channels, laudio_samplerate);
}

AudioOutputJACK::~AudioOutputJACK()
{
    // Close down all audio stuff
    KillAudio();
}

bool AudioOutputJACK::OpenDevice()
{
    QTime timer;
    timer.start();

    unsigned long jack_port_flags=JackPortIsPhysical;
    unsigned int jack_port_name_count=0;
    const char *jack_port_name=NULL;
    
    if(!audiodevice.isEmpty()) {
        jack_port_flags = 0;
        jack_port_name_count = 1;
	jack_port_name = audiodevice.ascii();
    }
	
    int err = -1;
    audioid = -1;
    while (timer.elapsed() < 2000 && audioid == -1)
    {
cout << "About to open\n";
        err = JACK_OpenEx(&audioid, 16, (unsigned long *) &audio_samplerate, audio_channels, audio_channels,
	                &jack_port_name, jack_port_name_count, jack_port_flags);
cout << "Err: " << err << "\n";
	if (err == 1) {
	    Error(QString("Error connecting to jackd:%1.  Is it running?").arg(audiodevice));
	    return false;
        } else if (err == 2) {
	  // need to resample
	  VERBOSE(VB_AUDIO, QString("Failed to open device at requested samplerate.  Retrying"));
          err = JACK_OpenEx(&audioid, 16, (unsigned long *) &audio_samplerate, audio_channels, audio_channels,
                        &jack_port_name, jack_port_name_count, jack_port_flags);
	}

        if (err != 0)
        {
            VERBOSE(VB_IMPORTANT, QString("Error opening audio device (%1), the"
                    " error num was: %2").arg(audiodevice).arg(err));
            perror(audiodevice.ascii());
        }
        if (audioid < 0)
            usleep(50);
    }

    if (audioid == -1)
    {
        Error(QString("Error opening audio device (%1), the error num was: %2")
              .arg(audiodevice).arg(err));
        return false;
    }
    
    // Set volumes to 100%
    JACK_SetAllVolume(audioid, 100);

    // TODO: Should we get this from the driver?
    fragment_size = 512;

//    VERBOSE(VB_GENERAL, QString("Audio fragment size: %1")
//                                 .arg(fragment_size));

    audio_buffer_unused = JACK_GetBytesFreeSpace(audioid);
    JACK_SetPosition(audioid, BYTES, 0);

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
    return  JACK_GetBytesStored(audioid);
}

inline int AudioOutputJACK::getSpaceOnSoundcard(void)
{
    int space = 0;

//    ioctl(audiofd, SNDCTL_DSP_GETOSPACE, &info);
//    space = info.bytes - audio_buffer_unused;
    space = JACK_GetBytesFreeSpace(audioid);

    if (space < 0)
    {
        VERBOSE(VB_IMPORTANT, "Jack returned negative number for Free Space...");
    }

    return space;
}

