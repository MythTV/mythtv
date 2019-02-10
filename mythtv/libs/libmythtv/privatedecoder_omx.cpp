// Qt
#include <QMutexLocker>

// MythTV
#include "avformatdecoder.h"
#include "mythcorecontext.h"
#include "mythlogging.h"
#include "omxcontext.h"
#include "mythavutil.h"
#include "privatedecoder_omx.h"

// FFmpeg
extern "C" {
#include "libavutil/pixdesc.h"
#include "libavcodec/avcodec.h"
#include "libavutil/imgutils.h"
}

// Std
#include <algorithm>
#include <cstddef>

// OpenMax
#include <OMX_Video.h>
#ifdef USING_BROADCOM
#include <OMX_Broadcom.h>
#endif

using namespace omxcontext;

#define LOC QString("DOMX:%1 ").arg(m_videoDecoder.Id())

// Stringize a macro
#define _STR(s) #s
#define STR(s) _STR(s)

// VideoFrame <> OMX_BUFFERHEADERTYPE
#define FRAMESETHEADER(f,h)    ((f)->priv[2] = reinterpret_cast<unsigned char* >(h))
#define FRAMEUNSETHEADER(f)    ((f)->priv[2] = nullptr)
#define FRAME2HEADER(f)        reinterpret_cast<OMX_BUFFERHEADERTYPE*>((f)->priv[2])
#define FRAMESETBUFFERREF(f,r) ((f)->priv[1] = reinterpret_cast<unsigned char* >(r))
#define FRAMEUNSETBUFFERREF(f) ((f)->priv[1] = nullptr)
#define FRAME2BUFFERREF(f)     (reinterpret_cast<AVBufferRef*>((f)->priv[1]))
#define HEADER2FRAME(h)        (static_cast<VideoFrame*>((h)->pAppPrivate))

// Component name
#ifdef USING_BELLAGIO
# define VIDEO_DECODE "" // Not implemented
#else
# define VIDEO_DECODE "video_decode"
#endif

typedef OMX_ERRORTYPE ComponentCB();

static const char *H264Profile2String(int profile);

/// \brief Convert between FFMpeg PTS and OMX ticks
static inline OMX_TICKS Pts2Ticks(AVStream *Stream, int64_t PTS)
{
    if (PTS == AV_NOPTS_VALUE)
        return S64_TO_TICKS(0);
    return S64_TO_TICKS( int64_t((av_q2d(Stream->time_base) * PTS)* OMX_TICKS_PER_SECOND));
}

/// \brief Convert between OMX ticks and FFMpeg PTS
static inline int64_t Ticks2Pts(AVStream *Stream, OMX_TICKS Ticks)
{
    return int64_t((TICKS_TO_S64(Ticks) / av_q2d(Stream->time_base)) / OMX_TICKS_PER_SECOND);
}

const QString PrivateDecoderOMX::DecoderName = QString("openmax");

void PrivateDecoderOMX::GetDecoders(render_opts &Options)
{
    Options.decoders->append(DecoderName);
    (*Options.equiv_decoders)[DecoderName].append("nuppel");
    (*Options.equiv_decoders)[DecoderName].append("ffmpeg");
    (*Options.equiv_decoders)[DecoderName].append("dummy");
}

PrivateDecoderOMX::PrivateDecoderOMX()
  : m_videoDecoder(gCoreContext->GetSetting("OMXVideoDecode", VIDEO_DECODE), *this),
    m_bitstreamFilter(nullptr),
    m_startTime(false),
    m_avctx(nullptr),
    m_settingsChangedLock(QMutex::Recursive),
    m_settingsChanged(false),
    m_settingsHaveChanged(false)
{
    if (OMX_ErrorNone != m_videoDecoder.Init(OMX_IndexParamVideoInit))
        return;

    if (!m_videoDecoder.IsValid())
        return;

    // Show default port definitions and video formats supported
    for (unsigned port = 0; port < m_videoDecoder.Ports(); ++port)
    {
        m_videoDecoder.ShowPortDef(port, LOG_DEBUG);
        m_videoDecoder.ShowFormats(port, LOG_DEBUG);
    }
}

PrivateDecoderOMX::~PrivateDecoderOMX()
{
    // Must shutdown the decoder now before our state becomes invalid.
    // When the decoder dtor is called our state has already been destroyed.
    m_videoDecoder.Shutdown();

    // flush and delete the bitstream filter
    if (m_bitstreamFilter)
        av_bsf_free(&m_bitstreamFilter);
}

QString PrivateDecoderOMX::GetName(void)
{
    return DecoderName;
}

bool PrivateDecoderOMX::Init(const QString &Decoder, PlayerFlags Flags, AVCodecContext *AVCtx)
{
    if (Decoder != DecoderName || !(Flags & kDecodeAllowEXT) || !AVCtx)
        return false;

    if (getenv("NO_OPENMAX"))
    {
        LOG(VB_PLAYBACK, LOG_NOTICE, LOC + "OpenMAX decoder disabled");
        return false;
    }

    if (!m_videoDecoder.IsValid())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "No video decoder");
        return false;
    }

    if (m_videoDecoder.Ports() < 2)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "No video decoder ports");
        return false;
    }

    OMX_VIDEO_CODINGTYPE type = OMX_VIDEO_CodingUnused;
    switch (AVCtx->codec_id)
    {
      case AV_CODEC_ID_MPEG1VIDEO:
      case AV_CODEC_ID_MPEG2VIDEO:
        type = OMX_VIDEO_CodingMPEG2;
        break;
      case AV_CODEC_ID_H263:
      case AV_CODEC_ID_H263P:
      case AV_CODEC_ID_H263I:
        type = OMX_VIDEO_CodingH263;
        break;
      case AV_CODEC_ID_RV10:
      case AV_CODEC_ID_RV20:
      case AV_CODEC_ID_RV30:
      case AV_CODEC_ID_RV40:
        type = OMX_VIDEO_CodingRV;
        break;
      case AV_CODEC_ID_MJPEG:
      case AV_CODEC_ID_MJPEGB:
        type = OMX_VIDEO_CodingMJPEG;
        break;
      case AV_CODEC_ID_MPEG4:
        type = OMX_VIDEO_CodingMPEG4;
        break;
      case AV_CODEC_ID_WMV1:
      case AV_CODEC_ID_WMV2:
      case AV_CODEC_ID_WMV3:
        type = OMX_VIDEO_CodingWMV;
        break;
      case AV_CODEC_ID_H264:
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("Codec H264 %1").arg(H264Profile2String(AVCtx->profile)));
        type = OMX_VIDEO_CodingAVC;
        break;
