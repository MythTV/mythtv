// MythTV
#include "decoders/avformatdecoder.h"
#include "mythmmalcontext.h"

// Broadcom
extern "C" {
#include "interface/vmcs_host/vc_vchi_gencmd.h"
}

// FFmpeg
extern "C" {
#include "libavutil/opt.h"
}

#define LOC QString("MMAL: ")

MythMMALContext::MythMMALContext(DecoderBase *Parent, MythCodecID Codec)
  : MythCodecContext(Parent, Codec)
{
}

MythMMALContext::~MythMMALContext()
{
    if (m_interop)
        m_interop->DecrRef();
}

MythCodecID MythMMALContext::GetSupportedCodec(AVCodecContext **Context,
                                               AVCodec **Codec,
                                               const QString &Decoder,
                                               AVStream *Stream,
                                               uint StreamType)
{
    bool decodeonly = Decoder == "mmal-dec";
    MythCodecID success = static_cast<MythCodecID>((decodeonly ? kCodec_MPEG1_MMAL_DEC : kCodec_MPEG1_MMAL) + (StreamType - 1));
    MythCodecID failure = static_cast<MythCodecID>(kCodec_MPEG1 + (StreamType - 1));

    if (!Decoder.startsWith("mmal"))
        return failure;

    // Only MPEG2, MPEG4, VC1 and H264 supported (and HEVC will never be supported)
    QString codecstr;
    switch ((*Codec)->id)
    {
        case AV_CODEC_ID_MPEG2VIDEO: codecstr = "MPG2"; break;
        case AV_CODEC_ID_MPEG4: codecstr = "MPG4"; break;
        case AV_CODEC_ID_VC1: codecstr = "WVC1"; break;
        case AV_CODEC_ID_H264: codecstr = "H264"; break;
        default: break;
    }

    if (codecstr.isEmpty())
        return failure;

    // check actual decoder support
    vcos_init();
    VCHI_INSTANCE_T vchi_instance;
    if (vchi_initialise(&vchi_instance) != 0)
        return failure;
    if (vchi_connect(nullptr, 0, vchi_instance) != 0)
        return failure;
    VCHI_CONNECTION_T *vchi_connection = nullptr;
    vc_vchi_gencmd_init(vchi_instance, &vchi_connection, 1 );

    bool found = false;
    char command[32];
    char* response = nullptr;
    int responsesize = 0;
    QString msg = QString("codec_enabled %1").arg(codecstr);
    if (!vc_gencmd(command, sizeof(command), msg.toLocal8Bit().constData()))
        vc_gencmd_string_property(command, codecstr.toLocal8Bit().constData(), &response, &responsesize);
 
    if (!response || responsesize < 1)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to query codec support");
    }
    else
    {
        if (qstrcmp(response, "enabled") != 0)
            LOG(VB_GENERAL, LOG_INFO, LOC +QString("Codec '%1' not supported (no license?)")
                .arg(avcodec_get_name((*Codec)->id)));
        else
            found = true;
    }

    vc_gencmd_stop();
    vchi_disconnect(vchi_instance);

    if (!found)
        return failure;

    if (!decodeonly)
    {
        // If called from outside of the main thread, we need a MythPlayer instance to
        // process the callback interop check callback - which may fail otherwise
        MythPlayer* player = nullptr;
        if (!gCoreContext->IsUIThread())
        {
            AvFormatDecoder* decoder = reinterpret_cast<AvFormatDecoder*>((*Context)->opaque);
            if (decoder)
                player = decoder->GetPlayer();
        }

        // direct rendering needs interop support
        if (MythOpenGLInterop::GetInteropType(FMT_MMAL, player) == MythOpenGLInterop::Unsupported)
            return failure;
    }

    // look for a decoder
    QString name = QString((*Codec)->name) + "_mmal";
    if (name == "mpeg2video_mmal")
        name = "mpeg2_mmal";
    AVCodec *codec = avcodec_find_decoder_by_name(name.toLocal8Bit());
    if (!codec)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to find %1").arg(name));
        return failure;
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Found MMAL/FFmpeg decoder '%1'").arg(name));
    *Codec = codec;    
    gCodecMap->freeCodecContext(Stream);
    *Context = gCodecMap->getCodecContext(Stream, *Codec);
    (*Context)->pix_fmt = decodeonly ? (*Context)->pix_fmt : AV_PIX_FMT_MMAL;
    return success;
}

void MythMMALContext::InitVideoCodec(AVCodecContext *Context, bool SelectedStream, bool &DirectRendering)
{
    if (codec_is_mmal_dec(m_codecID))
    {
        DirectRendering = false;
        return;
    }
    else if (codec_is_mmal(m_codecID))
    {
        Context->get_format  = MythMMALContext::GetFormat;
        DirectRendering = false;
        return;
    }

    MythCodecContext::InitVideoCodec(Context, SelectedStream, DirectRendering);
}

