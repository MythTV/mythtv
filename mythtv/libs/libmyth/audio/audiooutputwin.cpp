#include <iostream>

#include "libmythbase/mythlogging.h"
#include "audiooutputwin.h"

#include <windows.h>
#include <mmsystem.h>
#include <objbase.h> // For DEFINE_GUID

#define LOC QString("AOWin: ")

#ifndef WAVE_FORMAT_IEEE_FLOAT
#define WAVE_FORMAT_IEEE_FLOAT 0x0003
#endif

#ifndef WAVE_FORMAT_DOLBY_AC3_SPDIF
#define WAVE_FORMAT_DOLBY_AC3_SPDIF 0x0092
#endif

#ifndef WAVE_FORMAT_EXTENSIBLE
#define WAVE_FORMAT_EXTENSIBLE 0xFFFE
#endif

#ifndef _WAVEFORMATEXTENSIBLE_
struct WAVEFORMATEXTENSIBLE {
    WAVEFORMATEX    Format;
    union {
        WORD wValidBitsPerSample;       // bits of precision
        WORD wSamplesPerBlock;          // valid if wBitsPerSample==0
        WORD wReserved;                 // If neither applies, set to zero
    } Samples;
    DWORD           dwChannelMask;      // which channels are present in stream
    GUID            SubFormat;
};
using PWAVEFORMATEXTENSIBLE = WAVEFORMATEXTENSIBLE*;
#endif

const uint AudioOutputWin::kPacketCnt = 4;

