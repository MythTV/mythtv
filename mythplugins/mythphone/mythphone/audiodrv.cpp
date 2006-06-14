/*
	audiodrv.cpp

	(c) 2005 Paul Volkaerts

  Implementation of an Audio driver for the RTP class 

*/
#include <qapplication.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <iostream>

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
#else
#include <io.h>
#include <winsock2.h>
#include <sstream>
#include <Dsound.h>
#include "gcontext.h"
#endif

#include "rtp.h"
#include "audiodrv.h"

using namespace std;



////////////////////////////////////////////////////////////////////////////////////////////////////
//                                     AUDIO DRIVER BASE CLASS
////////////////////////////////////////////////////////////////////////////////////////////////////

int AudioDriver::WriteSilence(int samples)
{
#define MAX_SILENCE_BUFFER  320
    int byteCount=0;
    short silenceBuffer[MAX_SILENCE_BUFFER];
    memset(silenceBuffer, 0, sizeof(silenceBuffer));
    while (samples>0)
    {
        if (samples < MAX_SILENCE_BUFFER)
        {
            byteCount += Write(silenceBuffer, samples);
            samples = 0;
        }
        else
        {
            byteCount += Write(silenceBuffer, MAX_SILENCE_BUFFER);
            samples -= MAX_SILENCE_BUFFER;
        }
    }
    return byteCount;
}




////////////////////////////////////////////////////////////////////////////////////////////////////
//                                     WINDOWS WAVE AUDIO DRIVER
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef WIN32
waveAudioDriver::waveAudioDriver(QString s, QString m, int mCap) : AudioDriver(mCap)
{
    spkIndex = 0;
    spkInBuffer = 0;
    
    MicDevice = SpeakerDevice = WAVE_MAPPER;
    WAVEOUTCAPS AudioCap;
    int numAudioDevs = waveOutGetNumDevs();
    for (int i=0; i<=numAudioDevs; i++)
    {
        MMRESULT err = waveOutGetDevCaps(i, &AudioCap, sizeof(AudioCap));
        if ((err == MMSYSERR_NOERROR) && (s == AudioCap.szPname))
            SpeakerDevice = i;
    }

    WAVEINCAPS AudioInCap;
    numAudioDevs = waveInGetNumDevs();
    for (i=0; i<=numAudioDevs; i++)
    {
        MMRESULT err = waveInGetDevCaps(i, &AudioInCap, sizeof(AudioInCap));
        if ((err == MMSYSERR_NOERROR) && (m == AudioInCap.szPname))
            MicDevice = i;
    }
}


waveAudioDriver::~waveAudioDriver()
{
}

void waveAudioDriver::Open()
{
    openSpeaker();
    openMicrophone();
}

void waveAudioDriver::Close()
{
    closeSpeaker();
    closeMicrophone();
}

void waveAudioDriver::StartSpeaker()
{
    waveOutWrite(hSpeaker, &(spkBufferDescr[0]), sizeof(WAVEHDR));
    spkIndex = 0;
}

int waveAudioDriver::Write(short *data, int samples)
{
    int i = spkIndex % SPK_BUFFER_SIZE;
    int b = (spkIndex % (SPK_BUFFER_SIZE*NUM_SPK_BUFFERS))/SPK_BUFFER_SIZE;

    if ((i + samples) <= SPK_BUFFER_SIZE)
    {
        memcpy(&SpkBuffer[b][i], data, samples*sizeof(short));
    }
    else
    {
        int temp = SPK_BUFFER_SIZE - i;
        memcpy(&SpkBuffer[b][i], data, temp*sizeof(short));
        b = (b+1)%NUM_SPK_BUFFERS;
        memcpy(&SpkBuffer[b][0], &data[temp], (samples-temp)*sizeof(short));
    }

    spkIndex += samples;

    return (samples*sizeof(short));
}

bool waveAudioDriver::anyMicrophoneData()
{
    if ((micBufferDescr[micCurrBuffer].dwFlags & WHDR_DONE) != 0)
        return true;
    return false;
}

