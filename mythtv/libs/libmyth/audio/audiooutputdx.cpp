#include <iostream>
#include <cmath>

#include "libmythbase/mythlogging.h"
#include "audiooutputdx.h"

#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>
#include <unistd.h>

#define LOC QString("AODX: ")

#include <initguid.h>
DEFINE_GUID(IID_IDirectSoundNotify, 0xb0210783, 0x89cd, 0x11d0,
            0xaf, 0x8, 0x0, 0xa0, 0xc9, 0x25, 0xcd, 0x16);

#ifndef WAVE_FORMAT_DOLBY_AC3_SPDIF
#define WAVE_FORMAT_DOLBY_AC3_SPDIF 0x0092
#endif

#ifndef WAVE_FORMAT_IEEE_FLOAT
#define WAVE_FORMAT_IEEE_FLOAT 0x0003
#endif

#ifndef WAVE_FORMAT_EXTENSIBLE
#define WAVE_FORMAT_EXTENSIBLE 0xFFFE
#endif

#ifndef _WAVEFORMATEXTENSIBLE_
struct WAVEFORMATEXTENSIBLE {
    WAVEFORMATEX Format;
    union {
        WORD wValidBitsPerSample;       // bits of precision
        WORD wSamplesPerBlock;          // valid if wBitsPerSample==0
        WORD wReserved;                 // If neither applies, set to zero
    } Samples;
    DWORD        dwChannelMask;         // which channels are present in stream
    GUID         SubFormat;
};
using PWAVEFORMATEXTENSIBLE = WAVEFORMATEXTENSIBLE*;
#endif