#ifdef OMX_AUDIO_CodingTheora_Supported
      case AV_CODEC_ID_THEORA:
        type = OMX_VIDEO_CodingTheora;
        break;
#endif
#ifdef OMX_AUDIO_CodingVP6_Supported
      case AV_CODEC_ID_VP3:
      case AV_CODEC_ID_VP5:
      case AV_CODEC_ID_VP6:
      case AV_CODEC_ID_VP6F:
      case AV_CODEC_ID_VP6A:
        type = OMX_VIDEO_CodingVP6;
        break;
#endif
#ifdef OMX_AUDIO_CodingVP8_Supported
      case AV_CODEC_ID_VP8:
        type = OMX_VIDEO_CodingVP8;
        break;
#endif
#ifdef OMX_AUDIO_CodingVP9_Supported
      case AV_CODEC_ID_VP9:
        type = OMX_VIDEO_CodingVP9;
        break;
#endif
#ifdef OMX_AUDIO_CodingMVC_Supported
      case AV_CODEC_ID_MVC1:
      case AV_CODEC_ID_MVC2:
        type = OMX_VIDEO_CodingMVC;
        break;
#endif
      default:
        LOG(VB_PLAYBACK, LOG_ERR, LOC + QString("Codec %1 not supported")
            .arg(ff_codec_id_string(AVCtx->codec_id)));
        return false;
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("FFmpeg codec '%1' mapped to OpenMax '%2'")
        .arg(ff_codec_id_string(AVCtx->codec_id)).arg(Coding2String(type)));

    // Set input format
    OMX_VIDEO_PARAM_PORTFORMATTYPE portfmt;
    OMX_DATA_INIT(portfmt);
    portfmt.nPortIndex = m_videoDecoder.Base();
    portfmt.eCompressionFormat = type; // OMX_VIDEO_CodingAutoDetect gives error
    portfmt.eColorFormat = OMX_COLOR_FormatUnused;
    OMX_ERRORTYPE e = m_videoDecoder.SetParameter(OMX_IndexParamVideoPortFormat, &portfmt);
    if (e != OMX_ErrorNone)
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC + QString("Set input IndexParamVideoPortFormat error %1")
            .arg(Error2String(e)));
        return false;
    }

    // create bitstream filter if needed
    if ((type == OMX_VIDEO_CodingAVC) && (SetNalType(AVCtx) != OMX_ErrorNone))
        if (AVCtx->extradata_size > 0 && AVCtx->extradata[0] == 1)
            if (!CreateFilter(AVCtx))
                return false;

    // Check output port default pixel format
    switch (m_videoDecoder.PortDef(1).format.video.eColorFormat)
    {
        case OMX_COLOR_FormatYUV420Planar:
        case OMX_COLOR_FormatYUV420PackedPlanar: break; // Broadcom default
        default:
            // Set output pixel format
            portfmt.nPortIndex = m_videoDecoder.Base() + 1;
            portfmt.eCompressionFormat = OMX_VIDEO_CodingUnused;
            portfmt.eColorFormat = OMX_COLOR_FormatYUV420PackedPlanar;
            e = m_videoDecoder.SetParameter(OMX_IndexParamVideoPortFormat, &portfmt);
            if (e != OMX_ErrorNone)
            {
                LOG(VB_PLAYBACK, LOG_ERR, LOC + QString("Set output IndexParamVideoPortFormat error %1")
                    .arg(Error2String(e)));
                return false;
            }
            break;
    }

    AVCtx->pix_fmt = AV_PIX_FMT_YUV420P;

    // Update input buffers (default is 20 preset in OMX)
    m_videoDecoder.GetPortDef(0);
    OMX_PARAM_PORTDEFINITIONTYPE &inputport = m_videoDecoder.PortDef(0);
    OMX_U32 inputBuffers = OMX_U32(gCoreContext->GetNumSetting("OmxInputBuffers", 30));
    if (inputBuffers > 0U && inputBuffers != inputport.nBufferCountActual && inputBuffers > inputport.nBufferCountMin)
    {
        inputport.nBufferCountActual = inputBuffers;
        e = m_videoDecoder.SetParameter(OMX_IndexParamPortDefinition, &inputport);
        if (e != OMX_ErrorNone)
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC + QString("Set input IndexParamPortDefinition error %1")
                .arg(Error2String(e)));
            return false;
        }
    }
    m_videoDecoder.GetPortDef(0);
    m_videoDecoder.ShowPortDef(0, LOG_INFO);

    // Ensure at least 2 output buffers
    OMX_PARAM_PORTDEFINITIONTYPE &outputport = m_videoDecoder.PortDef(1);
    if (outputport.nBufferCountActual < 2U || outputport.nBufferCountActual < outputport.nBufferCountMin)
    {
        outputport.nBufferCountActual = std::max(OMX_U32(2), outputport.nBufferCountMin);
        e = m_videoDecoder.SetParameter(OMX_IndexParamPortDefinition, &outputport);
        if (e != OMX_ErrorNone)
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC + QString("Set output IndexParamPortDefinition error %1")
                .arg(Error2String(e)));
            return false;
        }
    }
    m_videoDecoder.GetPortDef(0);
    m_videoDecoder.ShowPortDef(1, LOG_INFO);

    // Goto OMX_StateIdle & allocate all buffers
    // This generates an error if fmt.eCompressionFormat is not supported
    OMXComponentCB<PrivateDecoderOMX> cb(this, &PrivateDecoderOMX::AllocBuffersCB);
    e = m_videoDecoder.SetState(OMX_StateIdle, 500, &cb);
    if (e != OMX_ErrorNone)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to allocated OpenMax buffers");
        if (type == OMX_VIDEO_CodingMPEG2 || type == OMX_VIDEO_CodingWMV)
            LOG(VB_GENERAL, LOG_WARNING, LOC + "Note: MPEG2 and WMV/VC1 require licenses");
        return false;
    }

    m_settingsHaveChanged = false;

    // Goto OMX_StateExecuting
    e = m_videoDecoder.SetState(OMX_StateExecuting, 500);
    if (e != OMX_ErrorNone)
        return false;

    e = FillOutputBuffers();
    if (e != OMX_ErrorNone)
        return false;
    return true;
}