int waveAudioDriver::Read(short *buffer, int maxSamples)
{
    if (maxSamples < micCaptureSamples)
        return 0;
    if ((micBufferDescr[micCurrBuffer].dwFlags & WHDR_DONE) != 0)
    {
        memcpy(buffer, MicBuffer[micCurrBuffer], micCaptureSamples*sizeof(short));
        int NextBuffer = ((micCurrBuffer+1)%NUM_MIC_BUFFERS);
        waveInAddBuffer(hMicrophone, &(micBufferDescr[micCurrBuffer]), sizeof(WAVEHDR));
        micCurrBuffer = NextBuffer;
        return micCaptureSamples*sizeof(short);
    }
    else
        return 0;
}

int waveAudioDriver::msOutQueued()
{
    int msDelay=0;
    MMTIME sPosn;
    sPosn.wType = TIME_SAMPLES;
    waveOutGetPosition(hSpeaker, &sPosn, sizeof(MMTIME));
    if (sPosn.u.sample <= spkIndex)
        msDelay = (spkIndex-sPosn.u.sample) / PCM_SAMPLES_PER_MS;
    else 
        msDelay = 0; // Don't report negatives if we've played more than we've queued
    return msDelay;
}

int waveAudioDriver::samplesOutSpaceRemaining()
{
    int remainingSamples=0;
    MMTIME sPosn;
    sPosn.wType = TIME_SAMPLES;
    waveOutGetPosition(hSpeaker, &sPosn, sizeof(MMTIME));
    if (sPosn.u.sample <= spkIndex)
        remainingSamples = SPK_BUFFER_SIZE-(spkIndex-sPosn.u.sample);
    else 
        remainingSamples = 0;
    return remainingSamples;
}

bool waveAudioDriver::openMicrophone()
{
WAVEFORMATEX wfx;
int b;


    micCurrBuffer = 0;

    wfx.cbSize = 0;
    wfx.nAvgBytesPerSec = 16000;
    wfx.nBlockAlign = 2;
    wfx.nChannels = 1;
    wfx.nSamplesPerSec = 8000;
    wfx.wBitsPerSample = 16;
    wfx.wFormatTag = WAVE_FORMAT_PCM;

    // First just query to check there is a compatible Mic.
    if (waveInOpen(0, MicDevice, &wfx, 0, 0, WAVE_FORMAT_QUERY))
        return FALSE;

    // Now actually open the device
    if (waveInOpen(&hMicrophone, MicDevice, &wfx, 0, 0, CALLBACK_NULL))
        return FALSE;

    // Inform the wave device of where to put the data
    for (b=0; b<NUM_MIC_BUFFERS; b++)
    {
        micBufferDescr[b].lpData = (LPSTR)(MicBuffer[b]);
        micBufferDescr[b].dwBufferLength = micCaptureSamples * sizeof(short); 
        micBufferDescr[b].dwFlags = 0L;
        if (waveInPrepareHeader(hMicrophone, &(micBufferDescr[b]), sizeof(WAVEHDR)))
            return FALSE;
    }

    // Start recording the data into the buffer
    if (waveInStart(hMicrophone))
        return FALSE;

    // Send a buffer to fill
    for (b=0; b<NUM_MIC_BUFFERS; b++)
    {
        if (waveInAddBuffer(hMicrophone, &(micBufferDescr[b]), sizeof(WAVEHDR)))
            return FALSE;
    }

    return TRUE;	
}


bool waveAudioDriver::openSpeaker()
{
int b;

    spkInBuffer = 0;

    // Playback the buffer through the speakers
    WAVEFORMATEX wfx;
    wfx.cbSize = 0;
    wfx.nAvgBytesPerSec = 16000;
    wfx.nBlockAlign = 2;
    wfx.nChannels = 1;
    wfx.nSamplesPerSec = 8000;
    wfx.wBitsPerSample = 16;
    wfx.wFormatTag = WAVE_FORMAT_PCM;

    if (waveOutOpen(&hSpeaker, SpeakerDevice, &wfx, 0, 0L, WAVE_FORMAT_QUERY))
        return FALSE;

    if (waveOutOpen(&hSpeaker, SpeakerDevice, &wfx, 0, 0L, CALLBACK_NULL))
        return FALSE;

    for (b=0; b<NUM_SPK_BUFFERS; b++)
    {
        spkBufferDescr[b].lpData = (LPSTR)(SpkBuffer[b]);
        spkBufferDescr[b].dwBufferLength = SPK_BUFFER_SIZE * sizeof(short);
        spkBufferDescr[b].dwFlags = (b == 0 ? WHDR_BEGINLOOP : 0) | 
                                    (b == NUM_SPK_BUFFERS-1 ? WHDR_ENDLOOP : 0);
        spkBufferDescr[b].dwLoops = (b == 0 ? 0xFFFF : 0); // FIXME - is there not an "indefinite" option?
        spkBufferDescr[b].dwUser = 0;

        if (waveOutPrepareHeader(hSpeaker, &(spkBufferDescr[b]), sizeof(WAVEHDR)))
            return FALSE;

        memset(SpkBuffer[b], 0, SPK_BUFFER_SIZE*sizeof(short));
    }

    return true;
}


