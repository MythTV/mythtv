// MythTV
#include "mythlogging.h"
#include "mythmainwindow.h"
#include "avformatdecoder.h"
#include "mythnvdecinterop.h"
#include "mythhwcontext.h"
#include "mythnvdeccontext.h"

#define LOC QString("NVDEC: ")

/*! \brief Determine whether NVDEC decoding is supported for this codec.
 *
 * \todo Add proper decoder support check if possible. FFmpeg does not expose the
 * CUVID functions though.
*/
MythCodecID MythNVDECContext::GetSupportedCodec(AVCodecContext *Context,
                                                AVCodec       **Codec,
                                                const QString  &Decoder,
                                                uint            StreamType,
                                                AVPixelFormat  &PixFmt)
{
    bool decodeonly = Decoder == "nvdec-dec";
    MythCodecID success = static_cast<MythCodecID>((decodeonly ? kCodec_MPEG1_NVDEC_DEC : kCodec_MPEG1_NVDEC) + (StreamType - 1));
    MythCodecID failure = static_cast<MythCodecID>(kCodec_MPEG1 + (StreamType - 1));

    if (((Decoder != "nvdec") && (Decoder != "nvdec-dec")) || getenv("NO_NVDEC"))
        return failure;

    // NVDec only supports 420 chroma
    // N.B. High end GPUs can decode H.265 4:4:4 - which will need fixing here
    VideoFrameType type = PixelFormatToFrameType(Context->pix_fmt);
    if (format_is_420(type))
    {
        for (int i = 0; ; i++)
        {
            const AVCodecHWConfig *config = avcodec_get_hw_config(*Codec, i);
            if (!config)
                break;

            if ((config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) &&
                (config->device_type == AV_HWDEVICE_TYPE_CUDA))
            {
                QString name = QString((*Codec)->name) + "_cuvid";
                if (name == "mpeg2video_cuvid")
                    name = "mpeg2_cuvid";
                AVCodec *codec = avcodec_find_decoder_by_name(name.toLocal8Bit());
                if (codec)
                {
                    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("HW device type '%1' supports decoding '%2 %3'")
                            .arg(av_hwdevice_get_type_name(AV_HWDEVICE_TYPE_CUDA)).arg((*Codec)->name)
                            .arg(format_description(type)));
                    *Codec = codec;
                    PixFmt = config->pix_fmt;
                    return success;
                }
                break;
            }
        }
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("HW device type '%1' does not support decoding '%2 %3'")
            .arg(av_hwdevice_get_type_name(AV_HWDEVICE_TYPE_CUDA)).arg((*Codec)->name)
            .arg(format_description(type)));
    return failure;
}

int MythNVDECContext::InitialiseDecoder(AVCodecContext *Context)
{
    if (!gCoreContext->IsUIThread() || !Context)
        return -1;

    // Retrieve OpenGL render context
    MythRenderOpenGL* render = MythRenderOpenGL::GetOpenGLRender();
    if (!render)
        return -1;

    // Lock
    OpenGLLocker locker(render);

    MythCodecID codecid = static_cast<MythCodecID>(kCodec_MPEG1_NVDEC + (mpeg_version(Context->codec_id) - 1));
    MythOpenGLInterop::Type type = MythNVDECInterop::GetInteropType(codecid, render);
    if (type == MythOpenGLInterop::Unsupported)
        return -1;

    // Create interop (and CUDA context)
    MythNVDECInterop *interop = MythNVDECInterop::Create(render);
    if (!interop)
        return -1;
    if (!interop->IsValid())
    {
        interop->DecrRef();
        return -1;
    }

    // Allocate the device context
    AVBufferRef* hwdeviceref = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_CUDA);
    if (!hwdeviceref)
    {
        interop->DecrRef();
        return -1;
    }

    // Set release method, interop and CUDA context
    AVHWDeviceContext* hwdevicecontext = reinterpret_cast<AVHWDeviceContext*>(hwdeviceref->data);
    hwdevicecontext->free = &MythHWContext::DeviceContextFinished;
    hwdevicecontext->user_opaque = interop;
    AVCUDADeviceContext *devicehwctx = reinterpret_cast<AVCUDADeviceContext*>(hwdevicecontext->hwctx);
    devicehwctx->cuda_ctx = interop->GetCUDAContext();
    devicehwctx->stream = nullptr;

    if (av_hwdevice_ctx_init(hwdeviceref) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to init CUDA hw device");
        av_buffer_unref(&hwdeviceref);
        return -1;
    }

    Context->hw_device_ctx = hwdeviceref;
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Created CUDA device context");
    return 0;
}

