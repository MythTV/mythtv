#include <iostream>
#include <cmath>
//#include <stdlib.h>

using namespace std;

#include "mythverbose.h"
#include "audiooutputdx.h"

#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>

#define LOC QString("AODX: ")
#define LOC_ERR QString("AODX, ERROR: ")

const uint AudioOutputDX::kFramesNum = 32;

#include <initguid.h>
DEFINE_GUID(IID_IDirectSoundNotify, 0xb0210783, 0x89cd, 0x11d0, 0xaf, 0x8, 0x0,
                                    0xa0, 0xc9, 0x25, 0xcd, 0x16);

#ifndef WAVE_FORMAT_DOLBY_AC3_SPDIF
#   define WAVE_FORMAT_DOLBY_AC3_SPDIF 0x0092
#endif

#ifndef WAVE_FORMAT_EXTENSIBLE
#define  WAVE_FORMAT_EXTENSIBLE   0xFFFE
#endif

#ifndef _WAVEFORMATEXTENSIBLE_
typedef struct {
    WAVEFORMATEX    Format;
    union {
        WORD wValidBitsPerSample;       /* bits of precision  */
        WORD wSamplesPerBlock;          /* valid if wBitsPerSample==0 */
        WORD wReserved;                 /* If neither applies, set to zero. */
    } Samples;
    DWORD           dwChannelMask;      /* which channels are */
                                        /* present in stream  */
    GUID            SubFormat;
} WAVEFORMATEXTENSIBLE, *PWAVEFORMATEXTENSIBLE;
#endif

DEFINE_GUID( _KSDATAFORMAT_SUBTYPE_PCM, WAVE_FORMAT_PCM, 0x0000, 0x0010, 0x80,
              0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 );
DEFINE_GUID( _KSDATAFORMAT_SUBTYPE_DOLBY_AC3_SPDIF, WAVE_FORMAT_DOLBY_AC3_SPDIF,
              0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 );

class AudioOutputDXPrivate
{
    public:
        AudioOutputDXPrivate(AudioOutputDX *in_parent) :
            parent(in_parent),
            dsound_dll(NULL),
            dsobject(NULL),
            dsbuffer(NULL),
            playStarted(false),
            writeCursor(0),
            lastValidTime(0),
            chosenGUID(NULL),
            device_count(0),
            device_num(0)
        {
        }

        ~AudioOutputDXPrivate()
        {
            DestroyDSBuffer();

            if (dsobject)
                IDirectSound_Release(dsobject);
    
            if (dsound_dll)
               FreeLibrary(dsound_dll);
        }

        int InitDirectSound(void);
        void ResetDirectSound(void);        
        void DestroyDSBuffer(void);
        void FillBuffer(unsigned char *buffer, int size);
        bool StartPlayback(void);
        void StopPlayback(bool reset);
        static int CALLBACK DSEnumCallback(LPGUID lpGuid,
                LPCSTR lpcstrDesc, LPCSTR lpcstrModule, LPVOID lpContext);

    public:
        AudioOutputDX *parent;
        HINSTANCE dsound_dll;
        LPDIRECTSOUND dsobject;
        LPDIRECTSOUNDBUFFER dsbuffer;
        bool playStarted;        
        DWORD writeCursor;        
        DWORD lastValidTime;        
        GUID deviceGUID, *chosenGUID;
        int device_count, device_num;
        QString device_name;        
};


AudioOutputDX::AudioOutputDX(const AudioSettings &settings) :
    AudioOutputBase(settings),
    m_priv(new AudioOutputDXPrivate(this)),
    m_UseSPDIF(settings.use_passthru)
{
    timeBeginPeriod(1);
    Reconfigure(settings);
}

AudioOutputDX::~AudioOutputDX()
{
    KillAudio();

    if (m_priv)
    {
        delete m_priv;
        m_priv = NULL;
    }
    timeEndPeriod(1);
}

typedef HRESULT (WINAPI *LPFNDSC) (LPGUID, LPDIRECTSOUND *, LPUNKNOWN);
typedef HRESULT (WINAPI *LPFNDSE) (LPDSENUMCALLBACK, LPVOID);