// Setup H264 decoder for NAL type
OMX_ERRORTYPE PrivateDecoderOMX::SetNalType(AVCodecContext *AVCtx)
{
#ifdef USING_BROADCOM
    OMX_NALSTREAMFORMATTYPE nalfmt;
    OMX_DATA_INIT(nalfmt);
    nalfmt.nPortIndex = m_videoDecoder.Base();

    OMX_ERRORTYPE err = m_videoDecoder.GetParameter(OMX_INDEXTYPE(OMX_IndexParamNalStreamFormatSupported), &nalfmt);
    if (err != OMX_ErrorNone)
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC + QString("Get ParamNalStreamFormatSupported error %1").arg(Error2String(err)));
        if (AVCtx->extradata_size == 0 || AVCtx->extradata[0] != 1)
            return OMX_ErrorNone; // Annex B, no filter needed
        return err;
    }

    static bool debugged = false;
    if (!debugged)
    {
        debugged = true;
        QStringList list;
        if (nalfmt.eNaluFormat & OMX_NaluFormatStartCodes)
            list << "StartCodes (Annex B)";
        if (nalfmt.eNaluFormat & OMX_NaluFormatOneNaluPerBuffer)
            list << "OneNaluPerBuffer";
        if (nalfmt.eNaluFormat & OMX_NaluFormatOneByteInterleaveLength)
            list << "OneByteInterleaveLength";
        if (nalfmt.eNaluFormat & OMX_NaluFormatTwoByteInterleaveLength)
            list << "TwoByteInterleaveLength";
        if (nalfmt.eNaluFormat & OMX_NaluFormatFourByteInterleaveLength)
            list << "FourByteInterleaveLength";
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "NalStreamFormatSupported: " + list.join(", "));
    }

    OMX_NALUFORMATSTYPE type;
    QString naluFormat;
    if (AVCtx->extradata_size >= 5 && AVCtx->extradata[0] == 1)
    {
        // AVCC NALs (mp4 stream)
        int n = 1 + (AVCtx->extradata[4] & 0x3);
        switch (n)
        {
            case 1:
                type = OMX_NaluFormatOneByteInterleaveLength;
                naluFormat = "OneByteInterleaveLength";
                break;
            case 2:
                type = OMX_NaluFormatTwoByteInterleaveLength;
                naluFormat = "TwoByteInterleaveLength";
                break;
            case 4:
                type = OMX_NaluFormatFourByteInterleaveLength;
                naluFormat = "FourByteInterleaveLength";
                break;
            default:
                return OMX_ErrorUnsupportedSetting;
        }
    }
    else
    {
        type = OMX_NaluFormatStartCodes; // Annex B
        naluFormat = "StartCodes";
    }

    nalfmt.eNaluFormat = OMX_NALUFORMATSTYPE(nalfmt.eNaluFormat & type);
    if (!nalfmt.eNaluFormat)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Unsupported NAL stream format " + naluFormat);
        return OMX_ErrorUnsupportedSetting;
    }

    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + "NAL stream format: " + naluFormat);

    err = m_videoDecoder.SetParameter(OMX_INDEXTYPE(OMX_IndexParamNalStreamFormatSelect), &nalfmt);
    if (err != OMX_ErrorNone)
        LOG(VB_PLAYBACK, LOG_ERR, LOC + QString("Set ParamNalStreamFormatSelect(%1) error %2")
            .arg(naluFormat).arg(Error2String(err)));
    return err;
#else
    if (AVCtx->extradata_size == 0 || AVCtx->extradata[0] != 1)
        return OMX_ErrorNone; // Annex B, no filter needed

    return OMX_ErrorUnsupportedSetting;
#endif //USING_BROADCOM
}