int MythNVDECContext::HwDecoderInit(AVCodecContext *Context)
{
    return MythHWContext::InitialiseDecoder2(Context, MythNVDECContext::InitialiseDecoder,
                                             "Create NVDEC decoder");
}

enum AVPixelFormat MythNVDECContext::GetFormat(AVCodecContext* Context, const AVPixelFormat *PixFmt)
{
    while (*PixFmt != AV_PIX_FMT_NONE)
    {
        if (*PixFmt == AV_PIX_FMT_CUDA)
        {
            if (!Context->hw_device_ctx)
            {
                if (HwDecoderInit(Context) >= 0)
                    return *PixFmt;
            }
            else
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC + "Re-using CUDA context");
                return *PixFmt;
            }
            PixFmt++;
        }
    }
    return AV_PIX_FMT_NONE;
}

/*! \brief Convert AVFrame data to MythFrame
 *
 * \note The CUDA decoder returns a complete AVFrame which should represent an NV12
 * frame held in device (GPU) memory. There is no need to call avcodec_default_get_buffer2.
*/
int MythNVDECContext::GetBuffer(struct AVCodecContext *Context, AVFrame *Frame, int)
{
    if ((Frame->format != AV_PIX_FMT_CUDA) || !Frame->data[0])
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Not a valid CUDA hw frame");
        return -1;
    }

    AvFormatDecoder *avfd = static_cast<AvFormatDecoder*>(Context->opaque);
    VideoFrame *videoframe = avfd->GetPlayer()->GetNextVideoFrame();

    for (int i = 0; i < 3; i++)
    {
        videoframe->pitches[i] = Frame->linesize[i];
        videoframe->offsets[i] = Frame->data[i] ? (static_cast<int>(Frame->data[i] - Frame->data[0])) : 0;
        videoframe->priv[i] = nullptr;
    }

    // hrm - hacky
    if (videoframe->pitches[0] && videoframe->offsets[1])
        videoframe->height = videoframe->offsets[1] / videoframe->pitches[0];

    Frame->opaque           = videoframe;
    videoframe->pix_fmt     = Context->pix_fmt;
    Frame->reordered_opaque = Context->reordered_opaque;

    // set the pixel format - normally NV12 but P010 for 10bit. Set here rather than guessing later.
    if (Frame->hw_frames_ctx)
    {
        AVHWFramesContext *context = reinterpret_cast<AVHWFramesContext*>(Frame->hw_frames_ctx->data);
        if (context)
            videoframe->sw_pix_fmt = context->sw_format;
    }

    // Frame->data[0] holds CUdeviceptr for the frame data - offsets calculated above
    videoframe->buf = Frame->data[0];

    // Retain the buffer so it is not released before we display it
    videoframe->priv[0] = reinterpret_cast<unsigned char*>(av_buffer_ref(Frame->buf[0]));

    // We need the CUDA device context in the interop class and it also holds the reference
    // to the interop class itself
    videoframe->priv[1] = reinterpret_cast<unsigned char*>(av_buffer_ref(Context->hw_device_ctx));

    // Set the release method
    Frame->buf[1] = av_buffer_create(reinterpret_cast<uint8_t*>(videoframe), 0,
                                     MythHWContext::ReleaseBuffer, avfd, 0);
    return 0;
}