int CALLBACK AudioOutputDXPrivate::DSEnumCallback(LPGUID lpGuid, 
        LPCSTR lpcstrDesc, LPCSTR lpcstrModule, LPVOID lpContext)
{
    const QString enum_desc = lpcstrDesc,
                  cfg_desc = ((AudioOutputDXPrivate*)lpContext)->device_name;
    const int device_num = ((AudioOutputDXPrivate*)lpContext)->device_num,
              device_count = ((AudioOutputDXPrivate*)lpContext)->device_count;

    VERBOSE(VB_AUDIO, QString(LOC + "Device %1:" + enum_desc)
            .arg(device_count));

    if ((device_num == device_count) ||
        (device_num == 0 && !cfg_desc.isEmpty() &&
        enum_desc.startsWith(cfg_desc, Qt::CaseInsensitive)))
    {
        if (lpGuid)
        {
            ((AudioOutputDXPrivate*)lpContext)->deviceGUID = *lpGuid;
            ((AudioOutputDXPrivate*)lpContext)->chosenGUID =
                &(((AudioOutputDXPrivate*)lpContext)->deviceGUID);
            ((AudioOutputDXPrivate*)lpContext)->device_name = enum_desc;
            ((AudioOutputDXPrivate*)lpContext)->device_num = device_count;
        }
    }

    ((AudioOutputDXPrivate*)lpContext)->device_count++;
    return 1;
}

void AudioOutputDXPrivate::ResetDirectSound(void)
{
    DestroyDSBuffer();

    if (dsobject)
    {
        IDirectSound_Release(dsobject);
        dsobject = NULL;
    }
 
    if (dsound_dll)
    {
       FreeLibrary(dsound_dll);
       dsound_dll = NULL;
    }

    chosenGUID   = NULL;
    device_count = 0;
    device_num   = 0;
}

int AudioOutputDXPrivate::InitDirectSound(void)
{
    LPFNDSC OurDirectSoundCreate;
    LPFNDSE OurDirectSoundEnumerate;
    bool ok;

    ResetDirectSound();

    dsound_dll = LoadLibrary("DSOUND.DLL");
    if (dsound_dll == NULL )
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Cannot open DSOUND.DLL" );
        goto error;
    }

    device_name = (parent->m_UseSPDIF) ?
        parent->audio_passthru_device : parent->audio_main_device;
    device_name = device_name.section(':', 1);
    device_num = device_name.toInt(&ok, 10);
    VERBOSE(VB_AUDIO, LOC + QString("Looking for device num:%1 or name:%2")
            .arg(device_num).arg(device_name));

    OurDirectSoundEnumerate =
        (LPFNDSE)GetProcAddress(dsound_dll, "DirectSoundEnumerateA" );
    if(OurDirectSoundEnumerate)
    {
        if(FAILED(OurDirectSoundEnumerate(DSEnumCallback, this)))
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "DirectSoundEnumerate FAILED" );
        }
    }

    if (!chosenGUID)
    {
        device_num = 0;
        device_name = "Primary Sound Driver";
    }
    VERBOSE(VB_AUDIO, LOC + QString("Using device %1:%2")
            .arg(device_num).arg(device_name));

    OurDirectSoundCreate = 
        (LPFNDSC)GetProcAddress(dsound_dll, "DirectSoundCreate");
    if (OurDirectSoundCreate == NULL)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "GetProcAddress FAILED" );
        goto error;
    }

    if (FAILED(OurDirectSoundCreate(chosenGUID, &dsobject, NULL)))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "cannot create a direct sound device" );
        goto error;
    }

    /* Set DirectSound Cooperative level, ie what control we want over Windows
     * sound device. In our case, DSSCL_EXCLUSIVE means that we can modify the
     * settings of the primary buffer, but also that only the sound of our
     * application will be hearable when it will have the focus.
     * !!! (this is not really working as intended yet because to set the
     * cooperative level you need the window handle of your application, and
     * I don't know of any easy way to get it. Especially since we might play
     * sound without any video, and so what window handle should we use ???
     * The hack for now is to use the Desktop window handle - it seems to be
     * working */
    if (FAILED(IDirectSound_SetCooperativeLevel(dsobject,
                                         GetDesktopWindow(),
                                         DSSCL_EXCLUSIVE)))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Cannot set DS cooperative level" );
    }

    VERBOSE(VB_AUDIO, LOC + "Initialised DirectSound");

    return 0;

 error:
    dsobject = NULL;
    if (dsound_dll)
    {
        FreeLibrary(dsound_dll);
        dsound_dll = NULL;
    }
    return -1;
}

void AudioOutputDXPrivate::DestroyDSBuffer(void)
{
    if (dsbuffer)
    {
        VERBOSE(VB_AUDIO, LOC + "Destroying DirectSound buffer");
        StopPlayback(true);
        IDirectSoundBuffer_Release(dsbuffer);
        dsbuffer = NULL;
    }
}

