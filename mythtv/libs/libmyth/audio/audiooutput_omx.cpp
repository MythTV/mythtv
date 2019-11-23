#include "audiooutput_omx.h"

#include <algorithm> // max/min
#include <cassert>
#include <cstddef>
#include <vector>

#include <OMX_Core.h>
#include <OMX_Audio.h>
#ifdef USING_BROADCOM
#include <OMX_Broadcom.h>
#include <bcm_host.h>
#endif

// MythTV
#include "mythcorecontext.h"

#include "omxcontext.h"
using namespace omxcontext;


/*
 * Macros
 */
#define LOC QString("AOMX:%1 ").arg(m_audiorender.Id())

// Stringize a macro
#define _STR(s) #s
#define STR(s) _STR(s)
#define CASE2STR(f) case f: return STR(f)

// Component names
#ifdef USING_BELLAGIO
# define AUDIO_RENDER "alsasink"
# define AUDIO_DECODE "audio_decoder.mp3.mad" // or audio_decoder.ogg.single
#else
# define AUDIO_RENDER "audio_render"
# define AUDIO_DECODE "audio_decode"
#endif


/*
 * Types
 */


/*
 * Functions
 */
AudioOutputOMX::AudioOutputOMX(const AudioSettings &settings) :
    AudioOutputBase(settings),
    m_audiorender(gCoreContext->GetSetting("OMXAudioRender", AUDIO_RENDER), *this)
{
    if (m_audiorender.GetState() != OMX_StateLoaded)
        return;

    if (getenv("NO_OPENMAX_AUDIO"))
    {
        LOG(VB_AUDIO, LOG_NOTICE, LOC + "OpenMAX audio disabled");
        return;
    }

#ifndef USING_BELLAGIO // Bellagio 0.9.3 times out in PortDisable
    // Disable the renderer's clock port, otherwise can't goto OMX_StateIdle
    if (OMX_ErrorNone == m_audiorender.Init(OMX_IndexParamOtherInit))
        m_audiorender.PortDisable(0, 500);
#endif

    if (OMX_ErrorNone != m_audiorender.Init(OMX_IndexParamAudioInit))
        return;

    if (!m_audiorender.IsValid())
        return;

    // Show default port definitions and audio formats supported
    for (unsigned port = 0; port < m_audiorender.Ports(); ++port)
    {
        m_audiorender.ShowPortDef(port, LOG_DEBUG, VB_AUDIO);
#if 0
        m_audiorender.ShowFormats(port, LOG_DEBUG, VB_AUDIO);
#endif
    }

    InitSettings(settings);
    if (settings.m_init)
        Reconfigure(settings);
}

// virtual
AudioOutputOMX::~AudioOutputOMX()
{
    KillAudio();

    // Must shutdown the OMX components now before our state becomes invalid.
    // When the component's dtor is called our state has already been destroyed.
    m_audiorender.Shutdown();
}

template<typename T>
static inline void SetupChannels(T &t)
{
    for (int i = OMX_AUDIO_MAXCHANNELS - 1; i >= int(t.nChannels); --i)
        t.eChannelMapping[i] = OMX_AUDIO_ChannelNone;
    switch (t.nChannels)
    {
      default:
      case 8:
        t.eChannelMapping[7] = OMX_AUDIO_ChannelRS;
        [[clang::fallthrough]];
      case 7:
        t.eChannelMapping[6] = OMX_AUDIO_ChannelLS;
        [[clang::fallthrough]];
      case 6:
        t.eChannelMapping[5] = OMX_AUDIO_ChannelRR;
        [[clang::fallthrough]];
      case 5:
        t.eChannelMapping[4] = OMX_AUDIO_ChannelLR;
        [[clang::fallthrough]];
      case 4:
        t.eChannelMapping[3] = OMX_AUDIO_ChannelLFE;
        [[clang::fallthrough]];
      case 3:
        t.eChannelMapping[2] = OMX_AUDIO_ChannelCF;
        [[clang::fallthrough]];
      case 2:
        t.eChannelMapping[1] = OMX_AUDIO_ChannelRF;
        t.eChannelMapping[0] = OMX_AUDIO_ChannelLF;
        break;
      case 1:
        t.eChannelMapping[0] = OMX_AUDIO_ChannelCF;
        break;
      case 0:
        break;
     }
}