bool waveAudioDriver::closeMicrophone()
{
int b;

    if (waveInReset(hMicrophone))
        return FALSE;

    for (b=0; b<NUM_MIC_BUFFERS; b++)
    {
        if (waveInUnprepareHeader(hMicrophone, &(micBufferDescr[b]), sizeof(WAVEHDR)))
            return FALSE;
    }

    if (waveInClose(hMicrophone))
        return FALSE;

    return TRUE;
}


bool waveAudioDriver::closeSpeaker()
{
    if (waveOutReset(hSpeaker))
        return FALSE;

    for (int b=0; b<NUM_SPK_BUFFERS; b++)
    {
        if (waveOutUnprepareHeader(hSpeaker, &(spkBufferDescr[b]), sizeof(WAVEHDR)))
            return FALSE;
    }

    if (waveOutClose(hSpeaker))
        return FALSE;

    return TRUE;	
}


#endif



////////////////////////////////////////////////////////////////////////////////////////////////////
//                                WINDOWS DIRECT SOUND AUDIO DRIVER
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef WIN32

bool CALLBACK enumerateCallback(LPGUID lpGUID, LPCTSTR lpszDesc, LPCTSTR lpszDrvName, LPVOID lpContext)
{
    dsAudioDriver *audio = (dsAudioDriver *)lpContext;
    if (audio)
        audio->dsEnumerateCallback(lpGUID, lpszDesc, lpszDrvName);
    return true;
}

dsAudioDriver::dsAudioDriver(QString s, QString m, int mCap, HWND hMainWindow) : AudioDriver(mCap)
{
    spkName = s;
    micName = m;
    spkGuid = 0;
    micGuid = 0;
    spkDS = 0;
    micDS = 0;
    dsSpkBuffer = 0;
    dsMicBuffer = 0;
    lastPlayPos = -1;
    lastReadPos = 0;
    micBufferBytes = 0;
    playBufferWraps = 0;
    enumerateSpeaker = true;
    DirectSoundEnumerate((LPDSENUMCALLBACK) enumerateCallback, this);
    enumerateSpeaker = false;
    DirectSoundCaptureEnumerate((LPDSENUMCALLBACK) enumerateCallback, this);
    if (spkGuid != 0) 
    {
        if (DirectSoundCreate(spkGuid, &spkDS, 0) == DS_OK)
        {
            spkDS->SetCooperativeLevel(hMainWindow, DSSCL_PRIORITY);
        }
    }
    if (micGuid != 0) 
    {
        DirectSoundCaptureCreate(micGuid, &micDS, 0);
    }
}

dsAudioDriver::~dsAudioDriver()
{
    if (dsMicBuffer)
        closeMicrophone();
    if (dsSpkBuffer)
        closeSpeaker();

    if (micDS)
        micDS->Release();
    if (spkDS)
        spkDS->Release();
    spkDS = 0;
    micDS = 0;

    if (spkGuid)
        delete spkGuid;
    if (micGuid);
        delete micGuid;
    spkGuid = micGuid = 0;
}

void dsAudioDriver::Open()
{
    if (spkDS != 0)
        openSpeaker();
    if (micDS != 0)
        openMicrophone();
}

void dsAudioDriver::Close()
{
    if (spkDS != 0)
        closeSpeaker();
    if (micDS != 0)
        closeMicrophone();
}

void dsAudioDriver::StartSpeaker()
{
    spkByteIndex = 0;
    dsSpkBuffer->Play(0, 0, DSBPLAY_LOOPING);
}

