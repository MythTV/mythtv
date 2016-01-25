#include "audiooutput_omx.h"

#include <cstddef>
#include <cassert>
#include <algorithm> // max/min
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
class AudioDecoderOMX : public OMXComponentCtx
{
    // No copying
    AudioDecoderOMX(const AudioDecoderOMX&);
    AudioDecoderOMX & operator =(const AudioDecoderOMX&);

  public:
    explicit AudioDecoderOMX(OMXComponent&);
    virtual ~AudioDecoderOMX();

    // OMXComponentCtx overrides
    virtual OMX_ERRORTYPE EmptyBufferDone(OMXComponent&, OMX_BUFFERHEADERTYPE*);
    virtual OMX_ERRORTYPE FillBufferDone(OMXComponent&, OMX_BUFFERHEADERTYPE*);
    virtual void ReleaseBuffers(OMXComponent&);

    // Implementation
    void Stop() { m_audiodecoder.Shutdown(); m_bIsStarted = false; }
    bool Start(AVCodecID, AudioFormat, int chnls, int sps);
    bool IsStarted() { return m_bIsStarted; }
    int DecodeAudio(AVCodecContext*, uint8_t*, int&, const AVPacket*);

  protected:
    int GetBufferedFrame(uint8_t *buffer);
    int ProcessPacket(const AVPacket *pkt);

  private:
    OMX_ERRORTYPE FillOutputBuffers();

    // OMXComponentCB actions
    typedef OMX_ERRORTYPE ComponentCB();
    ComponentCB FreeBuffersCB, AllocBuffersCB;
    ComponentCB AllocInputBuffers, AllocOutputBuffers;

  private:
    OMXComponent &m_audiorender;
    OMXComponent m_audiodecoder;
    bool m_bIsStarted;

    QSemaphore m_ibufs_sema;    // EmptyBufferDone signal
    QSemaphore m_obufs_sema;    // FillBufferDone signal
    QMutex mutable m_lock;      // Protects data following
    QList<OMX_BUFFERHEADERTYPE*> m_ibufs;
    QList<OMX_BUFFERHEADERTYPE*> m_obufs;
};


/*
 * Functions
 */
AudioOutputOMX::AudioOutputOMX(const AudioSettings &settings) :
    AudioOutputBase(settings),
    m_audiorender(gCoreContext->GetSetting("OMXAudioRender", AUDIO_RENDER), *this),
    m_audiodecoder(0),
    m_lock(QMutex::Recursive)
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
        if (0) m_audiorender.ShowFormats(port, LOG_DEBUG, VB_AUDIO);
    }

    // Create the OMX audio decoder
    m_audiodecoder = new AudioDecoderOMX(m_audiorender);

    InitSettings(settings);
    if (settings.init)
        Reconfigure(settings);
}

// virtual
AudioOutputOMX::~AudioOutputOMX()
{
    KillAudio();

    // Must shutdown the OMX components now before our state becomes invalid.
    // When the component's dtor is called our state has already been destroyed.
    delete m_audiodecoder, m_audiodecoder = 0;
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
      case 7:
        t.eChannelMapping[6] = OMX_AUDIO_ChannelLS;
      case 6:
        t.eChannelMapping[5] = OMX_AUDIO_ChannelRR;
      case 5:
        t.eChannelMapping[4] = OMX_AUDIO_ChannelLR;
      case 4:
        t.eChannelMapping[3] = OMX_AUDIO_ChannelLFE;
      case 3:
        t.eChannelMapping[2] = OMX_AUDIO_ChannelCF;
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
    if (m_audiodecoder) m_audiodecoder->Stop();

    OMX_ERRORTYPE e;
    unsigned nBitPerSample = 0;

    OMX_AUDIO_PARAM_PORTFORMATTYPE fmt;
    OMX_DATA_INIT(fmt);
    fmt.nPortIndex = m_audiorender.Base();

    if (passthru || enc)
    {
        nBitPerSample = 16;
        switch (codec)
        {
#ifdef OMX_AUDIO_CodingDDP_Supported
          case AV_CODEC_ID_AC3:
          case AV_CODEC_ID_EAC3:
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

            ddp.nChannels = channels;
            ddp.nBitRate = 0;
            ddp.nSampleRate = samplerate;
            ddp.eBitStreamId = (AV_CODEC_ID_AC3 == codec) ?
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

            dts.nChannels = channels;
            dts.nBitRate = 0;
            dts.nSampleRate = samplerate;
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
                    .arg(ff_codec_id_string(AVCodecID(codec))));
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

        pcm.nChannels = channels;
        pcm.nSamplingRate = samplerate;
        if (!::Format2Pcm(pcm, output_format))
        {
            LOG(VB_AUDIO, LOG_ERR, LOC + __func__ + QString(
                    " Unsupported format %1")
                .arg(AudioOutputSettings::FormatToString(output_format)));
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
#define OUT_CHANNELS(n) (((n) > 4) ? 8U: ((n) > 2) ? 4U: unsigned(n))
    def.nBufferSize = std::max(
        OMX_U32((1024 * nBitPerSample * OUT_CHANNELS(channels)) / 8),
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
    soundcard_buffer_size = def.nBufferSize * def.nBufferCountActual;
    fragment_size = def.nBufferSize;

#ifdef USING_BROADCOM
    // Select output device
    QString dev;
    if (passthru)
        dev = "hdmi";
    else if (main_device.contains(":hdmi"))
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

    // Setup the audio decoder
    if (!passthru && m_audiodecoder)
        m_audiodecoder->Start(AVCodecID(codec), output_format, channels, samplerate);

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

// virtual
void AudioOutputOMX::WriteAudio(uchar *aubuf, int size)
{
    if (!m_audiorender.IsValid())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + __func__ + " No audio render");
        return;
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
        free -= cnt;
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
    return u.nU32 * output_bytes_per_frame;
#else
#    if (QT_VERSION >= QT_VERSION_CHECK(5,0,0)) && (QT_VERSION < QT_VERSION_CHECK(5,3,0))
#        error No OpenMAX audio with QT5 before 5.3 due to missing operator int() of QAtomicInt, update your QT or remove libomxil-bellagio-dev
#    endif

    return m_pending;
#endif
}

// virtual
AudioOutputSettings* AudioOutputOMX::GetOutputSettings(bool passthrough)
{
    LOG(VB_AUDIO, LOG_INFO, LOC + __func__ + " begin");

    if (!m_audiorender.IsValid())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + __func__ + " No audio render");
        return NULL;
    }

    m_audiorender.Shutdown();
    if (m_audiodecoder) m_audiodecoder->Stop();

    OMX_ERRORTYPE e;
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
        return NULL;
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
        return NULL;
    }

    AudioOutputSettings *settings = new AudioOutputSettings();

    int rate;
    while ((rate = settings->GetNextRate()))
    {
        pcm.nSamplingRate = rate;
        if (OMX_ErrorNone == m_audiorender.SetParameter(OMX_IndexParamAudioPcm, &pcm))
            settings->AddSupportedRate(rate);
    }

    m_audiorender.GetParameter(OMX_IndexParamAudioPcm, &pcm);

    AudioFormat afmt;
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
    if (main_device.contains(":hdmi"))
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

    if (set_initial_vol)
    {
        QString controlLabel = gCoreContext->GetSetting("MixerControl", "PCM")
                             + "MixerVolume";
        int initial_vol = gCoreContext->GetNumSetting(controlLabel, 80);
        SetVolumeChannel(0, initial_vol);
    }

    return true;
}