static bool Format2Pcm(OMX_AUDIO_PARAM_PCMMODETYPE &pcm, AudioFormat afmt)
{
    switch (afmt)
    {
      case FORMAT_U8:
        pcm.eNumData = OMX_NumericalDataUnsigned;
        pcm.eEndian = OMX_EndianLittle;
        pcm.nBitPerSample = 8;
        break;
      case FORMAT_S16:
        pcm.eNumData = OMX_NumericalDataSigned;
        pcm.eEndian = OMX_EndianLittle;
        pcm.nBitPerSample = 16;
        break;
      case FORMAT_S24LSB:
        pcm.eNumData = OMX_NumericalDataSigned;
        pcm.eEndian = OMX_EndianLittle;
        pcm.nBitPerSample = 24;
        break;
      case FORMAT_S24:
        pcm.eNumData = OMX_NumericalDataSigned;
        pcm.eEndian = OMX_EndianBig;
        pcm.nBitPerSample = 24;
        break;
      case FORMAT_S32:
        pcm.eNumData = OMX_NumericalDataSigned;
        pcm.eEndian = OMX_EndianLittle;
        pcm.nBitPerSample = 32;
        break;
      case FORMAT_FLT:
      default:
        return false;
    }
    pcm.bInterleaved = OMX_TRUE;
    pcm.ePCMMode = OMX_AUDIO_PCMModeLinear;
    return true;
}

#ifdef OMX_AUDIO_CodingDDP_Supported
static const char *toString(OMX_AUDIO_DDPBITSTREAMID id)
{
    switch (id)
    {
        CASE2STR(OMX_AUDIO_DDPBitStreamIdAC3);
        CASE2STR(OMX_AUDIO_DDPBitStreamIdEAC3);
        default: break;
    }
    static char buf[32];
    return strcpy(buf, qPrintable(QString("DDPBitStreamId 0x%1").arg(id,0,16)));
}
#endif