void AudioOutputDXPrivate::FillBuffer(unsigned char *buffer, int size)
{
    void *p_write_position, *p_wrap_around;
    DWORD l_bytes1, l_bytes2;
    HRESULT dsresult;

    dsresult = IDirectSoundBuffer_Lock(
                dsbuffer,                /* DS buffer */
                writeCursor,             /* Start offset */
                size,                    /* Number of bytes */
                &p_write_position,       /* Address of lock start */
                &l_bytes1,               /* Bytes locked before wrap */
                &p_wrap_around,          /* Buffer address (if wrap around) */
                &l_bytes2,               /* Count of bytes after wrap around */
                0);                      /* Flags */
    if (dsresult == DSERR_BUFFERLOST)
    {
        IDirectSoundBuffer_Restore(dsbuffer);
        dsresult = IDirectSoundBuffer_Lock(dsbuffer, writeCursor, size,
                                           &p_write_position, &l_bytes1,
                                           &p_wrap_around, &l_bytes2, 0);
    }

    if (dsresult != DS_OK)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Cannot lock buffer, audio dropped");
        return;
    }

    if (buffer == NULL)
    {
        VERBOSE(VB_AUDIO, LOC + "Writing null bytes");
    
        memset(p_write_position, 0, l_bytes1);
        if (p_wrap_around)
            memset(p_wrap_around, 0, l_bytes2);
            
        writeCursor += l_bytes1 + l_bytes2;
           
        while (writeCursor >= (DWORD)parent->soundcard_buffer_size)
            writeCursor -= parent->soundcard_buffer_size;
    }
    else    
    {
        memcpy(p_write_position, buffer, l_bytes1);
        if (p_wrap_around) {
            memcpy(p_wrap_around, buffer + l_bytes1, l_bytes2);
        }
            
        writeCursor += l_bytes1 + l_bytes2;
           
        while (writeCursor >= (DWORD)parent->soundcard_buffer_size)
            writeCursor -= parent->soundcard_buffer_size;
    }

    IDirectSoundBuffer_Unlock(dsbuffer, p_write_position, l_bytes1,
                              p_wrap_around, l_bytes2 );
}

bool AudioOutputDXPrivate::StartPlayback(void)
{
    HRESULT dsresult;
    lastValidTime = 0;

    dsresult = IDirectSoundBuffer_Play(dsbuffer, 0, 0, DSBPLAY_LOOPING);
    if (dsresult == DSERR_BUFFERLOST)
    {
        IDirectSoundBuffer_Restore(dsbuffer);
        dsresult = IDirectSoundBuffer_Play(dsbuffer, 0, 0, DSBPLAY_LOOPING );
    }
    if (dsresult != DS_OK)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Cannot start playing buffer" );
        playStarted=false;
        return false;
    }
    playStarted=true;
    
    // set buffer expiration time (adding 1ms due to integer math)
    lastValidTime = 1 + timeGetTime() + (parent->GetBufferedOnSoundcard() * 
            1000 / (parent->audio_bytes_per_sample * parent->audio_samplerate));

    return true;
}

void AudioOutputDXPrivate::StopPlayback(bool reset)
{
    IDirectSoundBuffer_Stop(dsbuffer);
    lastValidTime=0;
    if (reset)
    {
        writeCursor = 0;
        IDirectSoundBuffer_SetCurrentPosition(dsbuffer, writeCursor);
        FillBuffer(NULL, parent->soundcard_buffer_size);
        playStarted=false;
    }
}

vector<int> AudioOutputDX::GetSupportedRates(void)
{
    const int srates[] = { 8000, 11025, 16000, 22050, 32000, 44100, 48000 };
    vector<int> rates(srates, srates + sizeof(srates) / sizeof(int) );
    m_UseSPDIF = audio_passthru || audio_enc;
    if (m_UseSPDIF)
    {
        rates.clear();
        rates.push_back(48000);
    }

    DSCAPS devcaps;
    devcaps.dwSize = sizeof(DSCAPS);

    m_priv->InitDirectSound();
    if ((!m_priv->dsobject || !m_priv->dsound_dll) ||
        FAILED(IDirectSound_GetCaps(m_priv->dsobject, &devcaps)) )
    {
        rates.clear();
        return rates;
    }

    VERBOSE(VB_AUDIO, QString(LOC + "GetCaps sample rate min: %1 max: %2")
                .arg(devcaps.dwMinSecondarySampleRate)
                .arg(devcaps.dwMaxSecondarySampleRate));

    vector<int>::iterator it = rates.begin();
    while (it != rates.end())
    {
        if(((DWORD)*it < devcaps.dwMinSecondarySampleRate) ||
           ((DWORD)*it > devcaps.dwMaxSecondarySampleRate) )
            it = rates.erase(it);
        else
            it++;
    }

    return rates;
}