// virtual
int AudioOutputOMX::DecodeAudio(AVCodecContext *ctx, uint8_t *buffer,
    int &data_size, const AVPacket *pkt)
{
    if (m_audiodecoder && m_audiodecoder->IsStarted())
    {
        static int s_bShown;
        if (!s_bShown)
        {
            s_bShown = 1;
            LOG(VB_GENERAL, LOG_CRIT, LOC +
                "AudioDecoderOMX::DecodeAudio is available but untested.");
        }

        static int s_enable = gCoreContext->GetNumSetting("OMXAudioDecoderEnable", 0);
        if (s_enable)
            return m_audiodecoder->DecodeAudio(ctx, buffer, data_size, pkt);
    }

    return AudioOutputBase::DecodeAudio(ctx, buffer, data_size, pkt);
}

// virtual
OMX_ERRORTYPE AudioOutputOMX::EmptyBufferDone(
    OMXComponent&, OMX_BUFFERHEADERTYPE *hdr)
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
void AudioOutputOMX::ReleaseBuffers(OMXComponent &cmpnt)
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

        OMX_ERRORTYPE e;
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
        OMX_BUFFERHEADERTYPE *hdr;
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

/*******************************************************************************
 * AudioDecoder
 ******************************************************************************/
#undef LOC
#define LOC QString("ADOMX:%1 ").arg(m_audiodecoder.Id())

AudioDecoderOMX::AudioDecoderOMX(OMXComponent &render) :
    m_audiorender(render),
    m_audiodecoder(gCoreContext->GetSetting("OMXAudioDecode", AUDIO_DECODE), *this),
    m_bIsStarted(false), m_lock(QMutex::Recursive)
{
    if (m_audiodecoder.GetState() != OMX_StateLoaded)
        return;

    if (OMX_ErrorNone != m_audiodecoder.Init(OMX_IndexParamAudioInit))
        return;

    if (!m_audiodecoder.IsValid())
        return;

    // Show default port definitions and audio formats supported
    for (unsigned port = 0; port < m_audiodecoder.Ports(); ++port)
    {
        m_audiodecoder.ShowPortDef(port, LOG_DEBUG, VB_AUDIO);
        if (0) m_audiodecoder.ShowFormats(port, LOG_DEBUG, VB_AUDIO);
    }
}

// virtual
AudioDecoderOMX::~AudioDecoderOMX()
{
    // Must shutdown the OMX components now before our state becomes invalid.
    // When the component's dtor is called our state has already been destroyed.
    m_audiodecoder.Shutdown();
}

static const char *toString(OMX_AUDIO_CHANNELMODETYPE mode)
{
    switch (mode)
    {
        CASE2STR(OMX_AUDIO_ChannelModeStereo);
        CASE2STR(OMX_AUDIO_ChannelModeJointStereo);
        CASE2STR(OMX_AUDIO_ChannelModeDual);
        CASE2STR(OMX_AUDIO_ChannelModeMono);
    }
    static char buf[32];
    return strcpy(buf, qPrintable(QString("ChannelMode 0x%1").arg(mode,0,16)));
}