// virtual
bool AudioOutputOMX::OpenDevice(void)
{
    LOG(VB_AUDIO, LOG_INFO, LOC + __func__ + " begin");

    if (!m_audiorender.IsValid())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + __func__ + " No audio render");
        return false;
    }

    m_audiorender.Shutdown();

    OMX_ERRORTYPE e = OMX_ErrorNone;
    unsigned nBitPerSample = 0;

    OMX_AUDIO_PARAM_PORTFORMATTYPE fmt;
    OMX_DATA_INIT(fmt);
    fmt.nPortIndex = m_audiorender.Base();

    if (m_passthru || m_enc)
    {
        switch (m_codec)
        {
#ifdef OMX_AUDIO_CodingDDP_Supported
          case AV_CODEC_ID_AC3:
          case AV_CODEC_ID_EAC3:
            nBitPerSample = 16;
            fmt.eEncoding = OMX_AUDIO_CodingDDP;
            e = m_audiorender.SetParameter(OMX_IndexParamAudioPortFormat, &fmt);
            if (e != OMX_ErrorNone)
            {
                LOG(VB_AUDIO, LOG_ERR, LOC + QString(
                        "SetParameter AudioPortFormat DDP error %1")
                    .arg(Error2String(e)));
                return false;
            }

            OMX_AUDIO_PARAM_DDPTYPE ddp;
            OMX_DATA_INIT(ddp);
            ddp.nPortIndex = m_audiorender.Base();
            e = m_audiorender.GetParameter(OMX_IndexParamAudioDdp, &ddp);
            if (e != OMX_ErrorNone)
            {
                LOG(VB_AUDIO, LOG_ERR, LOC + QString(
                        "GetParameter AudioDdp error %1")
                    .arg(Error2String(e)));
                return false;
            }

            ddp.nChannels = m_channels;
            ddp.nBitRate = 0;
            ddp.nSampleRate = m_samplerate;
            ddp.eBitStreamId = (AV_CODEC_ID_AC3 == m_codec) ?
                               OMX_AUDIO_DDPBitStreamIdAC3 :
                               OMX_AUDIO_DDPBitStreamIdEAC3;
            ddp.eBitStreamMode = OMX_AUDIO_DDPBitStreamModeCM;
            ddp.eDolbySurroundMode = OMX_AUDIO_DDPDolbySurroundModeNotIndicated;
            ::SetupChannels(ddp);

            LOG(VB_AUDIO, LOG_INFO, LOC + __func__ + QString(
                    " DDP %1 chnls @ %2 sps %3 bps %4")
                .arg(ddp.nChannels).arg(ddp.nSampleRate).arg(ddp.nBitRate)
                .arg(toString(ddp.eBitStreamId)) );

            e = m_audiorender.SetParameter(OMX_IndexParamAudioDdp, &ddp);
            if (e != OMX_ErrorNone)
            {
                LOG(VB_AUDIO, LOG_ERR, LOC + QString(
                        "SetParameter AudioDdp error %1")
                    .arg(Error2String(e)));
                return false;
            }
            break;
#endif //def OMX_AUDIO_CodingDDP_Supported

#ifdef OMX_AUDIO_CodingDTS_Supported
          case AV_CODEC_ID_DTS:
            nBitPerSample = 16;
            fmt.eEncoding = OMX_AUDIO_CodingDTS;
            e = m_audiorender.SetParameter(OMX_IndexParamAudioPortFormat, &fmt);
            if (e != OMX_ErrorNone)
            {
                LOG(VB_AUDIO, LOG_ERR, LOC + QString(
                        "SetParameter AudioPortFormat DTS error %1")
                    .arg(Error2String(e)));
                return false;
            }

            OMX_AUDIO_PARAM_DTSTYPE dts;
            OMX_DATA_INIT(dts);
            dts.nPortIndex = m_audiorender.Base();
            e = m_audiorender.GetParameter(OMX_IndexParamAudioDts, &dts);
            if (e != OMX_ErrorNone)
            {
                LOG(VB_AUDIO, LOG_ERR, LOC + QString(
                        "GetParameter AudioDts error %1")
                    .arg(Error2String(e)));
                return false;
            }

            dts.nChannels = m_channels;
            dts.nBitRate = 0;
            dts.nSampleRate = m_samplerate;
            // TODO
            //dts.nDtsType;           // OMX_U32 DTS type 1, 2, or 3
            //dts.nFormat;            // OMX_U32 DTS stream is either big/little endian and 16/14 bit packing
            //dts.nDtsFrameSizeBytes; // OMX_U32 DTS frame size in bytes
            ::SetupChannels(dts);

            LOG(VB_AUDIO, LOG_INFO, LOC + __func__ + QString(
                    " DTS %1 chnls @ %2 sps")
                .arg(dts.nChannels).arg(dts.nSampleRate) );

            e = m_audiorender.SetParameter(OMX_IndexParamAudioDts, &dts);
            if (e != OMX_ErrorNone)
            {
                LOG(VB_AUDIO, LOG_ERR, LOC + QString(
                        "SetParameter AudioDts error %1")
                    .arg(Error2String(e)));
                return false;
            }
            break;
#endif //def OMX_AUDIO_CodingDTS_Supported

          default:
            LOG(VB_AUDIO, LOG_NOTICE, LOC + __func__ +
                QString(" codec %1 not supported")
                    .arg(ff_codec_id_string(AVCodecID(m_codec))));
            return false;
        }
    }
    else
    {
        fmt.eEncoding = OMX_AUDIO_CodingPCM;
        e = m_audiorender.SetParameter(OMX_IndexParamAudioPortFormat, &fmt);
        if (e != OMX_ErrorNone)
        {
            LOG(VB_AUDIO, LOG_ERR, LOC + QString(
                    "SetParameter AudioPortFormat PCM error %1")
                .arg(Error2String(e)));
            return false;
        }

        OMX_AUDIO_PARAM_PCMMODETYPE pcm;
        OMX_DATA_INIT(pcm);
        pcm.nPortIndex = m_audiorender.Base();
        e = m_audiorender.GetParameter(OMX_IndexParamAudioPcm, &pcm);
        if (e != OMX_ErrorNone)
        {
            LOG(VB_AUDIO, LOG_ERR, LOC + QString(
                    "GetParameter AudioPcm error %1")
                .arg(Error2String(e)));
            return false;
        }

        pcm.nChannels = m_channels;
        pcm.nSamplingRate = m_samplerate;
        if (!::Format2Pcm(pcm, m_output_format))
        {
            LOG(VB_AUDIO, LOG_ERR, LOC + __func__ + QString(
                    " Unsupported format %1")
                .arg(AudioOutputSettings::FormatToString(m_output_format)));
            return false;
        }

        ::SetupChannels(pcm);

        LOG(VB_AUDIO, LOG_INFO, LOC + __func__ + QString(
                " PCM %1 chnls @ %2 sps %3 %4 bits")
            .arg(pcm.nChannels).arg(pcm.nSamplingRate).arg(pcm.nBitPerSample)
            .arg(pcm.eNumData == OMX_NumericalDataSigned ? "signed" : "unsigned") );

        e = m_audiorender.SetParameter(OMX_IndexParamAudioPcm, &pcm);
        if (e != OMX_ErrorNone)
        {
            LOG(VB_AUDIO, LOG_ERR, LOC + QString(
                    "SetParameter AudioPcm error %1")
                .arg(Error2String(e)));
            return false;
        }

        nBitPerSample = pcm.nBitPerSample;
    }

    // Setup buffer size & count
    // NB the OpenMAX spec requires PCM buffer size >= 5mS data
    m_audiorender.GetPortDef();
    OMX_PARAM_PORTDEFINITIONTYPE &def = m_audiorender.PortDef();
    def.nBufferSize = std::max(
        OMX_U32((1024 * nBitPerSample * m_channels) / 8),
        def.nBufferSize);
    def.nBufferCountActual = std::max(OMX_U32(10), def.nBufferCountActual);
    //def.bBuffersContiguous = OMX_FALSE;
    def.nBufferAlignment = std::max(OMX_U32(sizeof(int)), def.nBufferAlignment);
    e = m_audiorender.SetParameter(OMX_IndexParamPortDefinition, &def);
    if (e != OMX_ErrorNone)
    {
        LOG(VB_AUDIO, LOG_ERR, LOC + QString(
                "SetParameter PortDefinition error %1")
            .arg(Error2String(e)));
        return false;
    }

    // set AudioOutputBase member variables
    m_soundcard_buffer_size = def.nBufferSize * def.nBufferCountActual;
    m_fragment_size = def.nBufferSize;