int dsAudioDriver::Write(short *data, int samples)
{
    DWORD playPos, writePos, endPos;
    if (dsSpkBuffer->GetCurrentPosition(&playPos, &writePos) != DS_OK)
        return 0;

    if ((int)playPos < lastPlayPos) // Check for buffer wrap
        playBufferWraps++;
    lastPlayPos = playPos;

    int bytesToWrite = samples*sizeof(short);

    int i = spkByteIndex % DS_SPK_BUFFER_BYTES;

    // TODO - Check we are not writing into the protected area

    if ((i + bytesToWrite) <= DS_SPK_BUFFER_BYTES)
    {
        memcpy(dsSpkMemory+i, data, bytesToWrite);
    }
    else
    {
        int temp = DS_SPK_BUFFER_BYTES - i;
        memcpy(dsSpkMemory+i, data, temp);
        memcpy(dsSpkMemory, ((uchar *)data)+temp, bytesToWrite-temp);
    }

    spkByteIndex += bytesToWrite;

    return bytesToWrite;
}

bool dsAudioDriver::anyMicrophoneData()
{
    DWORD capPosn, readPosn;
    if (dsMicBuffer->GetCurrentPosition(&capPosn, &readPosn) != DS_OK)
        return false;    

    int bytesAvailable = (int)readPosn - lastReadPos;
    if (bytesAvailable < 0)
        bytesAvailable += micBufferBytes;

    return (bytesAvailable >= (micCaptureSamples * sizeof(short)) ? true : false);
}

int dsAudioDriver::Read(short *buffer, int maxSamples)
{
    if (maxSamples < micCaptureSamples)
        return 0;
    void *micBuffer1, *micBuffer2;
    DWORD len1, len2;
    HRESULT res;
    if ((res=dsMicBuffer->Lock(lastReadPos, micCaptureSamples*sizeof(short), &micBuffer1, &len1, &micBuffer2, &len2, 0)) == DS_OK)
    {
        if (micBuffer1)
            memcpy(buffer, micBuffer1, len1);
        if (micBuffer2)
            memcpy(((char *)buffer)+len1, micBuffer2, len2);
        dsMicBuffer->Unlock(micBuffer1, len1, micBuffer2, len2);
        lastReadPos = (lastReadPos+len1+len2) % micBufferBytes;
        return (len1+len2);
    }
    return 0;
}

int dsAudioDriver::msOutQueued()
{
    DWORD playPos, writePos, endPos;
    if (dsSpkBuffer->GetCurrentPosition(&playPos, &writePos) != DS_OK)
        return 0;

    if ((int)playPos < lastPlayPos) // Check for buffer wrap
        playBufferWraps++;
    lastPlayPos = playPos;
    int totalPlayedBytes = (playBufferWraps * DS_SPK_BUFFER_BYTES) + playPos;
    int bytesQueued = spkByteIndex - totalPlayedBytes;
    
    // If the play ptr has got ahead of the data ptr, just return zero
    if (bytesQueued < 0)
        bytesQueued = 0;

    return (bytesQueued / sizeof(short) / PCM_SAMPLES_PER_MS);
}

int dsAudioDriver::samplesOutSpaceRemaining()
{
    DWORD playPos, writePos, endPos;
    if (dsSpkBuffer->GetCurrentPosition(&playPos, &writePos) != DS_OK)
        return 0;

    if ((int)playPos < lastPlayPos) // Check for buffer wrap
        playBufferWraps++;
    lastPlayPos = playPos;
    int totalPlayedBytes = (playBufferWraps * DS_SPK_BUFFER_BYTES) + playPos;
    int bytesQueued = spkByteIndex - totalPlayedBytes;
    
    // If the play ptr has got ahead of the data ptr, just record zero bytes queued
    if (bytesQueued < 0)
        bytesQueued = 0;

    // TODO --- look at writePtr

    return ((DS_SPK_BUFFER_BYTES-bytesQueued) / sizeof(short));
}

void dsAudioDriver::dsEnumerateCallback(LPGUID lpGUID, LPCTSTR lpszDesc, LPCTSTR lpszDrvName)
{
    if (enumerateSpeaker)
    {
        if ((spkName == lpszDesc) && (spkGuid == 0))
        {
            spkGuid = new GUID;
            memcpy(spkGuid, lpGUID, sizeof(GUID));
        }
    }
    else
    {
        if ((micName == lpszDesc) && (micGuid == 0))
        {
            micGuid = new GUID;
            memcpy(micGuid, lpGUID, sizeof(GUID));
        }
    }
}