static const char *toString(OMX_AUDIO_MP3STREAMFORMATTYPE type)
{
    switch (type)
    {
        CASE2STR(OMX_AUDIO_MP3StreamFormatMP1Layer3);
        CASE2STR(OMX_AUDIO_MP3StreamFormatMP2Layer3);
        CASE2STR(OMX_AUDIO_MP3StreamFormatMP2_5Layer3);
    }
    static char buf[32];
    return strcpy(buf, qPrintable(QString("StreamFormat 0x%1").arg(type,0,16)));
}

bool AudioDecoderOMX::Start(AVCodecID codec, AudioFormat format, int chnls, int sps)
{
    if (!m_audiodecoder.IsValid())
        return false;

    Stop();

    if (m_audiodecoder.Ports() < 2)
    {
        LOG(VB_AUDIO, LOG_ERR, LOC + __func__ + ": missing output port");
        return false;
    }

    OMX_ERRORTYPE e;
    OMX_AUDIO_PARAM_PORTFORMATTYPE fmt;
    OMX_DATA_INIT(fmt);

    // Map Ffmpeg audio codec ID to OMX coding
    switch (codec)
    {
      case AV_CODEC_ID_NONE:
        return false;
      case AV_CODEC_ID_PCM_S16LE:
      case AV_CODEC_ID_PCM_S16BE:
      case AV_CODEC_ID_PCM_U16LE:
      case AV_CODEC_ID_PCM_U16BE:
      case AV_CODEC_ID_PCM_S8:
      case AV_CODEC_ID_PCM_U8:
      case AV_CODEC_ID_PCM_S32LE:
      case AV_CODEC_ID_PCM_S32BE:
      case AV_CODEC_ID_PCM_U32LE:
      case AV_CODEC_ID_PCM_U32BE:
      case AV_CODEC_ID_PCM_S24LE:
      case AV_CODEC_ID_PCM_S24BE:
      case AV_CODEC_ID_PCM_U24LE:
      case AV_CODEC_ID_PCM_U24BE:
        fmt.eEncoding = OMX_AUDIO_CodingPCM;
        break;
      case AV_CODEC_ID_MP1:
      case AV_CODEC_ID_MP2:
      case AV_CODEC_ID_MP3:
        fmt.eEncoding = OMX_AUDIO_CodingMP3;
        break;
      case AV_CODEC_ID_AAC:
        fmt.eEncoding = OMX_AUDIO_CodingAAC;
        break;
#ifdef OMX_AUDIO_CodingDDP_Supported
      case AV_CODEC_ID_AC3:
      case AV_CODEC_ID_EAC3:
        fmt.eEncoding = OMX_AUDIO_CodingDDP;
        break;
#endif
#ifdef OMX_AUDIO_CodingDTS_Supported
      case AV_CODEC_ID_DTS:
        fmt.eEncoding = OMX_AUDIO_CodingDTS;
        break;
#endif
      case AV_CODEC_ID_VORBIS:
      case AV_CODEC_ID_FLAC:
        fmt.eEncoding = OMX_AUDIO_CodingVORBIS;
        break;
      case AV_CODEC_ID_WMAV1:
      case AV_CODEC_ID_WMAV2:
        fmt.eEncoding = OMX_AUDIO_CodingWMA;
        break;
#ifdef OMX_AUDIO_CodingATRAC3_Supported
      case AV_CODEC_ID_ATRAC3:
      case AV_CODEC_ID_ATRAC3P:
        fmt.eEncoding = OMX_AUDIO_CodingATRAC3;
        break;
#endif
      default:
        LOG(VB_AUDIO, LOG_NOTICE, LOC + __func__ +
            QString(" codec %1 not supported").arg(ff_codec_id_string(codec)));
        return false;
    }

    // Set input encoding
    fmt.nPortIndex = m_audiodecoder.Base();
    e = m_audiodecoder.SetParameter(OMX_IndexParamAudioPortFormat, &fmt);
    if (e != OMX_ErrorNone)
    {
        LOG(VB_AUDIO, LOG_ERR, LOC + QString(
                "SetParameter input AudioPortFormat error %1")
            .arg(Error2String(e)));
        return false;
    }

    // Set input parameters
    switch (fmt.eEncoding)
    {
      case OMX_AUDIO_CodingPCM:
        OMX_AUDIO_PARAM_PCMMODETYPE pcm;
        OMX_DATA_INIT(pcm);
        pcm.nPortIndex = m_audiodecoder.Base();

        e = m_audiodecoder.GetParameter(OMX_IndexParamAudioPcm, &pcm);
        if (e != OMX_ErrorNone)
        {
            LOG(VB_AUDIO, LOG_ERR, LOC + QString(
                    "GetParameter input AudioPcm error %1")
                .arg(Error2String(e)));
            return false;
        }

        // TODO: Anything other than zeroes here causes bad parameter
        pcm.nChannels = chnls;
        pcm.nSamplingRate = sps;
        if (!::Format2Pcm(pcm, format))
        {
            LOG(VB_AUDIO, LOG_ERR, LOC + __func__ + QString(
                    " Unsupported PCM input format %1")
                .arg(AudioOutputSettings::FormatToString(format)));
            return false;
        }

        ::SetupChannels(pcm);

        LOG(VB_AUDIO, LOG_INFO, LOC + QString(
                "Input PCM %1 chnls @ %2 sps %3 %4 bits")
            .arg(pcm.nChannels).arg(pcm.nSamplingRate).arg(pcm.nBitPerSample)
            .arg(pcm.eNumData == OMX_NumericalDataSigned ? "signed" : "unsigned") );

        e = m_audiodecoder.SetParameter(OMX_IndexParamAudioPcm, &pcm);
        if (e != OMX_ErrorNone)
        {
            LOG(VB_AUDIO, LOG_ERR, LOC + QString(
                    "SetParameter input AudioPcm error %1")
                .arg(Error2String(e)));
            return false;
        }
        break;

      case OMX_AUDIO_CodingMP3:
        OMX_AUDIO_PARAM_MP3TYPE mp3type;
        OMX_DATA_INIT(mp3type);
        mp3type.nPortIndex = m_audiodecoder.Base();

        e = m_audiodecoder.GetParameter(OMX_IndexParamAudioMp3, &mp3type);
        if (e != OMX_ErrorNone)
        {
            LOG(VB_AUDIO, LOG_ERR, LOC + QString(
                    "GetParameter input AudioMp3 error %1")
                .arg(Error2String(e)));
            return false;
        }

        mp3type.nChannels = chnls;
        mp3type.nBitRate = 0;
        mp3type.nSampleRate = sps;
        mp3type.nAudioBandWidth = 0;
        mp3type.eChannelMode = (chnls == 1) ? OMX_AUDIO_ChannelModeMono :
                                OMX_AUDIO_ChannelModeStereo;
        mp3type.eFormat = (codec == AV_CODEC_ID_MP1) ?
                            OMX_AUDIO_MP3StreamFormatMP1Layer3 :
                            OMX_AUDIO_MP3StreamFormatMP2Layer3;
                            // or OMX_AUDIO_MP3StreamFormatMP2_5Layer3

        LOG(VB_AUDIO, LOG_INFO, LOC + QString("Input %1 %2 chnls @ %3 sps %4")
            .arg(toString(mp3type.eFormat)).arg(mp3type.nChannels)
            .arg(mp3type.nSampleRate).arg(toString(mp3type.eChannelMode)) );

        e = m_audiodecoder.SetParameter(OMX_IndexParamAudioMp3, &mp3type);
        if (e != OMX_ErrorNone)
        {
            LOG(VB_AUDIO, LOG_ERR, LOC + QString(
                    "SetParameter output AudioMp3 error %1")
                .arg(Error2String(e)));
            return false;
        }
        break;

#ifdef OMX_AUDIO_CodingDDP_Supported
      case OMX_AUDIO_CodingDDP:
        OMX_AUDIO_PARAM_DDPTYPE ddp;
        OMX_DATA_INIT(ddp);
        ddp.nPortIndex = m_audiodecoder.Base();
        e = m_audiodecoder.GetParameter(OMX_IndexParamAudioDdp, &ddp);
        if (e != OMX_ErrorNone)
        {
            LOG(VB_AUDIO, LOG_ERR, LOC + QString(
                    "GetParameter AudioDdp error %1")
                .arg(Error2String(e)));
            return false;
        }

        ddp.nChannels = chnls;
        ddp.nBitRate = 0;
        ddp.nSampleRate = sps;
        ddp.eBitStreamId = (AV_CODEC_ID_AC3 == codec) ?
                           OMX_AUDIO_DDPBitStreamIdAC3 :
                           OMX_AUDIO_DDPBitStreamIdEAC3;
        ddp.eBitStreamMode = OMX_AUDIO_DDPBitStreamModeCM;
        ddp.eDolbySurroundMode = OMX_AUDIO_DDPDolbySurroundModeNotIndicated;
        ::SetupChannels(ddp);

        LOG(VB_AUDIO, LOG_INFO, LOC + QString("Input %1 %2 chnls @ %3 sps")
            .arg(toString(ddp.eBitStreamId)).arg(ddp.nChannels)
            .arg(ddp.nSampleRate) );

        e = m_audiodecoder.SetParameter(OMX_IndexParamAudioDdp, &ddp);
        if (e != OMX_ErrorNone)
        {
            LOG(VB_AUDIO, LOG_ERR, LOC + QString(
                    "SetParameter AudioDdp error %1")
                .arg(Error2String(e)));
            return false;
        }
        break;
#endif //def OMX_AUDIO_CodingDDP_Supported

      case OMX_AUDIO_CodingVORBIS:
        OMX_AUDIO_PARAM_VORBISTYPE vorbis;
        OMX_DATA_INIT(vorbis);
        vorbis.nPortIndex = m_audiodecoder.Base();
        e = m_audiodecoder.GetParameter(OMX_IndexParamAudioVorbis, &vorbis);
        if (e != OMX_ErrorNone)
        {
            LOG(VB_AUDIO, LOG_ERR, LOC + QString(
                    "GetParameter AudioVorbis error %1")
                .arg(Error2String(e)));
            return false;
        }

        vorbis.nChannels = chnls;
        vorbis.nBitRate = 0;
        vorbis.nMinBitRate = 0;
        vorbis.nMaxBitRate = 0;
        vorbis.nSampleRate = sps;
        vorbis.nAudioBandWidth = 0;
        vorbis.nQuality = 0;
        vorbis.bManaged = OMX_FALSE;
        vorbis.bDownmix = OMX_FALSE;

        LOG(VB_AUDIO, LOG_INFO, LOC + QString("Input Vorbis %1 chnls @ %2 sps")
            .arg(vorbis.nChannels).arg(vorbis.nSampleRate) );

        e = m_audiodecoder.SetParameter(OMX_IndexParamAudioVorbis, &vorbis);
        if (e != OMX_ErrorNone)
        {
            LOG(VB_AUDIO, LOG_ERR, LOC + QString(
                    "SetParameter AudioVorbis error %1")
                .arg(Error2String(e)));
            return false;
        }
        break;

#ifdef OMX_AUDIO_CodingDTS_Supported
      case OMX_AUDIO_CodingDTS:
        OMX_AUDIO_PARAM_DTSTYPE dts;
        OMX_DATA_INIT(dts);
        dts.nPortIndex = m_audiodecoder.Base();
        e = m_audiorender.GetParameter(OMX_IndexParamAudioDts, &dts);
        if (e != OMX_ErrorNone)
        {
            LOG(VB_AUDIO, LOG_ERR, LOC + QString(
                    "GetParameter AudioDts error %1")
                .arg(Error2String(e)));
            return false;
        }

        dts.nChannels = chnls;
        dts.nBitRate = 0;
        dts.nSampleRate = sps;
        // TODO
        //dts.nDtsType;           // OMX_U32 DTS type 1, 2, or 3
        //dts.nFormat;            // OMX_U32 DTS stream is either big/little endian and 16/14 bit packing
        //dts.nDtsFrameSizeBytes; // OMX_U32 DTS frame size in bytes
        ::SetupChannels(dts);

        LOG(VB_AUDIO, LOG_INFO, LOC + QString("Input DTS %1 chnls @ %2 sps")
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

      case OMX_AUDIO_CodingAAC: // TODO
      case OMX_AUDIO_CodingWMA: // TODO
      //case OMX_AUDIO_CodingATRAC3: // TODO
      default:
        LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Unhandled codec %1")
            .arg(Coding2String(fmt.eEncoding)));
        return false;
    }

    // Setup input buffer size & count
    m_audiodecoder.GetPortDef();
    OMX_PARAM_PORTDEFINITIONTYPE &def = m_audiodecoder.PortDef();
    //def.nBufferSize = 1024;
    def.nBufferCountActual = std::max(OMX_U32(2), def.nBufferCountMin);
    assert(def.eDomain == OMX_PortDomainAudio);
    assert(def.format.audio.eEncoding == fmt.eEncoding);
    e = m_audiodecoder.SetParameter(OMX_IndexParamPortDefinition, &def);
    if (e != OMX_ErrorNone)
    {
        LOG(VB_AUDIO, LOG_ERR, LOC + QString(
                "SetParameter PortDefinition error %1")
            .arg(Error2String(e)));
        return false;
    }

#if 0
    // Setup pass through
    OMX_CONFIG_BOOLEANTYPE boolType;
    OMX_DATA_INIT(boolType);
    boolType.bEnabled = OMX_FALSE;
    e = m_audiodecoder.SetParameter(OMX_IndexParamBrcmDecoderPassThrough, &boolType);
    if (e != OMX_ErrorNone)
    {
        LOG(VB_AUDIO, LOG_ERR, LOC + QString(
                "SetParameter BrcmDecoderPassThrough error %1")
            .arg(Error2String(e)));
    }
#endif

    // Setup output encoding
    m_audiodecoder.GetPortDef(1);
    OMX_PARAM_PORTDEFINITIONTYPE &odef = m_audiodecoder.PortDef(1);
    assert(odef.eDomain == OMX_PortDomainAudio);
    if (odef.format.audio.eEncoding != OMX_AUDIO_CodingPCM)
    {
        // Set output encoding
        OMX_AUDIO_PARAM_PORTFORMATTYPE ofmt;
        OMX_DATA_INIT(ofmt);
        ofmt.nPortIndex = odef.nPortIndex;
        ofmt.eEncoding = OMX_AUDIO_CodingPCM;
        e = m_audiodecoder.SetParameter(OMX_IndexParamAudioPortFormat, &ofmt);
        if (e != OMX_ErrorNone)
        {
            LOG(VB_AUDIO, LOG_ERR, LOC + QString(
                    "SetParameter output AudioPortFormat error %1")
                .arg(Error2String(e)));
            return false;
        }
    }

    // Setup output PCM format
    OMX_AUDIO_PARAM_PCMMODETYPE pcm;
    OMX_DATA_INIT(pcm);
    pcm.nPortIndex = odef.nPortIndex;
    pcm.nChannels = chnls;
    pcm.nSamplingRate = sps;
    if (!::Format2Pcm(pcm, format))
    {
        LOG(VB_AUDIO, LOG_ERR, LOC + QString("Unsupported PCM output format %1")
            .arg(AudioOutputSettings::FormatToString(format)));
        return false;
    }
    ::SetupChannels(pcm);

    LOG(VB_AUDIO, LOG_INFO, LOC + QString("Output PCM %1 chnls @ %2 sps %3 %4 bits")
        .arg(pcm.nChannels).arg(pcm.nSamplingRate).arg(pcm.nBitPerSample)
        .arg(pcm.eNumData == OMX_NumericalDataSigned ? "signed" : "unsigned") );

    e = m_audiodecoder.SetParameter(OMX_IndexParamAudioPcm, &pcm);
    if (e != OMX_ErrorNone)
    {
        LOG(VB_AUDIO, LOG_ERR, LOC + QString(
                "SetParameter output AudioPcm error %1")
            .arg(Error2String(e)));
        return false;
    }

    m_audiodecoder.GetPortDef(1);
    assert(odef.format.audio.eEncoding == OMX_AUDIO_CodingPCM);

    // Setup output buffer size & count
    // NB the OpenMAX spec requires PCM buffer size >= 5mS data
    //odef.nBufferSize = 16384;
    odef.nBufferCountActual = std::max(OMX_U32(4), odef.nBufferCountMin);
    e = m_audiodecoder.SetParameter(OMX_IndexParamPortDefinition, &odef);
    if (e != OMX_ErrorNone)
    {
        LOG(VB_AUDIO, LOG_ERR, LOC + QString(
                "SetParameter output PortDefinition error %1")
            .arg(Error2String(e)));
        return false;
    }

#if 0
    // Goto OMX_StateIdle & allocate buffers
    OMXComponentCB<AudioDecoderOMX> cb(this, &AudioDecoderOMX::AllocBuffersCB);
    e = m_audiodecoder.SetState(OMX_StateIdle, 500, &cb);
    switch (e)
    {
      case OMX_ErrorNone:
        break;
      case OMX_ErrorUnsupportedSetting:
        // lvr: only OMX_AUDIO_CodingPCM is currently (17-Dec-2015) supported
        LOG(VB_AUDIO, LOG_WARNING, LOC + QString("%1 is not supported")
            .arg(Coding2String(fmt.eEncoding)));
        return false;
      default:
        LOG(VB_AUDIO, LOG_ERR, LOC + QString("SetState idle error %1")
            .arg(Error2String(e)));
        return false;
    }
#else
    // A disabled port is not populated with buffers on a transition to IDLE
    if (m_audiodecoder.PortDisable(0, 500) != OMX_ErrorNone)
        return false;
    if (m_audiodecoder.PortDisable(1, 500) != OMX_ErrorNone)
        return false;

    // Goto OMX_StateIdle
    e = m_audiodecoder.SetState(OMX_StateIdle, 500);
    if (e != OMX_ErrorNone)
        return false;

    // Enable input port
    OMXComponentCB<AudioDecoderOMX> cb(this, &AudioDecoderOMX::AllocInputBuffers);
    e = m_audiodecoder.PortEnable(0, 500, &cb);
    switch (e)
    {
      case OMX_ErrorNone:
        break;
      case OMX_ErrorUnsupportedSetting:
        // lvr: only OMX_AUDIO_CodingPCM is currently (17-Dec-2015) supported
        LOG(VB_AUDIO, LOG_WARNING, LOC + QString("%1 is not supported")
            .arg(Coding2String(fmt.eEncoding)));
        return false;
      default:
        LOG(VB_AUDIO, LOG_ERR, LOC + QString("SetState idle error %1")
            .arg(Error2String(e)));
        return false;
    }

    OMXComponentCB<AudioDecoderOMX> cb2(this, &AudioDecoderOMX::AllocOutputBuffers);
    e = m_audiodecoder.PortEnable(1, 500, &cb2);
    if (e != OMX_ErrorNone)
        return false;
#endif

    // Goto OMX_StateExecuting
    e = m_audiodecoder.SetState(OMX_StateExecuting, 500);
    if (e != OMX_ErrorNone)
        return false;

    e = FillOutputBuffers();
    if (e != OMX_ErrorNone)
        return false;

    m_bIsStarted = true;
    return true;
}

