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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <iostream>
#include <math.h>

#ifndef WIN32
#include <netinet/in.h>
#include <linux/soundcard.h>
#include <unistd.h>
#include <fcntl.h>                                     
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/sockios.h>
#include <mythtv/mythcontext.h>
#include "config.h"
#include "phoneui.h"
#else
#include <io.h>
#include <winsock2.h>
#include <sstream>
#include "gcontext.h"
#endif

#include "webcam.h"
#include "sipfsm.h"
#include "tone.h"

using namespace std;


#ifndef M_PI
#define M_PI 3.1415926
#endif



TelephonyTones::TelephonyTones()
{
    // Create the tones required
    // UK Ringback tone is 400Hz+450Hz with cadence 0.4s on 0.2s off 0.4s on 2s off
    // US Ringback tone is 440Hz+480Hz with cadence 2s on 4s off; in case anyone feels the need to localise this
    Tone oneRing(400, 7000, 400);   // 400Hz for 400ms, volume 7000
    oneRing.sum(450, 7000);         // 450Hz for 400ms, volume 7000
    Tone silenceA(200);
    Tone silenceB(2000);
    
    tone[TONE_RINGBACK] = new Tone(oneRing);
    *tone[TONE_RINGBACK] += silenceA;
    *tone[TONE_RINGBACK] += oneRing;
    *tone[TONE_RINGBACK] += silenceB;

    // Make the DTMF component tones 
    Tone f1(697, 7000, 100);
    Tone f2(770, 7000, 100);
    Tone f3(852, 7000, 100);
    Tone f4(941, 7000, 100);

    toneDtmf[0] = new Tone(f4);
    toneDtmf[0]->sum(1336, 7000);
    toneDtmf[1] = new Tone(f1);
    toneDtmf[1]->sum(1209, 7000);
    toneDtmf[2] = new Tone(f1);
    toneDtmf[2]->sum(1336, 7000);
    toneDtmf[3] = new Tone(f1);
    toneDtmf[3]->sum(1477, 7000);
    toneDtmf[4] = new Tone(f2);
    toneDtmf[4]->sum(1209, 7000);
    toneDtmf[5] = new Tone(f2);
    toneDtmf[5]->sum(1336, 7000);
    toneDtmf[6] = new Tone(f2);
    toneDtmf[6]->sum(1477, 7000);
    toneDtmf[7] = new Tone(f3);
    toneDtmf[7]->sum(1209, 7000);
    toneDtmf[8] = new Tone(f3);
    toneDtmf[8]->sum(1336, 7000);
    toneDtmf[9] = new Tone(f3);
    toneDtmf[9]->sum(1477, 7000);
    toneDtmf[10] = new Tone(f4);   // STAR
    toneDtmf[10]->sum(1209, 7000);
    toneDtmf[11] = new Tone(f4);   // HASH
    toneDtmf[11]->sum(1477, 7000);

}

TelephonyTones::~TelephonyTones()
{
    for (int i=0; i<12; i++)
        delete toneDtmf[i];
    delete tone[TONE_RINGBACK];
}

Tone *TelephonyTones::TTone(ToneId Id)
{
    if (tone.contains(Id))
        return tone[Id];
    else
        return 0;
}

Tone *TelephonyTones::dtmf(int Id)
{
    if (toneDtmf.contains(Id))
        return toneDtmf[Id];
    else
        return 0;
}

Tone::Tone(int freqHz, int volume, int ms, QObject *parent, const char *name) : QObject(parent, name)
{
#ifndef WIN32
    hSpeaker = -1;
#else    
    hSpeaker = 0;
#endif    
    Loop = false;
    audioTimer = 0;
    Samples = ms * 8; // PCM sampled at 8KHz
    toneBuffer = new short[Samples];

    for (int c=0; c<Samples; c++)
        toneBuffer[c] = (short)(sin(c * 2 * M_PI * freqHz / 8000) * volume);
}

Tone::Tone(const Tone &t, QObject *parent, const char *name) : QObject(parent, name)
{
#ifndef WIN32
    hSpeaker = -1;
#else    
    hSpeaker = 0;
#endif    
    Loop = false;
    audioTimer = 0;
    Samples = t.Samples;
    toneBuffer = new short[Samples];
    memcpy(toneBuffer, t.toneBuffer, Samples*sizeof(short));
}

Tone::Tone(int ms, QObject *parent, const char *name) : QObject(parent, name)
{
#ifndef WIN32
    hSpeaker = -1;
#else    
    hSpeaker = 0;
#endif    
    Loop = false;
    audioTimer = 0;
    Samples = ms * 8; // PCM sampled at 8KHz
    toneBuffer = new short[Samples];
    memset(toneBuffer, 0, Samples*sizeof(short));
}

