/*
	tone.cpp

	(c) 2004 Paul Volkaerts
	
  Simple class to handle creation of audio tones
*/
#include <qapplication.h>
#include <qfile.h>
#include <qdialog.h>   
#include <qcursor.h>
#include <qdir.h>
#include <qimage.h>
#include <linux/soundcard.h>

#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <math.h>
using namespace std;

#include <linux/videodev.h>
#include <mythtv/mythcontext.h>
#include <mythtv/mythwidgets.h>
#include <mythtv/lcddevice.h>

#include "config.h"
#include "webcam.h"
#include "sipfsm.h"
#include "phoneui.h"
#include "tone.h"





Tone::Tone(int freqHz, int volume, int ms, QObject *parent, const char *name) : QObject(parent, name)
{
    spkDev = -1;
    Loop = false;
    audioTimer = 0;
    Samples = ms * 8; // PCM sampled at 8KHz
    toneBuffer = new short[Samples];

    for (int c=0; c<Samples; c++)
        toneBuffer[c] = (short)(sin(c * 2 * M_PI * freqHz / 8000) * volume);
}

Tone::Tone(const Tone &t, QObject *parent, const char *name) : QObject(parent, name)
{
    spkDev = -1;
    Loop = false;
    audioTimer = 0;
    Samples = t.Samples;
    toneBuffer = new short[Samples];
    memcpy(toneBuffer, t.toneBuffer, Samples*sizeof(short));
}

Tone::Tone(int ms, QObject *parent, const char *name) : QObject(parent, name)
{
    spkDev = -1;
    Loop = false;
    audioTimer = 0;
    Samples = ms * 8; // PCM sampled at 8KHz
    toneBuffer = new short[Samples];
    memset(toneBuffer, 0, Samples*sizeof(short));
}

Tone::Tone(wavfile &wav, QObject *parent, const char *name) : QObject(parent, name)
{
    spkDev = -1;
    Loop = false;
    audioTimer = 0;
    Samples = wav.samples();
    toneBuffer = new short[Samples];
    memcpy(toneBuffer, wav.getData(), Samples*sizeof(short));
}

Tone::~Tone()
{
    Stop();

    if (toneBuffer)
        delete toneBuffer;
    toneBuffer = 0;
}

void Tone::sum(int freqHz, int volume)
{
    for (int c=0; c<Samples; c++)
        toneBuffer[c] += (short)(sin(c * 2 * M_PI * freqHz / 8000) * volume);
}

Tone& Tone::operator+=(const Tone &rhs)
{
    if (rhs.Samples > 0)
    {
        short *tmp = toneBuffer;
        toneBuffer = new short[Samples+rhs.Samples];
        memcpy(toneBuffer, tmp, Samples*sizeof(short));
        memcpy(&toneBuffer[Samples], rhs.toneBuffer, rhs.Samples*sizeof(short));
        Samples += rhs.Samples;
        delete tmp;
    }
    return *this;
}


void Tone::Play(QString deviceName, bool loop)
{
    if (spkDev == -1)
    {
        spkDev = OpenSpeaker(deviceName);
        Loop = loop;
    
        if (spkDev >= 0)
        {
            int b;
            audio_buf_info info;
            ioctl(spkDev, SNDCTL_DSP_GETOSPACE, &info);

            playPtr = 0;
            if (info.bytes > (int)(Samples*sizeof(short)))
            {
                b = write(spkDev, (uchar *)toneBuffer, Samples*sizeof(short));
            }
            else
            {
                b = write(spkDev, (uchar *)toneBuffer, info.bytes);
                playPtr = info.bytes;
            }

            audioTimer = new QTimer(this);
            connect(audioTimer, SIGNAL(timeout()), this, SLOT(audioTimerExpiry()));
            audioTimer->start((b / sizeof(short)) / 8, true); // Note this is /9 instead of /8 to make the timer expire "before" the sound stops
        }
        else
            cout << "Could not open " << deviceName << " to play tone\n";
    }
}

void Tone::audioTimerExpiry()
{
    if ((Loop || (playPtr != 0)) && (spkDev >= 0))
    {
        int b;
        audio_buf_info info;
        ioctl(spkDev, SNDCTL_DSP_GETOSPACE, &info);

        if (info.bytes > (int)((Samples*sizeof(short))-playPtr))
        {
            b = write(spkDev, (uchar *)toneBuffer+playPtr, (Samples*sizeof(short))-playPtr);
            playPtr = 0;
        }
        else
        {
            b = write(spkDev, (uchar *)toneBuffer+playPtr, info.bytes);
            playPtr += info.bytes;
        }

        audioTimer->start((b / sizeof(short)) / 8, true); // Note this is /9 instead of /8 to make the timer expire "before" the sound stops
    }
    else 
        Stop();
}

void Tone::Stop()
{
    if (audioTimer)
    {
        audioTimer->stop();
        delete audioTimer;
        audioTimer = 0;
    }
    if (spkDev >= 0)
    {
        int r = 1;
        if (ioctl(spkDev, SNDCTL_DSP_RESET, &r) == -1)
            cerr << "Error terminating playback\n";
    }
    CloseSpeaker();
}

int Tone::OpenSpeaker(QString devName)
{
    int fd = open(devName, O_WRONLY, 0);
    if (fd == -1)
    {
        cerr << "Cannot open device " << devName << endl;
        return -1;
    }                  

    int format = AFMT_S16_LE;
    if (ioctl(fd, SNDCTL_DSP_SETFMT, &format) == -1)
    {
        cerr << "Error setting audio driver format\n";
        close(fd);
        return -1;
    }

    int channels = 1;
    if (ioctl(fd, SNDCTL_DSP_CHANNELS, &channels) == -1)
    {
        cerr << "Error setting audio driver num-channels\n";
        close(fd);
        return -1;
    }

    int speed = 8000; // 8KHz
    if (ioctl(fd, SNDCTL_DSP_SPEED, &speed) == -1)
    {
        cerr << "Error setting audio driver speed\n";
        close(fd);
        return -1;
    }

    if ((format != AFMT_S16_LE) || (channels != 1) || (speed != 8000))
    {
        cerr << "Error setting audio driver; " << format << ", " << channels << ", " << speed << endl;
        close(fd);
        return -1;
    }

/*    uint frag_size = 0x7FFF0010; // unlimited number of fragments; fragment size=65535 bytes (ok for up to 3 seconds)
    if (ioctl(fd, SNDCTL_DSP_SETFRAGMENT, &frag_size) == -1)
    {
        cerr << "Error setting audio fragment size\n";
        close(fd);
        return -1;
    }
*/
    int flags;
    if ((flags = fcntl(fd, F_GETFL, 0)) > 0) 
    {
        flags &= O_NDELAY;
        fcntl(fd, F_SETFL, flags);
    }
    return fd;
}

void Tone::CloseSpeaker()
{
    if (spkDev >= 0)
        close(spkDev);   // For some reason, this blocks for ~ 1s.  I have no idea why
    spkDev = -1;
}