/**
 * Decode an audio packet, and compact it if data is planar
 * Return negative error code if an error occurred during decoding
 * or the number of bytes consumed from the input AVPacket
 * data_size contains the size of decoded data copied into buffer
 * data decoded will be S16 samples if class instance can't handle HD audio
 * or S16 and above otherwise. No U8 PCM format can be returned
 */
int AudioDecoderOMX::DecodeAudio(AVCodecContext *ctx, uint8_t *buffer,
    int &data_size, const AVPacket *pkt)
{
    if (!m_audiodecoder.IsValid())
        return -1;

    // Check for decoded data
    int ret = GetBufferedFrame(buffer);
    if (ret < 0)
        return ret;
    if (ret > 0)
        data_size = ret;

    // Submit a packet for decoding
    return (pkt && pkt->size) ? ProcessPacket(pkt) : 0;
}

int AudioDecoderOMX::GetBufferedFrame(uint8_t *buffer)
{
    if (!buffer)
        return 0;

    if (!m_obufs_sema.tryAcquire())
        return 0;

    m_lock.lock();
    assert(!m_obufs.isEmpty());
    OMX_BUFFERHEADERTYPE *hdr = m_obufs.takeFirst();
    m_lock.unlock();

    int ret = hdr->nFilledLen;
    memcpy(buffer, &hdr->pBuffer[hdr->nOffset], hdr->nFilledLen);

    hdr->nFlags = 0;
    hdr->nFilledLen = 0;
    OMX_ERRORTYPE e = OMX_FillThisBuffer(m_audiodecoder.Handle(), hdr);
    if (e != OMX_ErrorNone)
        LOG(VB_PLAYBACK, LOG_ERR, LOC + QString(
            "OMX_FillThisBuffer reQ error %1").arg(Error2String(e)) );

    return ret;
}