bool AudioOutputDX::OpenDevice(void)
{
    WAVEFORMATEXTENSIBLE wf;
    DSBUFFERDESC         dsbdesc;

    CloseDevice();

    m_UseSPDIF = audio_passthru || audio_enc;
    m_priv->InitDirectSound();
    if (!m_priv->dsobject || !m_priv->dsound_dll)
    {
        Error("DirectSound initialization failed");
        return false;
    }

    SetBlocking(true);
    fragment_size = (source == AUDIOOUTPUT_TELEPHONY) ? 320 : 6144;
    if (audio_channels > 2) 
        fragment_size *= 2;
    soundcard_buffer_size = kFramesNum * fragment_size;
    audio_bytes_per_sample = audio_bits / 8 * audio_channels;
    if (m_UseSPDIF && (audio_channels != 2))
    {
        Error("SPDIF passthru requires 2 channel data");
        return false;
    }

    wf.Format.wFormatTag =
        (m_UseSPDIF) ? WAVE_FORMAT_DOLBY_AC3_SPDIF : WAVE_FORMAT_PCM;
    wf.Format.nChannels = audio_channels;
    wf.Format.nSamplesPerSec = audio_samplerate;
    wf.Format.nBlockAlign = audio_bytes_per_sample;
    wf.Format.nAvgBytesPerSec = 
        wf.Format.nSamplesPerSec * wf.Format.nBlockAlign;
    wf.Format.wBitsPerSample = audio_bits;
    wf.Samples.wValidBitsPerSample = wf.Format.wBitsPerSample;
    wf.SubFormat = (m_UseSPDIF) ? _KSDATAFORMAT_SUBTYPE_DOLBY_AC3_SPDIF 
                                : _KSDATAFORMAT_SUBTYPE_PCM;
 
    VERBOSE(VB_AUDIO, LOC + QString("New format: %1bits %2ch %3Hz %4")
            .arg(audio_bits).arg(audio_channels).arg(audio_samplerate)
            .arg((m_UseSPDIF) ? "data" : "PCM"));

    if (audio_channels <= 2)
    {
        wf.Format.cbSize = 0;
    }
    else
    {
        wf.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        wf.dwChannelMask = 0x003F;          // 0x003F = 5.1 channels
        wf.Format.cbSize =
            sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    }

    memset(&dsbdesc, 0, sizeof(DSBUFFERDESC));
    dsbdesc.dwSize = sizeof(DSBUFFERDESC);
    dsbdesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2/* Better position accuracy */
                    | DSBCAPS_GLOBALFOCUS       /* Allows background playing */
                    | DSBCAPS_LOCHARDWARE;      /* Needed for 5.1 on emu101k */
    if (!m_UseSPDIF)
        dsbdesc.dwFlags |= DSBCAPS_CTRLVOLUME;  /* Allow volume control */
    dsbdesc.dwBufferBytes = soundcard_buffer_size;   /* buffer size */
    dsbdesc.lpwfxFormat = (WAVEFORMATEX *)&wf;

    if FAILED(IDirectSound_CreateSoundBuffer(
                m_priv->dsobject, &dsbdesc, &m_priv->dsbuffer, NULL))
    {
        /* Vista does not support hardware mixing */
        /* Try without DSBCAPS_LOCHARDWARE */
        dsbdesc.dwFlags &= ~DSBCAPS_LOCHARDWARE;
        HRESULT dsresult = IDirectSound_CreateSoundBuffer(
                m_priv->dsobject, &dsbdesc, &m_priv->dsbuffer, NULL);
        if FAILED(dsresult)
        {
            if (dsresult == DSERR_UNSUPPORTED)
                Error(QString("Unsupported format for device %1:%2")
                        .arg(m_priv->device_num).arg(m_priv->device_name));
            else
                Error(QString("Failed to create DS buffer 0x%1")
                        .arg((DWORD)dsresult, 0, 16));
            return false;
        }
        VERBOSE(VB_AUDIO, LOC + "Using software mixer" );
    }
    VERBOSE(VB_AUDIO, LOC + "Created DirectSound buffer");

    return true;
}

void AudioOutputDX::CloseDevice(void)
{
    if (m_priv->dsbuffer)
        m_priv->DestroyDSBuffer();
}