bool PrivateDecoderOMX::CreateFilter(AVCodecContext *AVCtx)
{
    // Check NAL size
    if (AVCtx->extradata_size < 5)
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC + "AVCC extradata_size too small");
        return false;
    }

    int size = 1 + (AVCtx->extradata[4] & 0x3);
    switch (size)
    {
        case 1:
        case 2:
        case 4:
            break;
        default:
            LOG(VB_PLAYBACK, LOG_ERR, LOC + QString("Invalid NAL size (%1)").arg(size));
            return false;
    }

    if (m_bitstreamFilter)
        return true;

    const AVBitStreamFilter *filter = av_bsf_get_by_name("h264_mp4toannexb");
    if (!filter)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create bitstream filter");
        return false;
    }

    if (av_bsf_alloc(filter, &m_bitstreamFilter) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to alloc bitstream filter");
        return false;
    }

    if ((avcodec_parameters_from_context(m_bitstreamFilter->par_in, AVCtx) < 0) ||
        (av_bsf_init(m_bitstreamFilter) < 0))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to initialise bitstream filter");
        av_bsf_free(&m_bitstreamFilter);
        m_bitstreamFilter = nullptr;
        return false;
    }

    if (avcodec_parameters_to_context(AVCtx, m_bitstreamFilter->par_out) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to setup bitstream filter");
        av_bsf_free(&m_bitstreamFilter);
        m_bitstreamFilter = nullptr;
        return false;
    }

    LOG(VB_GENERAL, LOG_INFO, LOC + "Created h264_mp4toannexb filter");
    return true;
}

OMX_ERRORTYPE PrivateDecoderOMX::AllocBuffersCB(void)
{
    // Allocate input buffers
    uint index = 0;
    const OMX_PARAM_PORTDEFINITIONTYPE &portdef = m_videoDecoder.PortDef(index);
    OMX_U32 bufferstocreate = portdef.nBufferCountActual;
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("Creating %1 %2byte input buffers").arg(bufferstocreate).arg(portdef.nBufferSize));
    while (bufferstocreate--)
    {
        OMX_BUFFERHEADERTYPE *header;
        OMX_ERRORTYPE err = OMX_AllocateBuffer(m_videoDecoder.Handle(), &header,
            m_videoDecoder.Base() + index, this, portdef.nBufferSize);
        if (err != OMX_ErrorNone)
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC + QString("OMX_AllocateBuffer error %1").arg(Error2String(err)) );
            return err;
        }
        if (header->nSize != sizeof(OMX_BUFFERHEADERTYPE))
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC + "OMX_AllocateBuffer header mismatch");
            OMX_FreeBuffer(m_videoDecoder.Handle(), m_videoDecoder.Base() + index, header);
            return OMX_ErrorVersionMismatch;
        }
        if (header->nVersion.nVersion != OMX_VERSION)
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC + "OMX_AllocateBuffer version mismatch");
            OMX_FreeBuffer(m_videoDecoder.Handle(), m_videoDecoder.Base() + index, header);
            return OMX_ErrorVersionMismatch;
        }
        header->nFilledLen = 0;
        header->nOffset = 0;
        m_settingsChangedLock.lock();
        m_inputBuffers.append(header);
        m_settingsChangedLock.unlock();
        m_inputBuffersSem.release();
    }

    return AllocOutputBuffersCB();
}

OMX_ERRORTYPE PrivateDecoderOMX::FillOutputBuffers()
{
    while (m_outputBuffersSem.tryAcquire())
    {
        m_settingsChangedLock.lock();
        OMX_BUFFERHEADERTYPE *header = m_outputBuffers.takeFirst();
        m_settingsChangedLock.unlock();

        header->nFlags = 0;
        header->nFilledLen = 0;
        OMX_ERRORTYPE err = OMX_FillThisBuffer(m_videoDecoder.Handle(), header);
        if (err != OMX_ErrorNone)
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC + QString("OMX_FillThisBuffer error %1").arg(Error2String(err)));
            m_settingsChangedLock.lock();
            m_outputBuffers.append(header);
            m_settingsChangedLock.unlock();
            m_outputBuffersSem.release();
            return err;
        }
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE PrivateDecoderOMX::FreeOutputBuffersCB()
{
    const int index = 1;
    while (m_outputBuffersSem.tryAcquire(1, !m_videoDecoder.IsCommandComplete() ? 100 : 0) )
    {
        m_settingsChangedLock.lock();
        OMX_BUFFERHEADERTYPE *hdr = m_outputBuffers.takeFirst();
        m_settingsChangedLock.unlock();

        VideoFrame *frame = HEADER2FRAME(hdr);
        if (frame && (FRAME2HEADER(frame) == hdr))
        {
            FRAMEUNSETHEADER(frame);

            AVBufferRef *ref = FRAME2BUFFERREF(frame);
            if (ref)
            {
                FRAMEUNSETBUFFERREF(frame);
                av_buffer_unref(&ref);
            }
        }

        OMX_ERRORTYPE e = OMX_FreeBuffer(m_videoDecoder.Handle(), m_videoDecoder.Base() + index, hdr);
        if (e != OMX_ErrorNone)
            LOG(VB_PLAYBACK, LOG_ERR, LOC + QString(
                    "OMX_FreeBuffer 0x%1 error %2")
                .arg(quintptr(hdr),0,16).arg(Error2String(e)));
    }
    return OMX_ErrorNone;
}