DEFINE_GUID(_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, WAVE_FORMAT_IEEE_FLOAT,
            0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(_KSDATAFORMAT_SUBTYPE_PCM, WAVE_FORMAT_PCM,
            0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(_KSDATAFORMAT_SUBTYPE_DOLBY_AC3_SPDIF, WAVE_FORMAT_DOLBY_AC3_SPDIF,
            0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);

class AudioOutputDXPrivate
{
    public:
        explicit AudioOutputDXPrivate(AudioOutputDX *in_parent) :
            m_parent(in_parent) {}

        ~AudioOutputDXPrivate()
        {
            DestroyDSBuffer();

            if (m_dsobject)
                IDirectSound_Release(m_dsobject);

            if (m_dsound_dll)
               FreeLibrary(m_dsound_dll);
        }

        int InitDirectSound(bool passthrough = false);
        void ResetDirectSound(void);
        void DestroyDSBuffer(void);
        void FillBuffer(unsigned char *buffer, int size);
        bool StartPlayback(void);
#ifdef UNICODE
        static int CALLBACK DSEnumCallback(LPGUID  lpGuid,
                                           LPCWSTR lpcstrDesc,
                                           LPCWSTR lpcstrModule,
                                           LPVOID  lpContext);
#else
                static int CALLBACK DSEnumCallback(LPGUID lpGuid,
                        LPCSTR lpcstrDesc,
                        LPCSTR lpcstrModule,
                        LPVOID lpContext);
#endif
    public:
        AudioOutputDX       *m_parent       {nullptr};
        HINSTANCE            m_dsound_dll   {nullptr};
        LPDIRECTSOUND        m_dsobject     {nullptr};
        LPDIRECTSOUNDBUFFER  m_dsbuffer     {nullptr};
        bool                 m_playStarted  {false};
        DWORD                m_writeCursor  {0};
        GUID                 m_deviceGUID;
        GUID                *m_chosenGUID   {nullptr};
        int                  m_device_count {0};
        int                  m_device_num   {0};
        QString              m_device_name;
        QMap<int, QString>   m_device_list;
};


AudioOutputDX::AudioOutputDX(const AudioSettings &settings) :
    AudioOutputBase(settings),
    m_priv(new AudioOutputDXPrivate(this)),
    m_UseSPDIF(settings.m_usePassthru)
{
    timeBeginPeriod(1);
    InitSettings(settings);
    if (m_passthruDevice == "auto" || m_passthruDevice.toLower() == "default")
        m_passthruDevice = m_mainDevice;
    else
        m_discreteDigital = true;
    if (settings.m_init)
        Reconfigure(settings);
}

AudioOutputDX::~AudioOutputDX()
{
    KillAudio();
    if (m_priv)
    {
        delete m_priv;
        m_priv = nullptr;
    }
    timeEndPeriod(1);
}

using LPFNDSC = HRESULT (WINAPI *) (LPGUID, LPDIRECTSOUND *, LPUNKNOWN);
using LPFNDSE = HRESULT (WINAPI *) (LPDSENUMCALLBACK, LPVOID);

#ifdef UNICODE
int CALLBACK AudioOutputDXPrivate::DSEnumCallback(LPGUID  lpGuid,
                                                  LPCWSTR lpcstrDesc,
                                 [[maybe_unused]] LPCWSTR lpcstrModule,
                                                  LPVOID  lpContext)
{
        QString enum_desc = QString::fromWCharArray( lpcstrDesc );
#else
int CALLBACK AudioOutputDXPrivate::DSEnumCallback(LPGUID lpGuid,
                                                  LPCSTR lpcstrDesc,
                                 [[maybe_unused]] LPCSTR lpcstrModule,
                                                  LPVOID lpContext)
{
        QString enum_desc = QString::fromLocal8Bit( lpcstrDesc );

#endif
    AudioOutputDXPrivate *context = static_cast<AudioOutputDXPrivate*>(lpContext);
    const QString cfg_desc  = context->m_device_name;
    const int device_num    = context->m_device_num;
    const int device_count  = context->m_device_count;

    VBAUDIO(QString("Device %1:" + enum_desc).arg(device_count));

    if ((device_num == device_count ||
        (device_num == 0 && !cfg_desc.isEmpty() &&
        enum_desc.startsWith(cfg_desc, Qt::CaseInsensitive))) && lpGuid)
    {
        context->m_deviceGUID  = *lpGuid;
        context->m_chosenGUID  =
            &(context->m_deviceGUID);
        context->m_device_name = enum_desc;
        context->m_device_num  = device_count;
    }

    context->m_device_list.insert(device_count, enum_desc);
    context->m_device_count++;
    return 1;
}

void AudioOutputDXPrivate::ResetDirectSound(void)
{
    DestroyDSBuffer();

    if (m_dsobject)
    {
        IDirectSound_Release(m_dsobject);
        m_dsobject = nullptr;
    }

    if (m_dsound_dll)
    {
       FreeLibrary(m_dsound_dll);
       m_dsound_dll = nullptr;
    }

    m_chosenGUID   = nullptr;
    m_device_count = 0;
    m_device_num   = 0;
    m_device_list.clear();
}

int AudioOutputDXPrivate::InitDirectSound(bool passthrough)
{
    LPFNDSC OurDirectSoundCreate;
    LPFNDSE OurDirectSoundEnumerate;
    bool ok;

    ResetDirectSound();

    m_dsound_dll = LoadLibrary(TEXT("DSOUND.DLL"));
    if (m_dsound_dll == nullptr)
    {
        VBERROR("Cannot open DSOUND.DLL");
        goto error;
    }

    if (m_parent)  // parent can be nullptr only when called from GetDXDevices()
        m_device_name = passthrough ?
                      m_parent->m_passthruDevice : m_parent->m_mainDevice;
    m_device_name = m_device_name.section(':', 1);
    m_device_num  = m_device_name.toInt(&ok, 10);

    VBAUDIO(QString("Looking for device num:%1 or name:%2")
            .arg(m_device_num).arg(m_device_name));

    OurDirectSoundEnumerate =
        (LPFNDSE)GetProcAddress(m_dsound_dll, "DirectSoundEnumerateA");

    if(OurDirectSoundEnumerate)
        if(FAILED(OurDirectSoundEnumerate(DSEnumCallback, this) != DS_OK))
            VBERROR("DirectSoundEnumerate FAILED");

    if (!m_chosenGUID)
    {
        m_device_num = 0;
        m_device_name = "Primary Sound Driver";
    }

    VBAUDIO(QString("Using device %1:%2").arg(m_device_num).arg(m_device_name));

    OurDirectSoundCreate =
        (LPFNDSC)GetProcAddress(m_dsound_dll, "DirectSoundCreate");

    if (OurDirectSoundCreate == nullptr)
    {
        VBERROR("GetProcAddress FAILED");
        goto error;
    }

    if (FAILED(OurDirectSoundCreate(m_chosenGUID, &m_dsobject, nullptr)))
    {
        VBERROR("Cannot create a direct sound device");
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
    if (FAILED(IDirectSound_SetCooperativeLevel(m_dsobject, GetDesktopWindow(),
                                                DSSCL_EXCLUSIVE)))
        VBERROR("Cannot set DS cooperative level");

    VBAUDIO("Initialised DirectSound");

    return 0;

 error:
    m_dsobject = nullptr;
    if (m_dsound_dll)
    {
        FreeLibrary(m_dsound_dll);
        m_dsound_dll = nullptr;
    }
    return -1;
}

void AudioOutputDXPrivate::DestroyDSBuffer(void)
{
    if (!m_dsbuffer)
        return;

    VBAUDIO("Destroying DirectSound buffer");
    IDirectSoundBuffer_Stop(m_dsbuffer);
    m_writeCursor = 0;
    IDirectSoundBuffer_SetCurrentPosition(m_dsbuffer, m_writeCursor);
    m_playStarted = false;
    IDirectSoundBuffer_Release(m_dsbuffer);
    m_dsbuffer = nullptr;
}

void AudioOutputDXPrivate::FillBuffer(unsigned char *buffer, int size)
{
    void    *p_write_position, *p_wrap_around;
    DWORD   l_bytes1, l_bytes2, play_pos, write_pos;
    HRESULT dsresult;

    if (!m_dsbuffer)
        return;

    while (true)
    {

        dsresult = IDirectSoundBuffer_GetCurrentPosition(m_dsbuffer,
                                                         &play_pos, &write_pos);
        if (dsresult != DS_OK)
        {
            VBERROR("Cannot get current buffer position");
            return;
        }

        VBAUDIOTS(QString("play: %1 write: %2 wcursor: %3")
                  .arg(play_pos).arg(write_pos).arg(m_writeCursor));

        while ((m_writeCursor < write_pos &&
                   ((m_writeCursor >= play_pos && write_pos >= play_pos)   ||
                    (m_writeCursor < play_pos  && write_pos <  play_pos))) ||
               (m_writeCursor >= play_pos && write_pos < play_pos))
        {
            VBERROR("buffer underrun");
            m_writeCursor += size;
            while (m_writeCursor >= (DWORD)m_parent->m_soundcardBufferSize)
                m_writeCursor -= m_parent->m_soundcardBufferSize;
        }

        if ((m_writeCursor < play_pos  && m_writeCursor + size >= play_pos) ||
            (m_writeCursor >= play_pos &&
             m_writeCursor + size >= play_pos + m_parent->m_soundcardBufferSize))
        {
            usleep(50000);
            continue;
        }

        break;
    }

    dsresult = IDirectSoundBuffer_Lock(
                   m_dsbuffer,              /* DS buffer */
                   m_writeCursor,           /* Start offset */
                   size,                  /* Number of bytes */
                   &p_write_position,     /* Address of lock start */
                   &l_bytes1,             /* Bytes locked before wrap */
                   &p_wrap_around,        /* Buffer address (if wrap around) */
                   &l_bytes2,             /* Count of bytes after wrap around */
                   0);                    /* Flags */

    if (dsresult == DSERR_BUFFERLOST)
    {
        IDirectSoundBuffer_Restore(m_dsbuffer);
        dsresult = IDirectSoundBuffer_Lock(m_dsbuffer, m_writeCursor, size,
                                           &p_write_position, &l_bytes1,
                                           &p_wrap_around, &l_bytes2, 0);
    }

    if (dsresult != DS_OK)
    {
        VBERROR("Cannot lock buffer, audio dropped");
        return;
    }

    memcpy(p_write_position, buffer, l_bytes1);
    if (p_wrap_around)
        memcpy(p_wrap_around, buffer + l_bytes1, l_bytes2);

    m_writeCursor += l_bytes1 + l_bytes2;

    while (m_writeCursor >= (DWORD)m_parent->m_soundcardBufferSize)
        m_writeCursor -= m_parent->m_soundcardBufferSize;

    IDirectSoundBuffer_Unlock(m_dsbuffer, p_write_position, l_bytes1,
                              p_wrap_around, l_bytes2);
}

bool AudioOutputDXPrivate::StartPlayback(void)
{
    HRESULT dsresult;

    dsresult = IDirectSoundBuffer_Play(m_dsbuffer, 0, 0, DSBPLAY_LOOPING);
    if (dsresult == DSERR_BUFFERLOST)
    {
        IDirectSoundBuffer_Restore(m_dsbuffer);
        dsresult = IDirectSoundBuffer_Play(m_dsbuffer, 0, 0, DSBPLAY_LOOPING);
    }
    if (dsresult != DS_OK)
    {
        VBERROR("Cannot start playing buffer");
        m_playStarted = false;
        return false;
    }

    m_playStarted=true;
    return true;
}

AudioOutputSettings* AudioOutputDX::GetOutputSettings(bool passthrough)
{
    AudioOutputSettings *settings = new AudioOutputSettings();
    DSCAPS devcaps;
    devcaps.dwSize = sizeof(DSCAPS);

    m_priv->InitDirectSound(passthrough);
    if ((!m_priv->m_dsobject || !m_priv->m_dsound_dll) ||
        FAILED(IDirectSound_GetCaps(m_priv->m_dsobject, &devcaps)) )
    {
        delete settings;
        return nullptr;
    }

    VBAUDIO(QString("GetCaps sample rate min: %1 max: %2")
            .arg(devcaps.dwMinSecondarySampleRate)
            .arg(devcaps.dwMaxSecondarySampleRate));

    /* We shouldn't assume we can do everything between min and max but
       there's no way to test individual rates, so we'll have to */
    while (DWORD rate = (DWORD)settings->GetNextRate())
        if((rate >= devcaps.dwMinSecondarySampleRate) ||
           (rate <= devcaps.dwMaxSecondarySampleRate))
            settings->AddSupportedRate(rate);

    /* We can only test for 8 and 16 bit support, DS uses float internally */
    if (devcaps.dwFlags & DSCAPS_PRIMARY8BIT)
        settings->AddSupportedFormat(FORMAT_U8);
    if (devcaps.dwFlags & DSCAPS_PRIMARY16BIT)
        settings->AddSupportedFormat(FORMAT_S16);
#if 0 // 24-bit integer is not supported
    settings->AddSupportedFormat(FORMAT_S24);
#endif
#if 0 // 32-bit integer (OGG) is not supported on all platforms.
    settings->AddSupportedFormat(FORMAT_S32);
#endif
#if 0 // 32-bit floating point (AC3) is not supported on all platforms.
    settings->AddSupportedFormat(FORMAT_FLT);
#endif

    /* No way to test anything other than mono or stereo, guess that we can do
       up to 5.1 */
    for (uint i = 2; i < 7; i++)
        settings->AddSupportedChannels(i);

    settings->setPassthrough(0); // Maybe passthrough

    return settings;
}

bool AudioOutputDX::OpenDevice(void)
{
    WAVEFORMATEXTENSIBLE wf;
    DSBUFFERDESC         dsbdesc;

    CloseDevice();

    m_UseSPDIF = m_passthru || m_enc;
    m_priv->InitDirectSound(m_UseSPDIF);
    if (!m_priv->m_dsobject || !m_priv->m_dsound_dll)
    {
        Error(QObject::tr("DirectSound initialization failed"));
        return false;
    }

    // fragments are 50ms worth of samples
    m_fragmentSize = 50 * m_outputBytesPerFrame * m_sampleRate / 1000;
    // DirectSound buffer holds 4 fragments = 200ms worth of samples
    m_soundcardBufferSize = m_fragmentSize << 2;

    VBAUDIO(QString("DirectSound buffer size: %1").arg(m_soundcardBufferSize));

    wf.Format.nChannels            = m_channels;
    wf.Format.nSamplesPerSec       = m_sampleRate;
    wf.Format.nBlockAlign          = m_outputBytesPerFrame;
    wf.Format.nAvgBytesPerSec      = m_sampleRate * m_outputBytesPerFrame;
    wf.Format.wBitsPerSample       = (m_outputBytesPerFrame << 3) / m_channels;
    wf.Samples.wValidBitsPerSample =
        AudioOutputSettings::FormatToBits(m_outputFormat);

    if (m_UseSPDIF)
    {
        wf.Format.wFormatTag = WAVE_FORMAT_DOLBY_AC3_SPDIF;
        wf.SubFormat         = _KSDATAFORMAT_SUBTYPE_DOLBY_AC3_SPDIF;
    }
    else if (m_outputFormat == FORMAT_FLT)
    {
        wf.Format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
        wf.SubFormat         = _KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
    }
    else
    {
        wf.Format.wFormatTag = WAVE_FORMAT_PCM;
        wf.SubFormat         = _KSDATAFORMAT_SUBTYPE_PCM;
    }

    VBAUDIO(QString("New format: %1bits %2ch %3Hz %4")
            .arg(wf.Samples.wValidBitsPerSample).arg(m_channels)
            .arg(m_sampleRate).arg(m_UseSPDIF ? "data" : "PCM"));

    if (m_channels <= 2)
        wf.Format.cbSize = 0;
    else
    {
        wf.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        wf.dwChannelMask = 0x003F; // 0x003F = 5.1 channels
        wf.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    }

    memset(&dsbdesc, 0, sizeof(DSBUFFERDESC));
    dsbdesc.dwSize = sizeof(DSBUFFERDESC);
    dsbdesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2  // Better position accuracy
                    | DSBCAPS_GLOBALFOCUS          // Allows background playing
                    | DSBCAPS_LOCHARDWARE;         // Needed for 5.1 on emu101k

    if (!m_UseSPDIF)
        dsbdesc.dwFlags |= DSBCAPS_CTRLVOLUME;     // Allow volume control

    dsbdesc.dwBufferBytes = m_soundcardBufferSize; // buffer size
    dsbdesc.lpwfxFormat = (WAVEFORMATEX *)&wf;

    if (FAILED(IDirectSound_CreateSoundBuffer(m_priv->m_dsobject, &dsbdesc,
                                             &m_priv->m_dsbuffer, nullptr)))
    {
        /* Vista does not support hardware mixing
           try without DSBCAPS_LOCHARDWARE */
        dsbdesc.dwFlags &= ~DSBCAPS_LOCHARDWARE;
        HRESULT dsresult =
            IDirectSound_CreateSoundBuffer(m_priv->m_dsobject, &dsbdesc,
                                           &m_priv->m_dsbuffer, nullptr);
        if (FAILED(dsresult))
        {
            if (dsresult == DSERR_UNSUPPORTED)
                Error(QObject::tr("Unsupported format for device %1:%2")
                      .arg(m_priv->m_device_num).arg(m_priv->m_device_name));
            else
                Error(QObject::tr("Failed to create DS buffer 0x%1")
                      .arg((DWORD)dsresult, 0, 16));
            return false;
        }
        VBAUDIO("Using software mixer");
    }
    VBAUDIO("Created DirectSound buffer");

    return true;
}

void AudioOutputDX::CloseDevice(void)
{
    if (m_priv->m_dsbuffer)
        m_priv->DestroyDSBuffer();
}

void AudioOutputDX::WriteAudio(unsigned char * buffer, int size)
{
    if (size == 0)
        return;

    m_priv->FillBuffer(buffer, size);
    if (!m_priv->m_playStarted)
        m_priv->StartPlayback();
}

int AudioOutputDX::GetBufferedOnSoundcard(void) const
{
    if (!m_priv->m_playStarted)
        return 0;

    HRESULT dsresult;
    DWORD   play_pos, write_pos;
    int     buffered;

    dsresult = IDirectSoundBuffer_GetCurrentPosition(m_priv->m_dsbuffer,
                                                     &play_pos, &write_pos);
    if (dsresult != DS_OK)
    {
        VBERROR("Cannot get current buffer position");
        return 0;
    }

    buffered = (int)m_priv->m_writeCursor - (int)play_pos;

    if (buffered <= 0)
        buffered += m_soundcardBufferSize;

    return buffered;
}

int AudioOutputDX::GetVolumeChannel([[maybe_unused]] int channel) const
{
    HRESULT dsresult;
    long dxVolume = 0;
    int volume;

    if (m_UseSPDIF)
        return 100;

    dsresult = IDirectSoundBuffer_GetVolume(m_priv->m_dsbuffer, &dxVolume);
    volume = (int)(pow(10,(float)dxVolume/20)*100);

    if (dsresult != DS_OK)
    {
        VBERROR(QString("Failed to get volume %1").arg(dxVolume));
        return volume;
    }

    VBAUDIO(QString("Got volume %1").arg(volume));
    return volume;
}

void AudioOutputDX::SetVolumeChannel([[maybe_unused]] int channel, int volume)
{
    HRESULT dsresult;
    long dxVolume { DSBVOLUME_MIN };
    if (volume > 0)
    {
        float dbAtten = 20 * log10((float)volume/100.F);
        dxVolume = (long)(100.0F * dbAtten);
    }

    if (m_UseSPDIF)
        return;

    // dxVolume is attenuation in 100ths of a decibel
    dsresult = IDirectSoundBuffer_SetVolume(m_priv->m_dsbuffer, dxVolume);

    if (dsresult != DS_OK)
    {
        VBERROR(QString("Failed to set volume %1").arg(dxVolume));
        return;
    }

    VBAUDIO(QString("Set volume %1").arg(dxVolume));
}

QMap<int, QString> *AudioOutputDX::GetDXDevices(void)
{
    AudioOutputDXPrivate *tmp_priv = new AudioOutputDXPrivate(nullptr);
    tmp_priv->InitDirectSound(false);
    QMap<int, QString> *dxdevs = new QMap<int, QString>(tmp_priv->m_device_list);
    delete tmp_priv;
    return dxdevs;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