void AudioOutputDX::WriteAudio(unsigned char * buffer, int size)
{
   	if (size == 0)
        return;

    m_priv->FillBuffer(buffer, size);
    if (!pauseaudio)
        m_priv->StartPlayback();
    else
        m_priv->playStarted = true;
}

void AudioOutputDX::Pause(bool pause)
{
    if (m_priv->dsbuffer)
    {
        if (pause)
        {
            m_priv->StopPlayback(false);
            VERBOSE(VB_AUDIO, LOC + "Buffer paused");
        }
        else
        {
            if (!m_priv->StartPlayback())
                VERBOSE(VB_IMPORTANT, LOC_ERR + "Unpause failed" );
            else
                VERBOSE(VB_AUDIO, LOC + "Buffer unpaused");
        }
    }

    AudioOutputBase::Pause(pause);
}

int AudioOutputDX::GetSpaceOnSoundcard(void) const
{
    if (!m_priv->playStarted)
        return soundcard_buffer_size;

    HRESULT dsresult;
    DWORD play_pos, write_pos;
    DWORD currentTime = timeGetTime();

    dsresult = IDirectSoundBuffer_GetCurrentPosition(
            m_priv->dsbuffer, &play_pos, &write_pos);
    if (dsresult != DS_OK)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Cannot get current buffer position");
	    return 0;
    }

    int buffer_free = (int)play_pos - (int)m_priv->writeCursor;
    if ( buffer_free < 0 )
        buffer_free += soundcard_buffer_size;

    if (((write_pos > play_pos) && (m_priv->writeCursor <= write_pos)
                && (m_priv->writeCursor > play_pos))
        || ((write_pos < play_pos) && ((m_priv->writeCursor <= write_pos)
                || (play_pos < m_priv->writeCursor))))
    {
        VERBOSE(VB_AUDIO, LOC + QString("buffer underrun(1) %1 %2 %3")
                    .arg(play_pos).arg(write_pos).arg(m_priv->writeCursor));
        // WriteCursor is in unsafe zone - stop playback for now
        // Next call to WriteAudio will restart the buffer
        m_priv->StopPlayback(true);
        buffer_free = soundcard_buffer_size;
    }
    else if (m_priv->lastValidTime && (currentTime > m_priv->lastValidTime))
    {
        VERBOSE(VB_AUDIO, LOC + QString("buffer underrun(2) %1 %2 %3 %4")
                    .arg(play_pos).arg(write_pos).arg(m_priv->writeCursor)
                    .arg(currentTime - m_priv->lastValidTime));
        m_priv->StopPlayback(true);
        buffer_free = soundcard_buffer_size;
    }

    return buffer_free;
}

int AudioOutputDX::GetBufferedOnSoundcard(void) const
{
    if (!m_priv->playStarted)
        return 0;

    HRESULT dsresult;
    DWORD play_pos;

    dsresult = IDirectSoundBuffer_GetCurrentPosition(m_priv->dsbuffer, 
            &play_pos, NULL);
    if (dsresult != DS_OK)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Cannot get current buffer position");
	    return 0;
    }

    int buffered = (int)m_priv->writeCursor - (int)play_pos;
    if (buffered <= 0)
        buffered += soundcard_buffer_size;
    return buffered;
}

int AudioOutputDX::GetVolumeChannel(int channel) const
{
    HRESULT dsresult;
    long dxVolume = 0;
    int volume;

    if (m_UseSPDIF)
        return 100;

    dsresult = IDirectSoundBuffer_GetVolume(m_priv->dsbuffer, &dxVolume);
    volume = (int)(pow(10,(float)dxVolume/20)*100);

    if (dsresult != DS_OK)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + QString("Failed to get volume %1")
                                                .arg(dxVolume));
        return volume;
    }

    VERBOSE(VB_AUDIO, LOC + QString("Got volume %1").arg(volume));
    return volume;
}

void AudioOutputDX::SetVolumeChannel(int channel, int volume)
{
    HRESULT dsresult;
    float dbAtten = 20 * log10((float)volume/100);
    long dxVolume = (volume == 0) ? DSBVOLUME_MIN : (long)(100.0f * dbAtten);

    if (m_UseSPDIF)
        return;

    // dxVolume is attenuation in 100ths of a decibel
    dsresult = IDirectSoundBuffer_SetVolume(m_priv->dsbuffer, dxVolume);

    if (dsresult != DS_OK)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + QString("Failed to set volume %1")
                                                .arg(dxVolume));
        return;
    }
    
    VERBOSE(VB_AUDIO, LOC + QString("Set volume %1").arg(dxVolume));
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