#ifdef USING_BROADCOM
    // Select output device
    QString dev;
    if (m_passthru)
        dev = "hdmi";
    else if (m_main_device.contains(":hdmi"))
        dev = "hdmi";
    else
        dev = "local";
    LOG(VB_AUDIO, LOG_INFO, LOC + QString("Output device: '%1'").arg(dev));

    OMX_CONFIG_BRCMAUDIODESTINATIONTYPE dest;
    OMX_DATA_INIT(dest);
    strncpy((char*)dest.sName, dev.toLatin1().constData(), sizeof dest.sName);
    e = m_audiorender.SetConfig(OMX_IndexConfigBrcmAudioDestination, &dest);
    if (e != OMX_ErrorNone)
    {
        LOG(VB_AUDIO, LOG_ERR, LOC + QString(
                "SetConfig BrcmAudioDestination '%1' error %2")
            .arg(dev).arg(Error2String(e)));
        return false;
    }
#endif

    // Goto OMX_StateIdle & allocate buffers
    OMXComponentCB<AudioOutputOMX> cb(this, &AudioOutputOMX::AllocBuffersCB);
    e = m_audiorender.SetState(OMX_StateIdle, 500, &cb);
    if (e != OMX_ErrorNone)
    {
        // DDP/DTS mode returns OMX_ErrorInsufficientResources if o/p != hdmi
        return false;
    }

    // Goto OMX_StateExecuting
    e = m_audiorender.SetState(OMX_StateExecuting, 500);
    if (e != OMX_ErrorNone)
        return false;

    if (internal_vol && !OpenMixer())
        LOG(VB_GENERAL, LOG_ERR, LOC + __func__ +
            " Unable to open audio mixer. Volume control disabled");

    LOG(VB_AUDIO, LOG_INFO, LOC + __func__ + " end");
    return true;
}

