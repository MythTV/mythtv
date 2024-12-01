// MythTV
#include "avformatdecoder.h"
#include "mythplayerui.h"
#include "mythdrmprimecontext.h"

#define LOC QString("DRMPRIMECtx: ")

QRecursiveMutex MythDRMPRIMEContext::s_drmPrimeLock;

QStringList MythDRMPRIMEContext::s_drmPrimeDecoders;

/*! \class MythDRMPRIMEContext
 *  \brief A generic context handler for codecs that return AV_PIX_FMT_DRM_PRIME frames.
 *
 * \note There is currently no support for codecs that use AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX
 * or AV_CODEC_HW_CONFIG_METHOD_HW_FRAMES_CTX. They will need additional handling
 * if they are created (all current DRM PRIME codecs use AV_CODEC_HW_CONFIG_METHOD_INTERNAL).
 *
 * \note This will currently only pick up the Rockchip/rkmpp decoder on supported
 * platforms. v4l2m2m codecs are filtered out and handled separately as we can
 * detect what actual support is available and handle software based formats as well
 * in the subclass (although the DRM PRIME version of the v4l2m2m codec works fine purely
 * with this class).
 * \note The Rockchip/mpp library has support for codec checks via mpp_check_support_format,
 * so it should be easy to add actual decoder check support for these devices.
 *
 * \todo When Rockchip support is complete, refactor this to a pure virtual class
 * and create appropriate sub-classes as needed for the specific codecs.
*/
MythDRMPRIMEContext::MythDRMPRIMEContext(DecoderBase *Parent, MythCodecID CodecID)
  : MythCodecContext(Parent, CodecID)
{
}

MythDRMPRIMEContext::~MythDRMPRIMEContext()
{
    if (m_interop)
        DestroyInterop(m_interop);
}

MythCodecID MythDRMPRIMEContext::GetPrimeCodec(AVCodecContext **Context,
                                               const AVCodec  **Codec,
                                               AVStream        *Stream,
                                               MythCodecID      Successs,
                                               MythCodecID      Failure,
                                               const QString   &CodecName,
                                               AVPixelFormat    Format)
{
    // Find decoder
    QString name = QString((*Codec)->name) + "_" + CodecName;
    if (name.startsWith("mpeg2video"))
        name = "mpeg2_" + CodecName;
    const AVCodec *codec = avcodec_find_decoder_by_name(name.toLocal8Bit());
    auto *decoder = dynamic_cast<AvFormatDecoder*>(reinterpret_cast<DecoderBase*>((*Context)->opaque));
    if (!codec || !decoder)
    {
        // this shouldn't happen!
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to find %1").arg(name));
        return Failure;
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Found FFmpeg decoder '%1'").arg(name));
    *Codec = codec;
    decoder->CodecMap()->FreeCodecContext(Stream);
    *Context = decoder->CodecMap()->GetCodecContext(Stream, *Codec);
    (*Context)->pix_fmt = Format;
    return Successs;
}

MythCodecID MythDRMPRIMEContext::GetSupportedCodec(AVCodecContext **Context,
                                                   const AVCodec **Codec,
                                                   const QString &Decoder,
                                                   AVStream *Stream,
                                                   uint StreamType)
{
    auto success = static_cast<MythCodecID>(kCodec_MPEG1_DRMPRIME + (StreamType - 1));
    auto failure = static_cast<MythCodecID>(kCodec_MPEG1 + (StreamType - 1));

    // not us
    if (Decoder != "drmprime")
        return failure;

    // Direct rendering needs interop support
    if (!FrameTypeIsSupported(*Context, FMT_DRMPRIME))
        return failure;

    // check support
    if (!HavePrimeDecoders((*Codec)->id))
        return failure;

    QString codecname;
    {
        QMutexLocker locker(&s_drmPrimeLock);
        // just take the first - there should only ever be one
        if (!s_drmPrimeDecoders.isEmpty())
            codecname = s_drmPrimeDecoders.first();
    }
    if (codecname.isEmpty())
        return failure;

    // and get the codec
    return MythDRMPRIMEContext::GetPrimeCodec(Context, Codec, Stream, success,
                                              failure, codecname, AV_PIX_FMT_DRM_PRIME);
}

int MythDRMPRIMEContext::HwDecoderInit(AVCodecContext *Context)
{
    if (!Context)
        return -1;
    if (Context->pix_fmt != AV_PIX_FMT_DRM_PRIME)
        return -1;

#ifdef USING_EGL
    if (auto * player = GetPlayerUI(Context); player != nullptr)
        if (FrameTypeIsSupported(Context, FMT_DRMPRIME))
            m_interop = MythDRMPRIMEInterop::CreateDRM(dynamic_cast<MythRenderOpenGL*>(player->GetRender()), player);
#endif
    return m_interop ? 0 : -1;
}

void MythDRMPRIMEContext::InitVideoCodec(AVCodecContext *Context, bool SelectedStream, bool &DirectRendering)
{
    if (Context->pix_fmt == AV_PIX_FMT_DRM_PRIME)
    {
        DirectRendering = false;
        Context->get_format = MythDRMPRIMEContext::GetFormat;
        return;
    }

    MythCodecContext::InitVideoCodec(Context, SelectedStream, DirectRendering);
}

bool MythDRMPRIMEContext::DecoderWillResetOnFlush(void)
{
    return true;
}

bool MythDRMPRIMEContext::RetrieveFrame(AVCodecContext *Context, MythVideoFrame *Frame, AVFrame *AvFrame)
{
    // Hrm - Context doesn't have the correct pix_fmt here (v4l2 at least). Bug? Use AvFrame
    if (AvFrame->format == AV_PIX_FMT_DRM_PRIME)
        return GetDRMBuffer(Context, Frame, AvFrame, 0);
    return false;
}

AVPixelFormat MythDRMPRIMEContext::GetFormat(AVCodecContext */*Context*/, const AVPixelFormat *PixFmt)
{
    while (*PixFmt != AV_PIX_FMT_NONE)
    {
        if (*PixFmt == AV_PIX_FMT_DRM_PRIME)
            return AV_PIX_FMT_DRM_PRIME;
        PixFmt++;
    }
    return AV_PIX_FMT_NONE;
}

bool MythDRMPRIMEContext::GetDRMBuffer(AVCodecContext *Context, MythVideoFrame *Frame, AVFrame *AvFrame, int /*unused*/)
{
    if (!Context || !AvFrame || !Frame)
        return false;

    if (Frame->m_type != FMT_DRMPRIME || static_cast<AVPixelFormat>(AvFrame->format) != AV_PIX_FMT_DRM_PRIME)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Not a DRM PRIME buffer");
        return false;
    }

    Frame->m_width = AvFrame->width;
    Frame->m_height = AvFrame->height;
    Frame->m_pixFmt = Context->pix_fmt;
    Frame->m_swPixFmt = Context->sw_pix_fmt;
    Frame->m_directRendering = true;
    AvFrame->opaque = Frame;

    // Frame->data[0] holds AVDRMFrameDescriptor
    Frame->m_buffer = AvFrame->data[0];
    // Retain the buffer so it is not released before we display it
    Frame->m_priv[0] = reinterpret_cast<unsigned char*>(av_buffer_ref(AvFrame->buf[0]));
    // Add interop
    Frame->m_priv[1] = reinterpret_cast<unsigned char*>(m_interop);
    // Set the release method
    AvFrame->buf[1] = av_buffer_create(reinterpret_cast<uint8_t*>(Frame), 0, MythCodecContext::ReleaseBuffer,
                                       static_cast<AvFormatDecoder*>(Context->opaque), 0);
    return true;
}

