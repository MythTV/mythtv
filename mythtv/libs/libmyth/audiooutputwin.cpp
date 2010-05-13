#include <iostream>

using namespace std;

#include "mythverbose.h"
#include "audiooutputwin.h"

#include <windows.h>
#include <mmsystem.h>

#ifndef WAVE_FORMAT_IEEE_FLOAT
#   define WAVE_FORMAT_IEEE_FLOAT 0x0003
#endif

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

DEFINE_GUID( _KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, WAVE_FORMAT_IEEE_FLOAT, 0x0000,
            0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 );
DEFINE_GUID( _KSDATAFORMAT_SUBTYPE_PCM, WAVE_FORMAT_PCM, 0x0000, 0x0010, 0x80,
            0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 );
DEFINE_GUID( _KSDATAFORMAT_SUBTYPE_DOLBY_AC3_SPDIF, WAVE_FORMAT_DOLBY_AC3_SPDIF,
            0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 );

// even number, 42 ~= 1.008 sec @ 48000 with 1152 samples per packet
const uint AudioOutputWin::kPacketCnt = 16;

class AudioOutputWinPrivate
{
  public:
    AudioOutputWinPrivate() :
        m_hWaveOut(NULL), m_WaveHdrs(NULL), m_hEvent(NULL)
    {
        m_WaveHdrs = new WAVEHDR[AudioOutputWin::kPacketCnt];
        memset(m_WaveHdrs, 0, sizeof(WAVEHDR) * AudioOutputWin::kPacketCnt);
        m_hEvent = CreateEvent(NULL, FALSE, TRUE, NULL);
    }

    ~AudioOutputWinPrivate()
    {
        if (m_WaveHdrs)
        {
            delete[] m_WaveHdrs;
            m_WaveHdrs = NULL;
        }
        if (m_hEvent)
        {
            CloseHandle(m_hEvent);
            m_hEvent = NULL;
        }
    }

    void Close(void)
    {
        if (m_hWaveOut)
        {
            waveOutReset(m_hWaveOut);
            waveOutClose(m_hWaveOut);
            m_hWaveOut = NULL;
        }
    }

    static void CALLBACK waveOutProc(
        HWAVEOUT hwo, UINT uMsg, DWORD dwInstance, DWORD dwParam1,
        DWORD dwParam2);

  public:
    HWAVEOUT  m_hWaveOut;
    WAVEHDR  *m_WaveHdrs;
    HANDLE    m_hEvent;
};