// virtual
void AudioOutputOMX::CloseDevice(void)
{
    LOG(VB_AUDIO, LOG_INFO, LOC + __func__);
    m_audiorender.Shutdown();
}

// HDMI uses a different channel order from WAV and others
// See CEA spec: Table 20, Audio InfoFrame

#define REORD_NUMCHAN 6     // Min Num of channels for reorder to be done
#define REORD_A 2           // First channel to switch
#define REORD_B 3           // Second channel to switch

void AudioOutputOMX::reorderChannels(int *aubuf, int size)
{
    int t_size = size;
    int *sample = aubuf;
    while (t_size >= REORD_NUMCHAN*4)
    {
        int savefirst = sample[REORD_A];
        sample[REORD_A] = sample[REORD_B];
        sample[REORD_B] = savefirst;
        sample += m_channels;
        t_size -= m_output_bytes_per_frame;
    }
}

void AudioOutputOMX::reorderChannels(short *aubuf, int size)
{
    int t_size = size;
    short *sample = aubuf;
    while (t_size >= REORD_NUMCHAN*2)
    {
        short savefirst = sample[REORD_A];
        sample[REORD_A] = sample[REORD_B];
        sample[REORD_B] = savefirst;
        sample += m_channels;
        t_size -= m_output_bytes_per_frame;
    }
}

void AudioOutputOMX::reorderChannels(uchar *aubuf, int size)
{
    int t_size = size;
    uchar *sample = aubuf;
    while (t_size >= REORD_NUMCHAN)
    {
        uchar savefirst = sample[REORD_A];
        sample[REORD_A] = sample[REORD_B];
        sample[REORD_B] = savefirst;
        sample += m_channels;
        t_size -= m_output_bytes_per_frame;
    }
}

// virtual
void AudioOutputOMX::WriteAudio(uchar *aubuf, int size)
{
    if (!m_audiorender.IsValid())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + __func__ + " No audio render");
        return;
    }

    // Reorder channels for CEA format
    // See CEA spec: Table 20, Audio InfoFrame
    if (!m_enc && !m_reenc && m_channels >= REORD_NUMCHAN)
    {
        int samplesize = m_output_bytes_per_frame / m_channels;
        switch (samplesize)
        {
            case 1:
                reorderChannels(aubuf, size);
                break;
            case 2:
                reorderChannels((short*)aubuf, size);
                break;
            case 4:
                reorderChannels((int*)aubuf, size);
                break;
        }
    }

    while (size > 0)
    {
        if (!m_ibufs_sema.tryAcquire(1, 3000))
        {
            LOG(VB_AUDIO, LOG_WARNING, LOC + __func__ + " - no input buffers");
            break;
        }
        m_lock.lock();
        assert(!m_ibufs.isEmpty());
        OMX_BUFFERHEADERTYPE *hdr = m_ibufs.takeFirst();
        m_lock.unlock();

        int free = int(hdr->nAllocLen) - int(hdr->nFilledLen + hdr->nOffset);
        int cnt = (free > size) ? size : free;
        memcpy(&hdr->pBuffer[hdr->nOffset + hdr->nFilledLen], aubuf, cnt);
        hdr->nFilledLen += cnt;
        aubuf += cnt;
        size -= cnt;
        m_pending.fetchAndAddRelaxed(cnt);

        hdr->nTimeStamp = S64_TO_TICKS(0);
        hdr->nFlags = 0;

        OMX_ERRORTYPE e = OMX_EmptyThisBuffer(m_audiorender.Handle(), hdr);
        if (e != OMX_ErrorNone)
        {
            LOG(VB_AUDIO, LOG_ERR, LOC + QString(
                "OMX_EmptyThisBuffer error %1").arg(Error2String(e)) );
            m_pending.fetchAndAddRelaxed(-cnt);
            m_lock.lock();
            m_ibufs.append(hdr);
            m_lock.unlock();
            m_ibufs_sema.release();
            break;
        }
    }
}