bool MythDRMPRIMEContext::HavePrimeDecoders(bool Reinit /*=false*/, AVCodecID Codec /*=AV_CODEC_ID_NONE*/)
{
    static bool s_needscheck = true;
    static QVector<AVCodecID> s_supportedCodecs;

    QMutexLocker locker(&s_drmPrimeLock);

    if (s_needscheck || Reinit)
    {
        s_needscheck = false;
        s_supportedCodecs.clear();
        s_drmPrimeDecoders.clear();

        const AVCodec* codec = nullptr;
        void* opaque = nullptr;
        QStringList debugcodecs;

        while ((codec = av_codec_iterate(&opaque)))
        {
            if (!av_codec_is_decoder(codec))
                continue;

            // Filter out v4l2_m2m decoders - which have their own implementation
            if (QString(codec->name).contains("v4l2m2m"))
                continue;

            const AVCodecHWConfig* config = nullptr;
            for (int i = 0; (config = avcodec_get_hw_config(codec, i)); ++i)
            {
                if (config->pix_fmt != AV_PIX_FMT_DRM_PRIME)
                    continue;

                // N.B. no support yet for DRMPRIME codecs using frames and/or
                // device contexts. None exist yet:)
                if (config->methods & AV_CODEC_HW_CONFIG_METHOD_INTERNAL)
                {
                    QStringList name = QString(codec->name).split("_", Qt::SkipEmptyParts);
                    if (name.size() > 1 && !s_drmPrimeDecoders.contains(name[1]))
                        s_drmPrimeDecoders.append(name[1]);
                    if (!s_supportedCodecs.contains(codec->id))
                    {
                        s_supportedCodecs.append(codec->id);
                        debugcodecs.append(avcodec_get_name(codec->id));
                    }
                }
            }
        }

        if (debugcodecs.isEmpty())
            debugcodecs.append("None");
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("DRM PRIME codecs supported: %1 %2")
            .arg(debugcodecs.join(","),
                 s_drmPrimeDecoders.isEmpty() ? "" : QString("using: %1").arg(s_drmPrimeDecoders.join(","))));
    }

    if (!Codec)
        return !s_supportedCodecs.isEmpty();
    return s_supportedCodecs.contains(Codec);
}