DEFINE_GUID(_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, WAVE_FORMAT_IEEE_FLOAT,
            0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(_KSDATAFORMAT_SUBTYPE_PCM, WAVE_FORMAT_PCM,
            0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(_KSDATAFORMAT_SUBTYPE_DOLBY_AC3_SPDIF, WAVE_FORMAT_DOLBY_AC3_SPDIF,
            0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);

class AudioOutputWinPrivate
{
  public:
    AudioOutputWinPrivate()
    {
        m_WaveHdrs = new WAVEHDR[AudioOutputWin::kPacketCnt];
        memset(m_WaveHdrs, 0, sizeof(WAVEHDR) * AudioOutputWin::kPacketCnt);
        m_hEvent = CreateEvent(nullptr, FALSE, TRUE, nullptr);
    }

    ~AudioOutputWinPrivate()
    {
        if (m_WaveHdrs)
        {
            delete[] m_WaveHdrs;
            m_WaveHdrs = nullptr;
        }
        if (m_hEvent)
        {
            CloseHandle(m_hEvent);
            m_hEvent = nullptr;
        }
    }

    void Close(void)
    {
        if (m_hWaveOut)
        {
            waveOutReset(m_hWaveOut);
            waveOutClose(m_hWaveOut);
            m_hWaveOut = nullptr;
        }
    }

    static void CALLBACK waveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD dwInstance,
                                     DWORD dwParam1, DWORD dwParam2);

  private:
    AudioOutputWinPrivate(const AudioOutputWinPrivate &) = delete;            // not copyable
    AudioOutputWinPrivate &operator=(const AudioOutputWinPrivate &) = delete; // not copyable

  public:
    HWAVEOUT  m_hWaveOut {nullptr};
    WAVEHDR  *m_WaveHdrs {nullptr};
    HANDLE    m_hEvent {nullptr};
};

void CALLBACK AudioOutputWinPrivate::waveOutProc([[maybe_unused]] HWAVEOUT hwo,
                                                 UINT uMsg,
                                                 DWORD dwInstance,
                                                 [[maybe_unused]] DWORD dwParam1,
                                                 [[maybe_unused]] DWORD dwParam2)
{
    if (uMsg != WOM_DONE)
        return;

    AudioOutputWin *instance = reinterpret_cast<AudioOutputWin*>(dwInstance);
    InterlockedDecrement(&instance->m_nPkts);
    if (instance->m_nPkts < (int)AudioOutputWin::kPacketCnt)
    {
        SetEvent(instance->m_priv->m_hEvent);
    }
}

AudioOutputWin::AudioOutputWin(const AudioSettings &settings) :
    AudioOutputBase(settings),
    m_priv(new AudioOutputWinPrivate()),
    m_UseSPDIF(settings.m_usePassthru)
{
    InitSettings(settings);
    if (settings.m_init)
        Reconfigure(settings);
    m_OutPkts = (unsigned char**) calloc(kPacketCnt, sizeof(unsigned char*));
}

AudioOutputWin::~AudioOutputWin()
{
    KillAudio();

    if (m_priv)
    {
        delete m_priv;
        m_priv = nullptr;
    }

    if (m_OutPkts)
    {
        for (uint i = 0; i < kPacketCnt; i++)
            if (m_OutPkts[i])
                free(m_OutPkts[i]);

        free(m_OutPkts);
        m_OutPkts = nullptr;
    }
}

AudioOutputSettings* AudioOutputWin::GetOutputSettings(bool /*digital*/)
{
    AudioOutputSettings *settings = new AudioOutputSettings();

    // We use WAVE_MAPPER to find a compatible device, so just claim support
    // for all of the standard rates
    while (DWORD rate = (DWORD)settings->GetNextRate())
        settings->AddSupportedRate(rate);

    // Support all standard formats
    settings->AddSupportedFormat(FORMAT_U8);
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

    // Guess that we can do up to 5.1
    for (uint i = 2; i < 7; i++)
        settings->AddSupportedChannels(i);

    settings->setPassthrough(0); //Maybe passthrough

    return settings;
}

bool AudioOutputWin::OpenDevice(void)
{
    CloseDevice();
    // fragments are 50ms worth of samples
    m_fragmentSize = 50 * m_outputBytesPerFrame * m_sampleRate / 1000;
    // DirectSound buffer holds 4 fragments = 200ms worth of samples
    m_soundcardBufferSize = kPacketCnt * m_fragmentSize;

    VBAUDIO(QString("Buffering %1 fragments of %2 bytes each, total: %3 bytes")
            .arg(kPacketCnt).arg(m_fragmentSize).arg(m_soundcardBufferSize));

    m_UseSPDIF = m_passthru || m_enc;

    WAVEFORMATEXTENSIBLE wf;
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

    /* Only use the new WAVE_FORMAT_EXTENSIBLE format for multichannel audio */
    if (m_channels <= 2)
        wf.Format.cbSize = 0;
    else
    {
        wf.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        wf.dwChannelMask = 0x003F; // 0x003F = 5.1 channels
        wf.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    }

    MMRESULT mmr = waveOutOpen(&m_priv->m_hWaveOut, WAVE_MAPPER,
                               (WAVEFORMATEX *)&wf,
                               (intptr_t)AudioOutputWinPrivate::waveOutProc,
                               (intptr_t)this, CALLBACK_FUNCTION);

    if (mmr == WAVERR_BADFORMAT)
    {
        Error(QString("Unable to set audio output parameters %1")
              .arg(wf.Format.nSamplesPerSec));
        return false;
    }

    return true;
}

void AudioOutputWin::CloseDevice(void)
{
    m_priv->Close();
}

void AudioOutputWin::WriteAudio(unsigned char * buffer, int size)
{
    if (!size)
        return;

    if (InterlockedIncrement(&m_nPkts) > (int)kPacketCnt)
    {
        while (m_nPkts > (int)kPacketCnt)
            WaitForSingleObject(m_priv->m_hEvent, INFINITE);
    }

    if (m_CurrentPkt >= kPacketCnt)
        m_CurrentPkt = 0;

    WAVEHDR *wh = &m_priv->m_WaveHdrs[m_CurrentPkt];
    if (wh->dwFlags & WHDR_PREPARED)
        waveOutUnprepareHeader(m_priv->m_hWaveOut, wh, sizeof(WAVEHDR));

    m_OutPkts[m_CurrentPkt] =
        (unsigned char*)realloc(m_OutPkts[m_CurrentPkt], size);

    memcpy(m_OutPkts[m_CurrentPkt], buffer, size);

    memset(wh, 0, sizeof(WAVEHDR));
    wh->lpData = (LPSTR)m_OutPkts[m_CurrentPkt];
    wh->dwBufferLength = size;

    if (MMSYSERR_NOERROR != waveOutPrepareHeader(m_priv->m_hWaveOut, wh,
                                                 sizeof(WAVEHDR)))
        VBERROR("WriteAudio: failed to prepare header");
    else if (MMSYSERR_NOERROR != waveOutWrite(m_priv->m_hWaveOut, wh,
                                              sizeof(WAVEHDR)))
        VBERROR("WriteAudio: failed to write packet");

    m_CurrentPkt++;
}

int AudioOutputWin::GetBufferedOnSoundcard(void) const
{
    return m_nPkts * m_fragmentSize;
}

int AudioOutputWin::GetVolumeChannel(int channel) const
{
    DWORD dwVolume = 0xffffffff;
    int Volume = 100;
    if (MMSYSERR_NOERROR == waveOutGetVolume((HWAVEOUT)WAVE_MAPPER, &dwVolume))
    {
        Volume = (channel == 0) ?
            (LOWORD(dwVolume) / (0xffff / 100)) :
            (HIWORD(dwVolume) / (0xffff / 100));
    }

    LOG(VB_AUDIO, LOG_INFO, QString("GetVolume(%1) %2 (%3)")
                                .arg(channel).arg(Volume).arg(dwVolume));

    return Volume;
}

void AudioOutputWin::SetVolumeChannel(int channel, int volume)
{
    if (channel > 1)
        VBERROR("Windows volume only supports stereo!");

    DWORD dwVolume = 0xffffffff;
    if (MMSYSERR_NOERROR == waveOutGetVolume((HWAVEOUT)WAVE_MAPPER, &dwVolume))
    {
        if (channel == 0)
            dwVolume = (dwVolume & 0xffff0000) | (volume * (0xffff / 100));
        else
            dwVolume = (dwVolume & 0xffff) | ((volume * (0xffff / 100)) << 16);
    }
    else
    {
        dwVolume = volume * (0xffff / 100);
        dwVolume |= (dwVolume << 16);
    }

    VBAUDIO(QString("SetVolume(%1) %2(%3)")
            .arg(channel).arg(volume).arg(dwVolume));

    waveOutSetVolume((HWAVEOUT)WAVE_MAPPER, dwVolume);
}