/**
 * Return the size in bytes of frames currently in the audio buffer adjusted
 * with the audio playback latency
 */
// virtual
int AudioOutputOMX::GetBufferedOnSoundcard(void) const
{
    if (!m_audiorender.IsValid())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + __func__ + " No audio render");
        return 0;
    }

#ifdef USING_BROADCOM
    // output bits per 10 frames
    int obpf;
    if (m_passthru && !usesSpdif())
        obpf = m_source_bitrate * 10 / m_source_samplerate;
    else
        obpf = m_output_bytes_per_frame * 80;

    OMX_PARAM_U32TYPE u;
    OMX_DATA_INIT(u);
    u.nPortIndex = m_audiorender.Base();
    OMX_ERRORTYPE e = m_audiorender.GetConfig(OMX_IndexConfigAudioRenderingLatency, &u);
    if (e != OMX_ErrorNone)
    {
        LOG(VB_AUDIO, LOG_ERR, LOC + QString(
            "GetConfig AudioRenderingLatency error %1").arg(Error2String(e)));
        return 0;
    }
    return u.nU32 * obpf / 80;
#else
    return m_pending;
#endif
}

// virtual
AudioOutputSettings* AudioOutputOMX::GetOutputSettings(bool /*passthrough*/)
{
    LOG(VB_AUDIO, LOG_INFO, LOC + __func__ + " begin");

    if (!m_audiorender.IsValid())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + __func__ + " No audio render");
        return nullptr;
    }

    m_audiorender.Shutdown();

    OMX_ERRORTYPE e = OMX_ErrorNone;
    OMX_AUDIO_PARAM_PORTFORMATTYPE fmt;
    OMX_DATA_INIT(fmt);
    fmt.nPortIndex = m_audiorender.Base();
    fmt.eEncoding = OMX_AUDIO_CodingPCM;
    e = m_audiorender.SetParameter(OMX_IndexParamAudioPortFormat, &fmt);
    if (e != OMX_ErrorNone)
    {
        LOG(VB_AUDIO, LOG_ERR, LOC + QString(
                "SetParameter AudioPortFormat PCM error %1")
            .arg(Error2String(e)));
        return nullptr;
    }

    OMX_AUDIO_PARAM_PCMMODETYPE pcm;
    OMX_DATA_INIT(pcm);
    pcm.nPortIndex = m_audiorender.Base();
    e = m_audiorender.GetParameter(OMX_IndexParamAudioPcm, &pcm);
    if (e != OMX_ErrorNone)
    {
        LOG(VB_AUDIO, LOG_ERR, LOC + QString(
                "GetParameter AudioPcm error %1")
            .arg(Error2String(e)));
        return nullptr;
    }

    AudioOutputSettings *settings = new AudioOutputSettings();

    // NOLINTNEXTLINE(bugprone-infinite-loop)
    while (int rate = settings->GetNextRate())
    {
        pcm.nSamplingRate = rate;
        if (OMX_ErrorNone == m_audiorender.SetParameter(OMX_IndexParamAudioPcm, &pcm))
            settings->AddSupportedRate(rate);
    }

    m_audiorender.GetParameter(OMX_IndexParamAudioPcm, &pcm);

    AudioFormat afmt = FORMAT_NONE;
    while ((afmt = settings->GetNextFormat()))
    {
        if (::Format2Pcm(pcm, afmt) &&
            OMX_ErrorNone == m_audiorender.SetParameter(OMX_IndexParamAudioPcm, &pcm))
        {
            settings->AddSupportedFormat(afmt);
        }
    }

    m_audiorender.GetParameter(OMX_IndexParamAudioPcm, &pcm);

    for (uint channels = 1; channels <= 8; channels++)
    {
#ifdef USING_BELLAGIO // v0.9.3 aborts on 6 channels
        if (channels == 6)
            continue;
#endif
        pcm.nChannels = channels;
        if (OMX_ErrorNone == m_audiorender.SetParameter(OMX_IndexParamAudioPcm, &pcm))
            settings->AddSupportedChannels(channels);
    }

    settings->setFeature(FEATURE_LPCM);
    settings->setPassthrough(-1);
    if (m_main_device.contains(":hdmi"))
    {
#ifdef OMX_AUDIO_CodingDDP_Supported
        fmt.eEncoding = OMX_AUDIO_CodingDDP;
        e = m_audiorender.SetParameter(OMX_IndexParamAudioPortFormat, &fmt);
        if (e == OMX_ErrorNone)
        {
            OMX_AUDIO_PARAM_DDPTYPE ddp;
            OMX_DATA_INIT(ddp);
            ddp.nPortIndex = m_audiorender.Base();
            e = m_audiorender.GetParameter(OMX_IndexParamAudioDdp, &ddp);
            if (e == OMX_ErrorNone)
            {
                LOG(VB_AUDIO, LOG_INFO, LOC + __func__ + " (E)AC3 supported");
                settings->setFeature(FEATURE_AC3);
                settings->setFeature(FEATURE_EAC3);
                settings->setPassthrough(1);
            }
        }
#endif
#ifdef OMX_AUDIO_CodingDTS_Supported
        fmt.eEncoding = OMX_AUDIO_CodingDTS;
        e = m_audiorender.SetParameter(OMX_IndexParamAudioPortFormat, &fmt);
        if (e == OMX_ErrorNone)
        {
            OMX_AUDIO_PARAM_DTSTYPE dts;
            OMX_DATA_INIT(dts);
            dts.nPortIndex = m_audiorender.Base();
            e = m_audiorender.GetParameter(OMX_IndexParamAudioDts, &dts);
            if (e == OMX_ErrorNone)
            {
                LOG(VB_AUDIO, LOG_INFO, LOC + __func__ + " DTS supported");
                settings->setFeature(FEATURE_DTS);
                settings->setFeature(FEATURE_DTSHD);
                settings->setPassthrough(1);
            }
        }
#endif
    }

    LOG(VB_AUDIO, LOG_INFO, LOC + __func__ + " end");
    return settings;
}