Tone::Tone(wavfile &wav, QObject *parent, const char *name) : QObject(parent, name)
{
#ifndef WIN32
    hSpeaker = -1;
#else    
    hSpeaker = 0;
#endif    
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
#ifdef WIN32
    if (hSpeaker == 0)
    {
        OpenSpeaker(deviceName);
        spkBufferDescr.lpData = (LPSTR)toneBuffer;
        spkBufferDescr.dwBufferLength = Samples * sizeof(short);
        if (loop)
        {
            spkBufferDescr.dwFlags = WHDR_BEGINLOOP | WHDR_ENDLOOP;
            spkBufferDescr.dwLoops = 0xFFFF;
        }
        else
        {
            spkBufferDescr.dwFlags = 0L;
            spkBufferDescr.dwLoops = 0L;
        }
    
        waveOutPrepareHeader(hSpeaker, &spkBufferDescr, sizeof(WAVEHDR));
        waveOutWrite(hSpeaker, &spkBufferDescr, sizeof(WAVEHDR));
    }
#else
    if (hSpeaker == -1)
    {
        OpenSpeaker(deviceName);
        Loop = loop;
    
        if (hSpeaker >= 0)
        {
            int b;
            audio_buf_info info;
            ioctl(hSpeaker, SNDCTL_DSP_GETOSPACE, &info);

            playPtr = 0;
            if (info.bytes > (int)(Samples*sizeof(short)))
            {
                b = write(hSpeaker, (uchar *)toneBuffer, Samples*sizeof(short));
            }
            else
            {
                b = write(hSpeaker, (uchar *)toneBuffer, info.bytes);
                playPtr = info.bytes;
            }

            audioTimer = new QTimer(this);
            connect(audioTimer, SIGNAL(timeout()), this, SLOT(audioTimerExpiry()));
            audioTimer->start((b / sizeof(short)) / 8, true); // Note this is /9 instead of /8 to make the timer expire "before" the sound stops
        }
        else
            cout << "Could not open " << deviceName << " to play tone\n";
    }
#endif    
}

void Tone::audioTimerExpiry()
{
#ifndef WIN32
    if ((Loop || (playPtr != 0)) && (hSpeaker >= 0))
    {
        int b;
        audio_buf_info info;
        ioctl(hSpeaker, SNDCTL_DSP_GETOSPACE, &info);

        if (info.bytes > (int)((Samples*sizeof(short))-playPtr))
        {
            b = write(hSpeaker, (uchar *)toneBuffer+playPtr, (Samples*sizeof(short))-playPtr);
            playPtr = 0;
        }
        else
        {
            b = write(hSpeaker, (uchar *)toneBuffer+playPtr, info.bytes);
            playPtr += info.bytes;
        }

        audioTimer->start((b / sizeof(short)) / 8, true); // Note this is /9 instead of /8 to make the timer expire "before" the sound stops
    }
    else 
        Stop();
#endif
}

void Tone::Stop()
{
    if (audioTimer)
    {
        audioTimer->stop();
        delete audioTimer;
        audioTimer = 0;
    }
#ifndef WIN32
    if (hSpeaker >= 0)
    {
        int r = 1;
        if (ioctl(hSpeaker, SNDCTL_DSP_RESET, &r) == -1)
            cerr << "Error terminating playback\n";
    }
#endif    
    CloseSpeaker();
}

void Tone::OpenSpeaker(QString devName)
{
#ifdef WIN32
    unsigned int dwResult;
    
    // Playback the buffer through the speakers
    WAVEFORMATEX wfx;
    wfx.cbSize = 0;
    wfx.nAvgBytesPerSec = 16000;
    wfx.nBlockAlign = 2;
    wfx.nChannels = 1;
    wfx.nSamplesPerSec = 8000;
    wfx.wBitsPerSample = 16;
    wfx.wFormatTag = WAVE_FORMAT_PCM;

    unsigned int SpeakerDevice = WAVE_MAPPER;
    WAVEOUTCAPS AudioCap;
    int numAudioDevs = waveOutGetNumDevs();
    for (int i=0; i<=numAudioDevs; i++)
    {
        MMRESULT err = waveOutGetDevCaps(i, &AudioCap, sizeof(AudioCap));
        if ((err == MMSYSERR_NOERROR) && (devName == AudioCap.szPname))
            SpeakerDevice = i;
    }

    if (dwResult = waveOutOpen(&hSpeaker, SpeakerDevice, &wfx, 0, 0L, WAVE_FORMAT_QUERY))
        return;

    if (dwResult = waveOutOpen(&hSpeaker, SpeakerDevice, &wfx, 0, 0L, CALLBACK_NULL))
        return;

#else
    int fd = open(devName, O_WRONLY, 0);
    if (fd == -1)
    {
        cerr << "Cannot open device " << devName << endl;
        return;
    }                  

    int format = AFMT_S16_LE;
    if (ioctl(fd, SNDCTL_DSP_SETFMT, &format) == -1)
    {
        cerr << "Error setting audio driver format\n";
        close(fd);
        return;
    }

    int channels = 1;
    if (ioctl(fd, SNDCTL_DSP_CHANNELS, &channels) == -1)
    {
        cerr << "Error setting audio driver num-channels\n";
        close(fd);
        return;
    }

    int speed = 8000; // 8KHz
    if (ioctl(fd, SNDCTL_DSP_SPEED, &speed) == -1)
    {
        cerr << "Error setting audio driver speed\n";
        close(fd);
        return;
    }

    if ((format != AFMT_S16_LE) || (channels != 1) || (speed != 8000))
    {
        cerr << "Error setting audio driver; " << format << ", " << channels << ", " << speed << endl;
        close(fd);
        return;
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
    hSpeaker = fd;
#endif
}

void Tone::CloseSpeaker()
{
#ifdef WIN32
    unsigned int dwResult;
    
    if (dwResult = waveOutReset(hSpeaker))
        return;

    if (dwResult = waveOutUnprepareHeader(hSpeaker, &spkBufferDescr, sizeof(WAVEHDR)))
        return;

    if (dwResult = waveOutClose(hSpeaker))
        return;
    hSpeaker = 0;
#else
    if (hSpeaker >= 0)
        close(hSpeaker);   // For some reason, this blocks for ~ 1s.  I have no idea why
    hSpeaker = -1;
#endif
}


bool Tone::Playing() 
{
#ifdef WIN32
    return (hSpeaker != 0); 
#else
    return (hSpeaker != -1); 
#endif    
};