bool MythMMALContext::RetrieveFrame(AVCodecContext *Context, VideoFrame *Frame, AVFrame *AvFrame)
{
    if (codec_is_mmal_dec(m_codecID))
        return GetBuffer(Context, Frame, AvFrame, 0);
    else if (codec_is_mmal(m_codecID))
        return GetBuffer2(Context, Frame, AvFrame, 0);
    return false;
}


int MythMMALContext::HwDecoderInit(AVCodecContext *Context)
{
    if (!Context)
        return -1;

    if (codec_is_mmal_dec(m_codecID))
        return 0;

    if (!codec_is_mmal(m_codecID) || Context->pix_fmt != AV_PIX_FMT_MMAL)
        return -1;

    MythRenderOpenGL *context = MythRenderOpenGL::GetOpenGLRender();
    m_interop = MythMMALInterop::Create(context, MythOpenGLInterop::MMAL);
    return m_interop ? 0 : -1;
}

void MythMMALContext::SetDecoderOptions(AVCodecContext *Context, AVCodec *Codec)
{
    if (!(codec_is_mmal(m_codecID)))
        return;
    if (!(Context && Codec))
        return;
    if (!(Codec->priv_class && Context->priv_data))
        return;
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Setting number of extra buffers to 8");
    av_opt_set(Context->priv_data, "extra_buffers", "8", 0);
}

bool MythMMALContext::GetBuffer(AVCodecContext *Context, VideoFrame *Frame, AVFrame *AvFrame, int)
{
    // Sanity checks
    if (!Context || !AvFrame || !Frame)
        return false;

    // Ensure we can render this format
    AvFormatDecoder *decoder = static_cast<AvFormatDecoder*>(Context->opaque);
    VideoFrameType type = PixelFormatToFrameType(static_cast<AVPixelFormat>(AvFrame->format));
    VideoFrameType* supported = decoder->GetPlayer()->DirectRenderFormats();
    bool found = false;
    while (*supported != FMT_NONE)
    {
        if (*supported == type)
        {
            found = true;
            break;
        }
        supported++;
    }

    // No fallback currently (unlikely)
    if (!found)
        return false;

    // Re-allocate if necessary
    if ((Frame->codec != type) || (Frame->width != AvFrame->width) || (Frame->height != AvFrame->height))
        if (!VideoBuffers::ReinitBuffer(Frame, type, decoder->GetVideoCodecID(), AvFrame->width, AvFrame->height))
            return false;

    // Copy data
    uint count = planes(Frame->codec);
    for (uint plane = 0; plane < count; ++plane)
        copyplane(Frame->buf + Frame->offsets[plane], Frame->pitches[plane], AvFrame->data[plane], AvFrame->linesize[plane],
                  pitch_for_plane(Frame->codec, AvFrame->width, plane), height_for_plane(Frame->codec, AvFrame->height, plane));

    AvFrame->reordered_opaque = Context->reordered_opaque;
    return true;
}

bool MythMMALContext::GetBuffer2(AVCodecContext *Context, VideoFrame *Frame, AVFrame *AvFrame, int)
{
    // Sanity checks
    if (!Context || !AvFrame || !Frame || !m_interop)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Error");
        return false;
    }

    // MMAL?
    if (Frame->codec != FMT_MMAL || static_cast<AVPixelFormat>(AvFrame->format) != AV_PIX_FMT_MMAL)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Not an MMAL frame");
        return false;
    }

    Frame->width = AvFrame->width;
    Frame->height = AvFrame->height;
    Frame->pix_fmt = Context->pix_fmt;
    Frame->sw_pix_fmt = Context->sw_pix_fmt;
    Frame->directrendering = 1;
    AvFrame->opaque = Frame;
    AvFrame->reordered_opaque = Context->reordered_opaque;

    // Frame->data[3] holds MMAL_BUFFER_HEADER_T
    Frame->buf = AvFrame->data[3];
    // Retain the buffer so it is not released before we display it
    Frame->priv[0] = reinterpret_cast<unsigned char*>(av_buffer_ref(AvFrame->buf[0]));
    // Add interop
    Frame->priv[1] = reinterpret_cast<unsigned char*>(m_interop);
    // Set the release method
    AvFrame->buf[1] = av_buffer_create(reinterpret_cast<uint8_t*>(Frame), 0, MythCodecContext::ReleaseBuffer,
                                       static_cast<AvFormatDecoder*>(Context->opaque), 0);
    return true;
}

AVPixelFormat MythMMALContext::GetFormat(AVCodecContext*, const AVPixelFormat *PixFmt)
{
    while (*PixFmt != AV_PIX_FMT_NONE)
    {
        if (*PixFmt == AV_PIX_FMT_MMAL)
            return AV_PIX_FMT_MMAL;
        PixFmt++;
    }
    return AV_PIX_FMT_NONE;
}