OMX_ERRORTYPE PrivateDecoderOMX::AllocOutputBuffersCB()
{
    const int index = 1;
    const OMX_PARAM_PORTDEFINITIONTYPE *pdef = &m_videoDecoder.PortDef(index);
    OMX_U32 uBufs = pdef->nBufferCountActual;
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString(
            "Allocate %1 of %2 byte output buffer(s)")
        .arg(uBufs).arg(pdef->nBufferSize));
    while (uBufs--)
    {
        OMX_BUFFERHEADERTYPE *hdr;
        OMX_ERRORTYPE e = OMX_AllocateBuffer(m_videoDecoder.Handle(), &hdr,
            m_videoDecoder.Base() + index, nullptr, pdef->nBufferSize);
        if (e != OMX_ErrorNone)
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC + QString(
                "OMX_AllocateBuffer error %1").arg(Error2String(e)) );
            return e;
        }
        if (hdr->nSize != sizeof(OMX_BUFFERHEADERTYPE))
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC + "OMX_AllocateBuffer header mismatch");
            OMX_FreeBuffer(m_videoDecoder.Handle(), m_videoDecoder.Base() + index, hdr);
            return OMX_ErrorVersionMismatch;
        }
        if (hdr->nVersion.nVersion != OMX_VERSION)
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC + "OMX_AllocateBuffer version mismatch");
            OMX_FreeBuffer(m_videoDecoder.Handle(), m_videoDecoder.Base() + index, hdr);
            return OMX_ErrorVersionMismatch;
        }
        hdr->nFilledLen = 0;
        hdr->nOffset = 0;
        m_outputBuffers.append(hdr);
        m_outputBuffersSem.release();
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE PrivateDecoderOMX::UseBuffersCB()
{
    const int index = 1;
    const OMX_PARAM_PORTDEFINITIONTYPE &portdef = m_videoDecoder.PortDef(index);
    OMX_U32 uBufs = portdef.nBufferCountActual;

    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("Use %1 of %2byte output buffer(s)")
        .arg(uBufs).arg(portdef.nBufferSize));

    OMX_ERRORTYPE err = OMX_ErrorNone;
    while (uBufs--)
    {
        MythAVFrame picture;
        if (m_avctx->get_buffer2(m_avctx, picture, 0) < 0)
            return OMX_ErrorOverflow;

        VideoFrame *frame = static_cast<VideoFrame*>(picture->opaque);
        OMX_BUFFERHEADERTYPE *header;
        err = OMX_UseBuffer(m_videoDecoder.Handle(), &header, m_videoDecoder.Base() + index, frame,
                            portdef.nBufferSize, frame->buf);
        if (err != OMX_ErrorNone)
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC + QString("OMX_UseBuffer error %1").arg(Error2String(err)));
            return err;
        }
        if (header->nSize != sizeof(OMX_BUFFERHEADERTYPE))
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC + "OMX_UseBuffer header mismatch");
            OMX_FreeBuffer(m_videoDecoder.Handle(), m_videoDecoder.Base() + index, header);
            return OMX_ErrorVersionMismatch;
        }
        if (header->nVersion.nVersion != OMX_VERSION)
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC + "OMX_UseBuffer version mismatch");
            OMX_FreeBuffer(m_videoDecoder.Handle(), m_videoDecoder.Base() + index, header);
            return OMX_ErrorVersionMismatch;
        }

        FRAMESETHEADER(frame, header);
        FRAMESETBUFFERREF(frame, picture->buf[0]);
        picture->buf[0] = nullptr;
        header->nFilledLen = 0;
        header->nOffset = 0;
        m_outputBuffers.append(header);
        m_outputBuffersSem.release();
    }

    return err;
}

bool PrivateDecoderOMX::Reset(void)
{
    if (!m_videoDecoder.IsValid())
        return false;

    m_startTime = false;

    // Flush input
    OMX_ERRORTYPE e;
    e = m_videoDecoder.SendCommand(OMX_CommandFlush, m_videoDecoder.Base(), nullptr, 50);
    if (e != OMX_ErrorNone)
        return false;

    // Flush output
    e = m_videoDecoder.SendCommand(OMX_CommandFlush, m_videoDecoder.Base() + 1, nullptr, 50);
    if (e != OMX_ErrorNone)
        return false;

    if (m_avctx && m_avctx->get_buffer2)
    {
        // Request new buffers when GetFrame is next called
        QMutexLocker lock(&m_settingsChangedLock);
        m_settingsChanged = true;
    }
    else
    {
        // Re-submit the empty output buffers
        FillOutputBuffers();
    }

    return true;
}

int PrivateDecoderOMX::GetFrame(AVStream *Stream, AVFrame *Frame, int *GotPicturePtr, AVPacket *Packet)
{
    if (!m_videoDecoder.IsValid())
        return -1;

    if (!Stream)
        return -1;

    // Check for a decoded frame
    int ret = GetBufferedFrame(Stream, Frame);
    if (ret < 0)
        return ret;
    if (ret > 0)
        *GotPicturePtr = 1;

    // Check for OMX_EventPortSettingsChanged notification
    AVCodecContext *avctx = gCodecMap->getCodecContext(Stream);
    if (SettingsChanged(avctx) != OMX_ErrorNone)
    {
        // PGB If there was an error, discard this packet
        *GotPicturePtr = 0;
        return -1;
    }

    // PGB If settings have changed, discard this one packet
    QMutexLocker lock(&m_settingsChangedLock);
    if (m_settingsHaveChanged)
    {
        m_settingsHaveChanged = false;
        *GotPicturePtr = 0;
        return -1;
    }
    else 
    {
        // Submit a packet for decoding
        lock.unlock();
        return (Packet && Packet->size) ? ProcessPacket(Stream, Packet) : 0;
    }
}