bool dsAudioDriver::openMicrophone()
{
    DSCBUFFERDESC dscBufferDescr; 
    WAVEFORMATEX wf;
 
    // Set up wave format structure. 
    memset(&wf, 0, sizeof(WAVEFORMATEX)); 
    wf.wFormatTag = WAVE_FORMAT_PCM; 
    wf.nChannels = 1; 
    wf.nSamplesPerSec = 8000; 
    wf.nBlockAlign = 2; 
    wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign; 
    wf.wBitsPerSample = 16; 
 
    // Set up DSBUFFERDESC structure. 
    micBufferBytes = DS_NUM_MIC_BUFFERS * micCaptureSamples * sizeof(short);
    memset(&dscBufferDescr, 0, sizeof(DSCBUFFERDESC));
    dscBufferDescr.dwSize = sizeof(DSCBUFFERDESC); 
    dscBufferDescr.dwFlags = 0;
    dscBufferDescr.dwBufferBytes = micBufferBytes;
    dscBufferDescr.lpwfxFormat = &wf;
 
    if (micDS->CreateCaptureBuffer(&dscBufferDescr, &dsMicBuffer, 0) != DS_OK)
        return false;

    // Start capturing
    dsMicBuffer->Start(DSCBSTART_LOOPING);

    return true;
}


bool dsAudioDriver::openSpeaker()
{
    DSBUFFERDESC dsBufferDescr; 
    WAVEFORMATEX wf;
 
    // Set up wave format structure. 
    memset(&wf, 0, sizeof(WAVEFORMATEX)); 
    wf.wFormatTag = WAVE_FORMAT_PCM; 
    wf.nChannels = 1; 
    wf.nSamplesPerSec = 8000; 
    wf.nBlockAlign = 2; 
    wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign; 
    wf.wBitsPerSample = 16; 
 
    // Set up DSBUFFERDESC structure. 
    memset(&dsBufferDescr, 0, sizeof(DSBUFFERDESC));
    dsBufferDescr.dwSize = sizeof(DSBUFFERDESC); 
    dsBufferDescr.dwFlags = DSBCAPS_CTRLPOSITIONNOTIFY | 
                            DSBCAPS_GETCURRENTPOSITION2 | 
                            DSBCAPS_GLOBALFOCUS; 
    dsBufferDescr.dwBufferBytes = DS_SPK_BUFFER_BYTES; 
    dsBufferDescr.lpwfxFormat = &wf;
 
    if (spkDS->CreateSoundBuffer(&dsBufferDescr, &dsSpkBuffer, 0) != DS_OK)
        return false;
        
    DWORD audioBytes1;
    void *audioPtr2;
    DWORD audioBytes2;
    if (dsSpkBuffer->Lock(0, 0, (void **)&dsSpkMemory, &audioBytes1, &audioPtr2, &audioBytes2, DSBLOCK_ENTIREBUFFER ) != DS_OK)
        return false;

    memset(dsSpkMemory, 0, audioBytes1);
    if (audioPtr2)
        memset(audioPtr2, 0, audioBytes2);

    if (dsSpkBuffer->Unlock(dsSpkMemory, audioBytes1, audioPtr2, audioBytes2) != DS_OK)
        return false;

    return true;
}


bool dsAudioDriver::closeMicrophone()
{
    if (dsMicBuffer)
    {
        dsMicBuffer->Stop();
        dsMicBuffer->Release();
        dsMicBuffer = 0;
    }

    return true;
}


bool dsAudioDriver::closeSpeaker()
{
    if (dsSpkBuffer) 
    {
        dsSpkBuffer->Stop();
        dsSpkBuffer->Release();
        dsSpkBuffer = 0;
    }
    return true;
}


#endif



////////////////////////////////////////////////////////////////////////////////////////////////////
//                                     LINUX OSS AUDIO DRIVER
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef WIN32
ossAudioDriver::ossAudioDriver(QString s, QString m, int mCap) : AudioDriver(mCap)
{
    speakerFd    = -1;
    microphoneFd = -1;
    spkDevice    = s;
    micDevice    = m;
    readAnyData  = false;
}

ossAudioDriver::~ossAudioDriver()
{
}