void CALLBACK AudioOutputWinPrivate::waveOutProc(
    HWAVEOUT hwo, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
{
    if (uMsg == WOM_DONE)
    {
        InterlockedDecrement(&((AudioOutputWin*)dwInstance)->m_nPkts);
        if (((AudioOutputWin*)dwInstance)->m_nPkts <
            (int)AudioOutputWin::kPacketCnt)
        {
            SetEvent(((AudioOutputWin*)dwInstance)->m_priv->m_hEvent);
        }
    }
}

AudioOutputWin::AudioOutputWin(const AudioSettings &settings) :
    AudioOutputBase(settings),
    m_priv(new AudioOutputWinPrivate()),
    m_nPkts(0),
    m_CurrentPkt(0),
    m_OutPkts(NULL),
    m_UseSPDIF(settings.use_passthru)
{
    Reconfigure(settings);

    m_OutPkts = (unsigned char**) calloc(kPacketCnt, sizeof(unsigned char*));
}

AudioOutputWin::~AudioOutputWin()
{
    KillAudio();

    if (m_priv)
    {
        delete m_priv;
        m_priv = NULL;
    }

    if (m_OutPkts)
    {
        for (uint i = 0; i < kPacketCnt; i++)
        {
            if (m_OutPkts[i])
                free(m_OutPkts[i]);
        }
        free(m_OutPkts);
        m_OutPkts = NULL;
    }
}

vector<int> AudioOutputWin::GetSupportedRates(void)
{
    // We use WAVE_MAPPER to find a compatible device,
    // so just return a set of standard rates
    const int srates[] = { 11025, 22050, 44100, 48000 };
    vector<int> rates(srates, srates + sizeof(srates) / sizeof(int) );
    if (audio_passthru || audio_enc)
    {
        rates.clear();
        rates.push_back(48000);
    }
    return rates;
}

bool AudioOutputWin::OpenDevice(void)
{
    CloseDevice();
    // Set everything up
    SetBlocking(true);
    fragment_size = (AUDIOOUTPUT_TELEPHONY == source) ? 320 : 6144;
    soundcard_buffer_size = kPacketCnt * fragment_size;
    m_UseSPDIF = audio_passthru || audio_enc;

    WAVEFORMATEXTENSIBLE wf;
    wf.Format.wFormatTag =
        (m_UseSPDIF) ? WAVE_FORMAT_DOLBY_AC3_SPDIF : WAVE_FORMAT_PCM;
    wf.Format.nChannels = audio_channels;
    wf.Format.nSamplesPerSec = audio_samplerate;
    wf.Format.nBlockAlign = audio_bytes_per_sample;
    wf.Format.nAvgBytesPerSec = 
        wf.Format.nSamplesPerSec * wf.Format.nBlockAlign;
    wf.Format.wBitsPerSample = audio_bits;
    wf.Samples.wValidBitsPerSample = wf.Format.wBitsPerSample;
    wf.SubFormat = 
        (m_UseSPDIF) ? _KSDATAFORMAT_SUBTYPE_DOLBY_AC3_SPDIF : _KSDATAFORMAT_SUBTYPE_PCM;

    VERBOSE(VB_AUDIO, QString("New format: %1bits, %2ch, %3Hz")
                .arg(audio_bits).arg(audio_channels).arg(audio_samplerate));

    /* Only use the new WAVE_FORMAT_EXTENSIBLE format for multichannel audio */
    if (audio_channels <= 2)
    {
        wf.Format.cbSize = 0;
    }
    else
    {
        wf.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        wf.dwChannelMask = 0x003F; // 0x003F = 5.1 channels
        wf.Format.cbSize =
            sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    }

    MMRESULT mmr = waveOutOpen(
        &m_priv->m_hWaveOut,
        WAVE_MAPPER,
        (WAVEFORMATEX *)&wf,
        (DWORD) AudioOutputWinPrivate::waveOutProc,
        (DWORD) this,
        CALLBACK_FUNCTION);

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
    if (size == 0)
        return;

    if (InterlockedIncrement(&m_nPkts) > kPacketCnt)
    {
        while (m_nPkts > kPacketCnt)
            WaitForSingleObject(m_priv->m_hEvent, INFINITE);
    }

    if (m_CurrentPkt >= kPacketCnt)
        m_CurrentPkt = 0;

    WAVEHDR *wh = &m_priv->m_WaveHdrs[m_CurrentPkt];
    if (wh->dwFlags & WHDR_PREPARED)
        waveOutUnprepareHeader(m_priv->m_hWaveOut, wh, sizeof(WAVEHDR));

    m_OutPkts[m_CurrentPkt] = (unsigned char*)
        realloc(m_OutPkts[m_CurrentPkt], size);

    memcpy(m_OutPkts[m_CurrentPkt], buffer, size);

    memset(wh, 0, sizeof(WAVEHDR));
    wh->lpData = (LPSTR)m_OutPkts[m_CurrentPkt];
    wh->dwBufferLength = size;

    if (MMSYSERR_NOERROR != waveOutPrepareHeader(
            m_priv->m_hWaveOut, wh, sizeof(WAVEHDR)))
    {
        VERBOSE(VB_IMPORTANT, "WriteAudio: failed to prepare header");
    }
    else if (MMSYSERR_NOERROR != waveOutWrite(
                 m_priv->m_hWaveOut, wh, sizeof(WAVEHDR)))
    {
        VERBOSE(VB_IMPORTANT, "WriteAudio: failed to write packet");
    }

    m_CurrentPkt++;
}

int AudioOutputWin::GetSpaceOnSoundcard(void) const
{
    return soundcard_buffer_size - GetBufferedOnSoundcard();
}

int AudioOutputWin::GetBufferedOnSoundcard(void) const
{
    return m_nPkts * fragment_size;
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

    VERBOSE(VB_AUDIO, "GetVolume(" << channel << ") "
            << Volume << "(" << dwVolume << ")");

    return Volume;
}

void AudioOutputWin::SetVolumeChannel(int channel, int volume)
{
    if (channel > 1)
    {
        Error(QString("Error setting channel: %1. "
                      "Only stereo volume supported").arg(channel));
        return;
    }

    DWORD dwVolume = 0xffffffff;
    if (MMSYSERR_NOERROR == waveOutGetVolume((HWAVEOUT)WAVE_MAPPER, &dwVolume))
    {
        if (channel == 0)
            dwVolume = dwVolume & 0xffff0000 | volume * (0xffff / 100);
        else
            dwVolume = dwVolume & 0xffff | ((volume * (0xffff / 100)) << 16);
    }
    else
    {
        dwVolume = volume * (0xffff / 100);
        dwVolume |= (dwVolume << 16);
    }

    VERBOSE(VB_AUDIO, QString("SetVolume(%1) %2(%3)")
            .arg(channel).arg(volume).arg(dwVolume));
    waveOutSetVolume((HWAVEOUT)WAVE_MAPPER, dwVolume);
}