// Submit a packet for decoding
int PrivateDecoderOMX::ProcessPacket(AVStream *Stream, AVPacket *Packet)
{
    // Convert h264_mp4toannexb
    if (m_bitstreamFilter)
    {
        int res = av_bsf_send_packet(m_bitstreamFilter, Packet);
        if (res < 0)
        {
            av_packet_unref(Packet);
            return res;
        }

        while (!res)
            res = av_bsf_receive_packet(m_bitstreamFilter, Packet);

        if (res < 0 && (res != AVERROR(EAGAIN) && res != AVERROR_EOF))
            return res;
    }

    // size is typically 50000 - 70000 but occasionally as low as 2500
    // or as high as 360000
    uint8_t *buf = Packet->data, *free_buf = nullptr;
    int size     = Packet->size;
    int result   = Packet->size;
    while (size > 0)
    {
        if (!m_inputBuffersSem.tryAcquire(1, 100))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Ran out of OMX input buffers, see OmxInputBuffers setting.");
            result = 0;
            break;
        }
        int freebuffers = m_inputBuffersSem.available();
        if (freebuffers < 2)
            LOG(VB_PLAYBACK, LOG_WARNING, LOC + QString("Free OMX input buffers = %1, see OmxInputBuffers setting.")
                .arg(freebuffers));
        m_settingsChangedLock.lock();
        OMX_BUFFERHEADERTYPE *header = m_inputBuffers.takeFirst();
        m_settingsChangedLock.unlock();

        OMX_U32 free  = header->nAllocLen - (header->nFilledLen + header->nOffset);
        OMX_U32 count = (free > static_cast<OMX_U32>(size)) ? static_cast<OMX_U32>(size) : free;
        memcpy(&header->pBuffer[header->nOffset + header->nFilledLen], buf, count);
        header->nFilledLen += count;
        buf += count;
        size -= count;
        free -= count;

        header->nTimeStamp = Pts2Ticks(Stream, Packet->pts);
        if (!m_startTime && (Packet->flags & AV_PKT_FLAG_KEY))
        {
            m_startTime = true;
            header->nFlags = OMX_BUFFERFLAG_STARTTIME;
        }
        else
        {
            header->nFlags = 0;
        }

        LOG(VB_PLAYBACK | VB_TIMESTAMP, LOG_INFO, LOC + QString("EmptyThisBuffer len=%1 tc=%2uS (pts %3)")
            .arg(header->nFilledLen).arg(TICKS_TO_S64(header->nTimeStamp)).arg(Packet->pts) );

        OMX_ERRORTYPE err = OMX_EmptyThisBuffer(m_videoDecoder.Handle(), header);
        if (err != OMX_ErrorNone)
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC + QString("OMX_EmptyThisBuffer error %1").arg(Error2String(err)) );
            m_settingsChangedLock.lock();
            m_inputBuffers.append(header);
            m_settingsChangedLock.unlock();
            m_inputBuffersSem.release();
            result = -1;
            break;
        }
    }

    if (free_buf)
        av_freep(&free_buf);
    return result;
}

int PrivateDecoderOMX::GetBufferedFrame(AVStream *Stream, AVFrame *Picture)
{
    if (!Picture)
        return -1;

    AVCodecContext *avctx = gCodecMap->getCodecContext(Stream);
    if (!avctx)
        return -1;

    if (!m_outputBuffersSem.tryAcquire())
        return 0;

    m_settingsChangedLock.lock();
    OMX_BUFFERHEADERTYPE *header = m_outputBuffers.takeFirst();
    m_settingsChangedLock.unlock();

    OMX_U32 nFlags = header->nFlags;
    if (nFlags & ~OMX_BUFFERFLAG_ENDOFFRAME)
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("Decoded frame flags=%1").arg(HeaderFlags(nFlags)));

    if (avctx->pix_fmt < 0)
        avctx->pix_fmt = AV_PIX_FMT_YUV420P;

    int result = 1; // Assume success
    if (header->nFilledLen == 0)
    {
        // This happens after an EventBufferFlag EOS
        result = 0;
    }
    else if (avctx->get_buffer2(avctx, Picture, 0) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Decoded frame available but no video buffers");
        result = 0;
    }
    else
    {
        VideoFrame scratchframe;
        VideoFrameType frametype = FMT_YV12;
        AVPixelFormat  outputfmt = AV_PIX_FMT_YUV420P;
        AVPixelFormat  inputfmt  = AV_PIX_FMT_NONE;
        int pitches[3];
        int offsets[3];

        const OMX_VIDEO_PORTDEFINITIONTYPE &portdef = m_videoDecoder.PortDef(1).format.video;
        switch (portdef.eColorFormat)
        {
            case OMX_COLOR_FormatYUV420Planar:
            case OMX_COLOR_FormatYUV420PackedPlanar: // Broadcom default
                pitches[0] = static_cast<int>(portdef.nStride);
                pitches[1] = pitches[2] = static_cast<int>(portdef.nStride >> 1);
                offsets[0] = 0;
                offsets[1] = static_cast<int>(portdef.nStride * static_cast<OMX_S32>(portdef.nSliceHeight));
                offsets[2] = offsets[1] + (offsets[1] >> 2);
                inputfmt   = AV_PIX_FMT_YUV420P;
                break;
            case OMX_COLOR_Format16bitRGB565:
                LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Format %1 broken").arg(Format2String(portdef.eColorFormat)));
                inputfmt  = AV_PIX_FMT_RGB565LE;
                outputfmt = AV_PIX_FMT_RGBA;
                frametype = FMT_RGBA32;
                break;
            default:
                LOG(VB_GENERAL, LOG_ERR, LOC + QString("Unsupported frame type: %1")
                    .arg(Format2String(portdef.eColorFormat)) );
                break;
        }

        Picture->reordered_opaque = Ticks2Pts(Stream, header->nTimeStamp);

        VideoFrame *frame = HEADER2FRAME(header);

        if (frame && (FRAME2HEADER(frame) == header))
        {
            frame->bpp    = bitsperpixel(frametype);
            frame->codec  = frametype;
            frame->width  = static_cast<int>(portdef.nFrameWidth);
            frame->height = static_cast<int>(portdef.nSliceHeight);
            memcpy(frame->offsets, offsets, sizeof frame->offsets);
            memcpy(frame->pitches, pitches, sizeof frame->pitches);

            VideoFrame *frame2 = reinterpret_cast<VideoFrame*>(Picture->opaque);
            OMX_BUFFERHEADERTYPE *header2 = FRAME2HEADER(frame2);
            if (header2 && HEADER2FRAME(header2) == frame2)
            {
                // The new frame has an associated hdr so use that
                header = header2;
            }
            else
            {
                // Re-use the current header with the new frame
                header->pBuffer = frame2->buf;
                header->pAppPrivate = frame2;
                FRAMESETHEADER(frame2, header);
            }

            FRAMESETBUFFERREF(frame2, Picture->buf[0]);

            // Indicate the buffered frame from the completed OMX buffer
            Picture->buf[0] = FRAME2BUFFERREF(frame);
            FRAMEUNSETBUFFERREF(frame);
            Picture->opaque = frame;
        }
        else if (inputfmt == avctx->pix_fmt)
        {
            // Prepare to copy the OMX buffer
            init(&scratchframe, frametype, &header->pBuffer[header->nOffset],
                 static_cast<int>(portdef.nFrameWidth), static_cast<int>(portdef.nSliceHeight),
                 static_cast<int>(header->nFilledLen), pitches, offsets);
        }
        else if (inputfmt != AV_PIX_FMT_NONE)
        {
            int in_width   = static_cast<int>(portdef.nStride);
            int in_height  = static_cast<int>(portdef.nSliceHeight);
            int out_width  = static_cast<int>(portdef.nFrameWidth + 15) & (~0xf);
            int out_height = static_cast<int>(portdef.nSliceHeight);
            int size       = ((bitsperpixel(frametype) * out_width) / 8) * out_height;
            uint8_t* src   = &header->pBuffer[header->nOffset];

            unsigned char *buf = static_cast<unsigned char*>(av_malloc(static_cast<size_t>(size)));
            init(&scratchframe, frametype, buf, out_width, out_height, size);

            AVFrame img_in, img_out;
            av_image_fill_arrays(img_out.data, img_out.linesize, scratchframe.buf, outputfmt, out_width,out_height, IMAGE_ALIGN);
            av_image_fill_arrays(img_in.data, img_in.linesize, src, inputfmt, in_width, in_height, IMAGE_ALIGN);

            LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("Converting %1 to %2")
                .arg(av_pix_fmt_desc_get(inputfmt)->name).arg(av_pix_fmt_desc_get(outputfmt)->name));
            m_copyCtx.Copy(&img_out, outputfmt, &img_in, inputfmt, in_width, in_height);
            av_freep(&buf);
        }
        else
        {
            result = 0;
        }

        if (result)
        {
#ifdef OMX_BUFFERFLAG_INTERLACED
            if (nFlags & OMX_BUFFERFLAG_INTERLACED)
                Picture->interlaced_frame = 1;
            else
#endif
                Picture->interlaced_frame = 0;
#ifdef OMX_BUFFERFLAG_TOP_FIELD_FIRST
            if (nFlags & OMX_BUFFERFLAG_TOP_FIELD_FIRST)
                Picture->top_field_first = 1;
            else
#endif
                Picture->top_field_first = 0;
            Picture->repeat_pict = 0;
            if (!frame)
            {
                // Copy OMX buffer to the frame provided
                copy(static_cast<VideoFrame*>(Picture->opaque), &scratchframe);
            }
        }
    }

    header->nFlags = 0;
    header->nFilledLen = 0;
    OMX_ERRORTYPE err = OMX_FillThisBuffer(m_videoDecoder.Handle(), header);
    if (err != OMX_ErrorNone)
        LOG(VB_PLAYBACK, LOG_ERR, LOC + QString("OMX_FillThisBuffer error %1").arg(Error2String(err)));
    return result;
}