void ossAudioDriver::Open()
{
    // Open the audio devices
    if (spkDevice == micDevice)
        microphoneFd = speakerFd = OpenAudioDevice(spkDevice, O_RDWR);    
    else
    {
        if (spkDevice.length() > 0)
            speakerFd = OpenAudioDevice(spkDevice, O_WRONLY);    

        if ((micDevice.length() > 0) && (micDevice != "None"))
            microphoneFd = OpenAudioDevice(micDevice, O_RDONLY);    
    }
}

void ossAudioDriver::Close()
{
    if (speakerFd > 0)
        close(speakerFd);
    if ((microphoneFd != speakerFd) && (microphoneFd > 0))
        close(microphoneFd);

    speakerFd = -1;
    microphoneFd = -1;
}

void ossAudioDriver::StartSpeaker()
{
    // No action required, writing causes it to start
}

int ossAudioDriver::Write(short *data, int samples)
{
    return write(speakerFd, data, samples*sizeof(short));
}

bool ossAudioDriver::anyMicrophoneData()
{
    if (!readAnyData) // Need to have done a read for this call to work
        return true;
    audio_buf_info info;
    ioctl(microphoneFd, SNDCTL_DSP_GETISPACE, &info);
    if (info.bytes > (int)(micCaptureSamples*sizeof(short)))
        return true;
    return false;
}

int ossAudioDriver::Read(short *buffer, int maxSamples)
{
    if (maxSamples < micCaptureSamples)
        return 0;
    readAnyData = true;
    return read(microphoneFd, (char *)buffer, micCaptureSamples*sizeof(short));
}

int ossAudioDriver::msOutQueued()
{
    // In Linux / OSS -- the most accurate reading you can get is using ODELAY, which can miss the 
    // fragment being DMA'ed out
    int bytesQueued;
    ioctl(speakerFd, SNDCTL_DSP_GETODELAY, &bytesQueued);
    return (bytesQueued / sizeof(short) / PCM_SAMPLES_PER_MS);
}

int ossAudioDriver::samplesOutSpaceRemaining()
{
    audio_buf_info info;
    ioctl(speakerFd, SNDCTL_DSP_GETOSPACE, &info);
    return (info.bytes / sizeof(short));
}

int ossAudioDriver::OpenAudioDevice(QString devName, int mode)
{
    int fd = open(devName, mode, 0);
    if (fd == -1)
    {
        cerr << "Cannot open device " << devName << endl;
        return -1;
    }                  

    // Set Full Duplex operation
    /*if (ioctl(fd, SNDCTL_DSP_SETDUPLEX, 0) == -1)
    {
        cerr << "Error setting audio driver duplex\n";
        close(fd);
        return -1;
    }*/

    int format = AFMT_S16_LE;//AFMT_MU_LAW;
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

    if ((format != AFMT_S16_LE/*AFMT_MU_LAW*/) || (channels != 1) || (speed != 8000))
    {
        cerr << "Error setting audio driver; " << format << ", " << channels << ", " << speed << endl;
        close(fd);
        return -1;
    }

    uint frag_size = 0x7FFF0007; // unlimited number of fragments; fragment size=128 bytes (ok for most RTP sample sizes)
    if (ioctl(fd, SNDCTL_DSP_SETFRAGMENT, &frag_size) == -1)
    {
        cerr << "Error setting audio fragment size\n";
        close(fd);
        return -1;
    }

    int flags;
    if ((flags = fcntl(fd, F_GETFL, 0)) > 0) 
    {
        flags &= O_NDELAY;
        fcntl(fd, F_SETFL, flags);
    }

/*    audio_buf_info info;
    if ((ioctl(fd, SNDCTL_DSP_GETBLKSIZE, &frag_size) == -1) ||
        (ioctl(fd, SNDCTL_DSP_GETOSPACE, &info) == -1))
    {
        cerr << "Error getting audio driver fragment info\n";
        close(fd);
        return -1;
    }*/
    return fd;
}

#endif



////////////////////////////////////////////////////////////////////////////////////////////////////
//                       MYTHTV Speaker with OSS Mic AUDIO DRIVER
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef WIN32
mythAudioDriver::mythAudioDriver(QString s, QString m, int mCap) : AudioDriver(mCap)
{
    mythOutput = 0;
    microphoneFd = -1;
    spkDevice    = s;
    micDevice    = m;
    readAnyData  = false;
}

