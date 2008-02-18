#include <iostream>
//#include <stdlib.h>

using namespace std;

#include "mythcontext.h"
#include "audiooutputwin.h"

#include <windows.h>
#include <mmsystem.h>

// even number, 42 ~= 1.008 sec @ 48000 with 1152 samples per packet
const uint AudioOutputWin::kPacketCnt = 16;

class AudioOutputWinPrivate
{
  public:
    AudioOutputWinPrivate() :
        m_WaveHdrs(NULL), m_hEvent(NULL)
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

AudioOutputWin::AudioOutputWin(
    QString laudio_main_device,  QString           laudio_passthru_device,
    int     laudio_bits,         int               laudio_channels,
    int     laudio_samplerate,   AudioOutputSource lsource,
    bool    lset_initial_vol,    bool              laudio_passthru) :
    AudioOutputBase(laudio_main_device, laudio_passthru_device,
                    laudio_bits,        laudio_channels,
                    laudio_samplerate,  lsource,
                    lset_initial_vol,   laudio_passthru),
    m_priv(new AudioOutputWinPrivate()),
    m_nPkts(0),
    m_CurrentPkt(0),
    m_OutPkts(NULL)
{
    Reconfigure(laudio_bits,       laudio_channels,
                laudio_samplerate, laudio_passthru);

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

bool AudioOutputWin::OpenDevice(void)
{
    CloseDevice();
    // Set everything up
    SetBlocking(true);
    fragment_size = (AUDIOOUTPUT_TELEPHONY == source) ? 320 : 6144;

    WAVEFORMATEX wf;
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nChannels = audio_channels;
    wf.nSamplesPerSec = audio_samplerate;
    wf.wBitsPerSample = audio_bits;
    wf.nBlockAlign = wf.wBitsPerSample / 8 * wf.nChannels;
    wf.nAvgBytesPerSec = wf.nBlockAlign * wf.nSamplesPerSec;
    wf.cbSize = 0;

    MMRESULT mmr = waveOutOpen(
        &m_priv->m_hWaveOut,
        WAVE_MAPPER,
        &wf,
        (DWORD) AudioOutputWinPrivate::waveOutProc,
        (DWORD) this,
        CALLBACK_FUNCTION);

    if (mmr == WAVERR_BADFORMAT)
    {
        Error(QString("Unable to set audio output parameters %1")
              .arg(wf.nSamplesPerSec));
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

    if (InterlockedIncrement(&m_nPkts) >= kPacketCnt)
    {
        while (m_nPkts >= kPacketCnt)
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

int AudioOutputWin::getSpaceOnSoundcard(void)
{
    return (kPacketCnt - m_nPkts) * 1536 * 4;
}

int AudioOutputWin::getBufferedOnSoundcard(void)
{
    return m_nPkts * 1536 * 4;
}

int AudioOutputWin::GetVolumeChannel(int channel)
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

    VERBOSE(VB_AUDIO, "SetVolume(" << channel << ") "
            << volume << "(" << dwVolume << ")");

    waveOutSetVolume((HWAVEOUT)WAVE_MAPPER, dwVolume);
}