// Handle the port settings changed notification
OMX_ERRORTYPE PrivateDecoderOMX::SettingsChanged(AVCodecContext *AVCtx)
{
    QMutexLocker lock(&m_settingsChangedLock);

    // Check for errors notified in EventCB
    OMX_ERRORTYPE err = m_videoDecoder.LastError();
    if (err != OMX_ErrorNone)
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC + QString("SettingsChanged error %1").arg(Error2String(err)));
        return err;
    }

    if (!m_settingsChanged)
        return OMX_ErrorNone;

    lock.unlock();

    const int index = 1; // Output port index

    // Disable output port and free output buffers
    OMXComponentCB<PrivateDecoderOMX> cb(this, &PrivateDecoderOMX::FreeOutputBuffersCB);
    err = m_videoDecoder.PortDisable(index, 500, &cb);
    if (err != OMX_ErrorNone)
        return err;

    // Update the output port definition to get revised buffer no. and size
    err = m_videoDecoder.GetPortDef(index);
    if (err != OMX_ErrorNone)
        return err;

    // Log the new port definition
    m_videoDecoder.ShowPortDef(index, LOG_INFO);

    // Set output format
    // Broadcom doc says OMX_COLOR_Format16bitRGB565 also supported
    OMX_VIDEO_PARAM_PORTFORMATTYPE fmt;
    OMX_DATA_INIT(fmt);
    fmt.nPortIndex = m_videoDecoder.Base() + index;
    fmt.eCompressionFormat = OMX_VIDEO_CodingUnused;
    fmt.eColorFormat = OMX_COLOR_FormatYUV420PackedPlanar; // default
    err = m_videoDecoder.SetParameter(OMX_IndexParamVideoPortFormat, &fmt);
    if (err != OMX_ErrorNone)
        LOG(VB_PLAYBACK, LOG_ERR, LOC + QString("SetParameter IndexParamVideoPortFormat error %1").arg(Error2String(err)));

#ifdef USING_BROADCOM
    if (VERBOSE_LEVEL_CHECK(VB_PLAYBACK, LOG_INFO))
    {
        OMX_CONFIG_POINTTYPE aspect;
        err = GetAspect(aspect, index);
        if (err == OMX_ErrorNone)
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Pixel aspect x/y = %1/%2").arg(aspect.nX).arg(aspect.nY) );
    }