// virtual // Returns 0-100
int AudioOutputOMX::GetVolumeChannel(int channel) const
{
    if (channel > 0)
        return -1;

    OMX_AUDIO_CONFIG_VOLUMETYPE v;
    OMX_DATA_INIT(v);
    v.nPortIndex = m_audiorender.Base();
    v.bLinear = OMX_TRUE;
    OMX_ERRORTYPE e = m_audiorender.GetConfig(OMX_IndexConfigAudioVolume, &v);
    if (e != OMX_ErrorNone)
    {
        LOG(VB_AUDIO, LOG_ERR, LOC + QString(
            "GetConfig AudioVolume error %1").arg(Error2String(e)));
        return -1;
    }

    return v.sVolume.nValue;
}

// virtual // range 0-100 for vol
void AudioOutputOMX::SetVolumeChannel(int channel, int volume)
{
    if (channel > 0)
        return;

    OMX_AUDIO_CONFIG_VOLUMETYPE v;
    OMX_DATA_INIT(v);
    v.nPortIndex = m_audiorender.Base();
    v.bLinear = OMX_TRUE;
    v.sVolume.nValue = volume;
    OMX_ERRORTYPE e = m_audiorender.SetConfig(OMX_IndexConfigAudioVolume, &v);
    if (e != OMX_ErrorNone)
    {
        LOG(VB_AUDIO, LOG_ERR, LOC + QString(
            "SetConfig AudioVolume error %1").arg(Error2String(e)));
    }
}

bool AudioOutputOMX::OpenMixer()
{
    QString mixerDevice = gCoreContext->GetSetting("MixerDevice", "OpenMAX:");
    if (!mixerDevice.startsWith("OpenMAX:"))
        return true;

    if (m_set_initial_vol)
    {
        QString controlLabel = gCoreContext->GetSetting("MixerControl", "PCM")
                             + "MixerVolume";
        int initial_vol = gCoreContext->GetNumSetting(controlLabel, 80);
        SetVolumeChannel(0, initial_vol);
    }

    return true;
}