int AudioDecoderOMX::ProcessPacket(const AVPacket *pkt)
{
    uint8_t *buf = pkt->data;
    int size = pkt->size;
    int ret = pkt->size;

    while (size > 0)
    {
        if (!m_ibufs_sema.tryAcquire(1, 5))
        {
            LOG(VB_AUDIO, LOG_DEBUG, LOC + __func__ + " - no input buffers");
            ret = 0;
            break;
        }
        m_lock.lock();
        assert(!m_ibufs.isEmpty());
        OMX_BUFFERHEADERTYPE *hdr = m_ibufs.takeFirst();
        m_lock.unlock();

        int free = int(hdr->nAllocLen) - int(hdr->nFilledLen + hdr->nOffset);
        int cnt = (free > size) ? size : free;
        memcpy(&hdr->pBuffer[hdr->nOffset + hdr->nFilledLen], buf, cnt);
        hdr->nFilledLen += cnt;
        buf += cnt;
        size -= cnt;
        free -= cnt;

        hdr->nFlags = 0;
        OMX_ERRORTYPE e = OMX_EmptyThisBuffer(m_audiodecoder.Handle(), hdr);
        if (e != OMX_ErrorNone)
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC + QString(
                "OMX_EmptyThisBuffer error %1").arg(Error2String(e)) );
            m_lock.lock();
            m_ibufs.append(hdr);
            m_lock.unlock();
            m_ibufs_sema.release();
            ret = -1;
            break;
        }
    }

    return ret;
}