#endif

    ComponentCB PrivateDecoderOMX::*allocBuffers = &PrivateDecoderOMX::AllocOutputBuffersCB;
    if ((m_avctx = AVCtx) && AVCtx->get_buffer2)
    {
        // Use the video output buffers directly
        allocBuffers = &PrivateDecoderOMX::UseBuffersCB;
    }

    // Re-enable output port and allocate new output buffers
    OMXComponentCB<PrivateDecoderOMX> cb2(this, allocBuffers);
    err = m_videoDecoder.PortEnable(index, 500, &cb2);
    if (err != OMX_ErrorNone)
        return err;

    // Submit the new output buffers
    err = FillOutputBuffers();
    if (err != OMX_ErrorNone)
        return err;

    lock.relock();
    m_settingsChanged = false;
    m_settingsHaveChanged = true;
    lock.unlock();

    return OMX_ErrorNone;
}

bool PrivateDecoderOMX::HasBufferedFrames(void)
{
    QMutexLocker lock(&m_settingsChangedLock);
    return !m_outputBuffers.isEmpty();
}

#ifdef USING_BROADCOM
OMX_ERRORTYPE PrivateDecoderOMX::GetAspect(OMX_CONFIG_POINTTYPE &Point, int Index) const
{
    OMX_DATA_INIT(Point);
    Point.nPortIndex = m_videoDecoder.Base() + Index;
    OMX_ERRORTYPE err = m_videoDecoder.GetParameter(OMX_IndexParamBrcmPixelAspectRatio, &Point);
    if (err != OMX_ErrorNone)
        LOG(VB_PLAYBACK, LOG_ERR, LOC + QString("IndexParamBrcmPixelAspectRatio error %1").arg(Error2String(err)));
    return err;
}
#endif

OMX_ERRORTYPE PrivateDecoderOMX::Event(OMXComponent &Component, OMX_EVENTTYPE EventType,
                                       OMX_U32 Data1, OMX_U32 Data2, OMX_PTR EventData)
{
    switch(EventType)
    {
      case OMX_EventPortSettingsChanged:
        if (Data1 != m_videoDecoder.Base() + 1)
            break;

        if (m_settingsChangedLock.tryLock(500))
        {
            m_settingsChanged = true;
            m_settingsChangedLock.unlock();
        }
        else
        {
            LOG(VB_GENERAL, LOG_CRIT, LOC + "EventPortSettingsChanged deadlock");
        }
        return OMX_ErrorNone;

      default:
        break;
    }

    return OMXComponentCtx::Event(Component, EventType, Data1, Data2, EventData);
}

OMX_ERRORTYPE PrivateDecoderOMX::EmptyBufferDone(OMXComponent&, OMX_BUFFERHEADERTYPE *Header)
{
    if (Header)
    {
        Header->nFilledLen = 0;
        if (m_settingsChangedLock.tryLock(1000))
        {
            m_inputBuffers.append(Header);
            m_settingsChangedLock.unlock();
            m_inputBuffersSem.release();
        }
        else
            LOG(VB_GENERAL, LOG_CRIT, LOC + "EmptyBufferDone deadlock");
    }
    return OMX_ErrorNone;
}

OMX_ERRORTYPE PrivateDecoderOMX::FillBufferDone(OMXComponent&, OMX_BUFFERHEADERTYPE *Header)
{
    if (Header)
    {
        if (m_settingsChangedLock.tryLock(1000))
        {
            m_outputBuffers.append(Header);
            m_settingsChangedLock.unlock();
            m_outputBuffersSem.release();
        }
        else
            LOG(VB_GENERAL, LOG_CRIT, LOC + "FillBufferDone deadlock");
    }
    return OMX_ErrorNone;
}

void PrivateDecoderOMX::ReleaseBuffers(OMXComponent&)
{
    // Free all output buffers
    FreeOutputBuffersCB();

    // Free all input buffers
    while (m_inputBuffersSem.tryAcquire())
    {
        m_settingsChangedLock.lock();
        OMX_BUFFERHEADERTYPE *header = m_inputBuffers.takeFirst();
        m_settingsChangedLock.unlock();

        OMX_ERRORTYPE e = OMX_FreeBuffer(m_videoDecoder.Handle(), m_videoDecoder.Base(), header);
        if (e != OMX_ErrorNone)
            LOG(VB_PLAYBACK, LOG_ERR, LOC + QString("OMX_FreeBuffer 0x%1 error %2")
                .arg(quintptr(header),0,16).arg(Error2String(e)));
    }
}

#define CASE2STR(f) case f: return STR(f)
#define CASE2STR_(f) case f: return #f

static const char *H264Profile2String(int profile)
{
    switch (profile)
    {
        CASE2STR_(FF_PROFILE_H264_BASELINE);
        CASE2STR_(FF_PROFILE_H264_MAIN);
        CASE2STR_(FF_PROFILE_H264_EXTENDED);
        CASE2STR_(FF_PROFILE_H264_HIGH);
        CASE2STR_(FF_PROFILE_H264_HIGH_10);
        CASE2STR_(FF_PROFILE_H264_HIGH_10_INTRA);
        CASE2STR_(FF_PROFILE_H264_HIGH_422);
        CASE2STR_(FF_PROFILE_H264_HIGH_422_INTRA);
        CASE2STR_(FF_PROFILE_H264_HIGH_444_PREDICTIVE);
        CASE2STR_(FF_PROFILE_H264_HIGH_444_INTRA);
        CASE2STR_(FF_PROFILE_H264_CAVLC_444);
    }
    static char buf[32];
    return strcpy(buf, qPrintable(QString("Profile 0x%1").arg(profile)));
}