// virtual
OMX_ERRORTYPE AudioOutputOMX::EmptyBufferDone(
    OMXComponent& /*cmpnt*/, OMX_BUFFERHEADERTYPE *hdr)
{
    assert(hdr->nSize == sizeof(OMX_BUFFERHEADERTYPE));
    assert(hdr->nVersion.nVersion == OMX_VERSION);
    int nFilledLen = hdr->nFilledLen;
    hdr->nFilledLen = 0;
    hdr->nFlags = 0;
    if (m_lock.tryLock(1000))
    {
        m_ibufs.append(hdr);
        m_lock.unlock();
        m_pending.fetchAndAddRelaxed(-nFilledLen);
        m_ibufs_sema.release();
    }
    else
        LOG(VB_GENERAL, LOG_CRIT, LOC + "EmptyBufferDone deadlock");
    return OMX_ErrorNone;
}

// Shutdown OMX_StateIdle -> OMX_StateLoaded callback
// virtual
void AudioOutputOMX::ReleaseBuffers(OMXComponent &/*cmpnt*/)
{
    FreeBuffersCB();
}

// Free all OMX buffers
// OMX_CommandPortDisable callback
OMX_ERRORTYPE AudioOutputOMX::FreeBuffersCB()
{
    assert(m_audiorender.IsValid());

    // Free all input buffers
    while (m_ibufs_sema.tryAcquire())
    {
        m_lock.lock();
        assert(!m_ibufs.isEmpty());
        OMX_BUFFERHEADERTYPE *hdr = m_ibufs.takeFirst();
        m_lock.unlock();

        OMX_ERRORTYPE e = OMX_ErrorNone;
        e = OMX_FreeBuffer(m_audiorender.Handle(), m_audiorender.Base(), hdr);
        if (e != OMX_ErrorNone)
            LOG(VB_AUDIO, LOG_ERR, LOC + QString(
                    "OMX_FreeBuffer 0x%1 error %2")
                .arg(quintptr(hdr),0,16).arg(Error2String(e)));
    }
    return OMX_ErrorNone;
}

// OMX_StateIdle callback
OMX_ERRORTYPE AudioOutputOMX::AllocBuffersCB()
{
    assert(m_audiorender.IsValid());
    assert(m_ibufs_sema.available() == 0);
    assert(m_ibufs.isEmpty());

    // Allocate input buffers
    const OMX_PARAM_PORTDEFINITIONTYPE &def = m_audiorender.PortDef();
    OMX_U32 uBufs = def.nBufferCountActual;
    LOG(VB_AUDIO, LOG_DEBUG, LOC + QString(
            "Allocate %1 of %2 byte input buffer(s)")
        .arg(uBufs).arg(def.nBufferSize));
    while (uBufs--)
    {
        OMX_BUFFERHEADERTYPE *hdr = nullptr;
        OMX_ERRORTYPE e = OMX_AllocateBuffer(m_audiorender.Handle(), &hdr,
            def.nPortIndex, this, def.nBufferSize);
        if (e != OMX_ErrorNone)
        {
            LOG(VB_AUDIO, LOG_ERR, LOC + QString(
                "OMX_AllocateBuffer error %1").arg(Error2String(e)) );
            return e;
        }
        if (hdr->nSize != sizeof(OMX_BUFFERHEADERTYPE))
        {
            LOG(VB_AUDIO, LOG_ERR, LOC + "OMX_AllocateBuffer header mismatch");
            OMX_FreeBuffer(m_audiorender.Handle(), def.nPortIndex, hdr);
            return OMX_ErrorVersionMismatch;
        }
        if (hdr->nVersion.nVersion != OMX_VERSION)
        {
            LOG(VB_AUDIO, LOG_ERR, LOC + "OMX_AllocateBuffer version mismatch");
            OMX_FreeBuffer(m_audiorender.Handle(), def.nPortIndex, hdr);
            return OMX_ErrorVersionMismatch;
        }
        hdr->nFilledLen = 0;
        hdr->nOffset = 0;
        m_lock.lock();
        m_ibufs.append(hdr);
        m_lock.unlock();
        m_ibufs_sema.release();
    }
    return OMX_ErrorNone;
}

// EOF