mythAudioDriver::~mythAudioDriver()
{
    if (mythOutput)
        delete mythOutput;
    mythOutput = 0;
}

void mythAudioDriver::Open()
{
    // Open the audio devices
    if (spkDevice == micDevice)
        cerr << "Cannot have matching spk and mic devices in this mode, should have chosen OSS mode\n";
    else
    {
        mythOutput = AudioOutput::OpenAudio(spkDevice, "default", 16, 1, 8000,
                                            AUDIOOUTPUT_TELEPHONY, true,
                                            false /* AC3/DTS pass through */);
        if (mythOutput)
        {
            mythOutput->SetBlocking(false);
            mythOutput->SetEffDsp(8000 * 100);
        }

        if ((micDevice.length() > 0) && (micDevice != "None"))
            microphoneFd = OpenAudioDevice(micDevice, O_RDONLY);    
    }
}

void mythAudioDriver::Close()
{
    if (mythOutput)
        delete mythOutput;
    mythOutput = 0;

    if (microphoneFd > 0)
        close(microphoneFd);
    microphoneFd = -1;
}

void mythAudioDriver::StartSpeaker()
{
    // No action required, writing causes it to start
}

int mythAudioDriver::Write(short *data, int samples)
{
    if (mythOutput)
    {
        mythOutput->AddSamples((char *)data, samples, 100); // Set timecode to a static, known value
        return samples*sizeof(short);
    }
    else
        return 0;
}

bool mythAudioDriver::anyMicrophoneData()
{
    if (!readAnyData) // Need to have done a read for this call to work
        return true;
    audio_buf_info info;
    ioctl(microphoneFd, SNDCTL_DSP_GETISPACE, &info);
    if (info.bytes > (int)(micCaptureSamples*sizeof(short)))
        return true;
    return false;
}

int mythAudioDriver::Read(short *buffer, int maxSamples)
{
    if (maxSamples < micCaptureSamples)
        return 0;
    readAnyData = true;
    return read(microphoneFd, (char *)buffer, micCaptureSamples*sizeof(short));
}

int mythAudioDriver::msOutQueued()
{
    if (mythOutput)
    {
        int ms = mythOutput->GetAudiotime();
        // Subtract static timecode set in write. Note that we set this to 100 in the write,
        // but write always adds the length of the packet written, which is 20ms in our case.
        // It does not really matter, we are just compensating for mythAudio not liking zero
        // values for the timecode
        return 120-ms; 
    }
    else
        return 0xFFFF;
}

int mythAudioDriver::samplesOutSpaceRemaining()
{
    return micCaptureSamples*20; // Always say there is enough room for more samples
}

int mythAudioDriver::OpenAudioDevice(QString devName, int mode)
{
    int fd = open(devName, mode, 0);
    if (fd == -1)
    {
        cerr << "Cannot open device " << devName << endl;
        return -1;
    }                  

    // Set Full Duplex operation
    /*if (ioctl(fd, SNDCTL_DSP_SETDUPLEX, 0) == -1)
    {
        cerr << "Error setting audio driver duplex\n";
        close(fd);
        return -1;
    }*/

    int format = AFMT_S16_LE;//AFMT_MU_LAW;
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

    if ((format != AFMT_S16_LE/*AFMT_MU_LAW*/) || (channels != 1) || (speed != 8000))
    {
        cerr << "Error setting audio driver; " << format << ", " << channels << ", " << speed << endl;
        close(fd);
        return -1;
    }

    uint frag_size = 0x7FFF0007; // unlimited number of fragments; fragment size=128 bytes (ok for most RTP sample sizes)
    if (ioctl(fd, SNDCTL_DSP_SETFRAGMENT, &frag_size) == -1)
    {
        cerr << "Error setting audio fragment size\n";
        close(fd);
        return -1;
    }

    int flags;
    if ((flags = fcntl(fd, F_GETFL, 0)) > 0) 
    {
        flags &= O_NDELAY;
        fcntl(fd, F_SETFL, flags);
    }

/*    audio_buf_info info;
    if ((ioctl(fd, SNDCTL_DSP_GETBLKSIZE, &frag_size) == -1) ||
        (ioctl(fd, SNDCTL_DSP_GETOSPACE, &info) == -1))
    {
        cerr << "Error getting audio driver fragment info\n";
        close(fd);
        return -1;
    }*/
    return fd;
}

#endif