// OMX_StateIdle callback
OMX_ERRORTYPE AudioDecoderOMX::AllocBuffersCB()
{
    OMX_ERRORTYPE e = AllocInputBuffers();
    return (e == OMX_ErrorNone) ? AllocOutputBuffers() : e;
}

// Allocate decoder input buffers
OMX_ERRORTYPE AudioDecoderOMX::AllocInputBuffers()
{
    assert(m_audiodecoder.IsValid());
    assert(m_ibufs_sema.available() == 0);
    assert(m_ibufs.isEmpty());

    const OMX_PARAM_PORTDEFINITIONTYPE &def = m_audiodecoder.PortDef();
    OMX_U32 uBufs = def.nBufferCountActual;
    LOG(VB_AUDIO, LOG_DEBUG, LOC + __func__ + QString(" %1 x %2 bytes")
            .arg(uBufs).arg(def.nBufferSize));
    while (uBufs--)
    {
        OMX_BUFFERHEADERTYPE *hdr;
        OMX_ERRORTYPE e = OMX_AllocateBuffer(m_audiodecoder.Handle(), &hdr,
            def.nPortIndex, 0, def.nBufferSize);
        if (e != OMX_ErrorNone)
        {
            LOG(VB_AUDIO, LOG_ERR, LOC + QString(
                "OMX_AllocateBuffer error %1").arg(Error2String(e)) );
            return e;
        }
        if (hdr->nSize != sizeof(OMX_BUFFERHEADERTYPE))
        {
            LOG(VB_AUDIO, LOG_ERR, LOC + "OMX_AllocateBuffer header mismatch");
            OMX_FreeBuffer(m_audiodecoder.Handle(), def.nPortIndex, hdr);
            return OMX_ErrorVersionMismatch;
        }
        if (hdr->nVersion.nVersion != OMX_VERSION)
        {
            LOG(VB_AUDIO, LOG_ERR, LOC + "OMX_AllocateBuffer version mismatch");
            OMX_FreeBuffer(m_audiodecoder.Handle(), def.nPortIndex, hdr);
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

// Allocate decoder output buffers
OMX_ERRORTYPE AudioDecoderOMX::AllocOutputBuffers()
{
    assert(m_audiodecoder.IsValid());
    assert(m_obufs_sema.available() == 0);
    assert(m_obufs.isEmpty());

    const OMX_PARAM_PORTDEFINITIONTYPE &def = m_audiodecoder.PortDef(1);
    OMX_U32 uBufs = def.nBufferCountActual;
    LOG(VB_AUDIO, LOG_DEBUG, LOC + __func__ + QString(" %1 x %2 bytes")
            .arg(uBufs).arg(def.nBufferSize));
    while (uBufs--)
    {
        OMX_BUFFERHEADERTYPE *hdr;
        OMX_ERRORTYPE e = OMX_AllocateBuffer(m_audiodecoder.Handle(), &hdr,
            def.nPortIndex, 0, def.nBufferSize);
        if (e != OMX_ErrorNone)
        {
            LOG(VB_AUDIO, LOG_ERR, LOC + QString(
                "OMX_AllocateBuffer error %1").arg(Error2String(e)) );
            return e;
        }
        if (hdr->nSize != sizeof(OMX_BUFFERHEADERTYPE))
        {
            LOG(VB_AUDIO, LOG_ERR, LOC + "OMX_AllocateBuffer header mismatch");
            OMX_FreeBuffer(m_audiodecoder.Handle(), def.nPortIndex, hdr);
            return OMX_ErrorVersionMismatch;
        }
        if (hdr->nVersion.nVersion != OMX_VERSION)
        {
            LOG(VB_AUDIO, LOG_ERR, LOC + "OMX_AllocateBuffer version mismatch");
            OMX_FreeBuffer(m_audiodecoder.Handle(), def.nPortIndex, hdr);
            return OMX_ErrorVersionMismatch;
        }
        hdr->nFilledLen = 0;
        hdr->nOffset = 0;
        m_lock.lock();
        m_obufs.append(hdr);
        m_lock.unlock();
        m_obufs_sema.release();
    }
    return OMX_ErrorNone;
}

// Shutdown OMX_StateIdle -> OMX_StateLoaded callback
// virtual
void AudioDecoderOMX::ReleaseBuffers(OMXComponent &cmpnt)
{
    FreeBuffersCB();
}

// Free all OMX buffers
// OMX_CommandPortDisable callback
OMX_ERRORTYPE AudioDecoderOMX::FreeBuffersCB()
{
    assert(m_audiodecoder.IsValid());

    // Free all input buffers
    while (m_ibufs_sema.tryAcquire())
    {
        m_lock.lock();
        assert(!m_ibufs.isEmpty());
        OMX_BUFFERHEADERTYPE *hdr = m_ibufs.takeFirst();
        m_lock.unlock();

        assert(hdr->nSize == sizeof(OMX_BUFFERHEADERTYPE));
        assert(hdr->nVersion.nVersion == OMX_VERSION);

        OMX_ERRORTYPE e;
        e = OMX_FreeBuffer(m_audiodecoder.Handle(), m_audiodecoder.Base(), hdr);
        if (e != OMX_ErrorNone)
            LOG(VB_AUDIO, LOG_ERR, LOC + QString(
                    "OMX_FreeBuffer 0x%1 error %2")
                .arg(quintptr(hdr),0,16).arg(Error2String(e)));
    }

    // Free all output buffers
    while (m_obufs_sema.tryAcquire())
    {
        m_lock.lock();
        assert(!m_obufs.isEmpty());
        OMX_BUFFERHEADERTYPE *hdr = m_obufs.takeFirst();
        m_lock.unlock();

        assert(hdr->nSize == sizeof(OMX_BUFFERHEADERTYPE));
        assert(hdr->nVersion.nVersion == OMX_VERSION);

        OMX_ERRORTYPE e;
        e = OMX_FreeBuffer(m_audiodecoder.Handle(), m_audiodecoder.Base() + 1, hdr);
        if (e != OMX_ErrorNone)
            LOG(VB_AUDIO, LOG_ERR, LOC + QString(
                    "OMX_FreeBuffer 0x%1 error %2")
                .arg(quintptr(hdr),0,16).arg(Error2String(e)));
    }

    return OMX_ErrorNone;
}

// virtual
OMX_ERRORTYPE AudioDecoderOMX::EmptyBufferDone(
    OMXComponent&, OMX_BUFFERHEADERTYPE *hdr)
{
    assert(hdr->nSize == sizeof(OMX_BUFFERHEADERTYPE));
    assert(hdr->nVersion.nVersion == OMX_VERSION);
    hdr->nFilledLen = 0;
    hdr->nFlags = 0;
    if (m_lock.tryLock(1000))
    {
        m_ibufs.append(hdr);
        m_lock.unlock();
        m_ibufs_sema.release();
    }
    else
        LOG(VB_GENERAL, LOG_CRIT, LOC + "EmptyBufferDone deadlock");
    return OMX_ErrorNone;
}

// virtual
OMX_ERRORTYPE AudioDecoderOMX::FillBufferDone(
    OMXComponent&, OMX_BUFFERHEADERTYPE *hdr)
{
    assert(hdr->nSize == sizeof(OMX_BUFFERHEADERTYPE));
    assert(hdr->nVersion.nVersion == OMX_VERSION);
    if (m_lock.tryLock(1000))
    {
        m_obufs.append(hdr);
        m_lock.unlock();
        m_obufs_sema.release();
    }
    else
        LOG(VB_GENERAL, LOG_CRIT, LOC + "FillBufferDone deadlock");
    return OMX_ErrorNone;
}

// Start filling the output buffers
OMX_ERRORTYPE AudioDecoderOMX::FillOutputBuffers()
{
    while (m_obufs_sema.tryAcquire())
    {
        m_lock.lock();
        assert(!m_obufs.isEmpty());
        OMX_BUFFERHEADERTYPE *hdr = m_obufs.takeFirst();
        m_lock.unlock();

        assert(hdr->nSize == sizeof(OMX_BUFFERHEADERTYPE));
        assert(hdr->nVersion.nVersion == OMX_VERSION);

        hdr->nFlags = 0;
        hdr->nFilledLen = 0;
        OMX_ERRORTYPE e = OMX_FillThisBuffer(m_audiodecoder.Handle(), hdr);
        if (e != OMX_ErrorNone)
        {
            LOG(VB_AUDIO, LOG_ERR, LOC + QString(
                "OMX_FillThisBuffer error %1").arg(Error2String(e)) );
            m_lock.lock();
            m_obufs.append(hdr);
            m_lock.unlock();
            m_obufs_sema.release();
            return e;
        }
    }

    return OMX_ErrorNone;
}
// EOF
