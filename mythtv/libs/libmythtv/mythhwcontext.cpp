// MythTV
#include "mythmainwindow.h"
#include "avformatdecoder.h"
#include "mythopenglinterop.h"
#include "mythhwcontext.h"

// FFmpeg
extern "C" {
#include "libavutil/pixfmt.h"
#include "libavcodec/avcodec.h"
}

#define LOC QString("HWCtx: ")

/// \brief A generic hardware buffer initialisation method when using AVHWFramesContext.
int MythHWContext::GetBuffer(struct AVCodecContext *Context, AVFrame *Frame, int Flags)
{
    AvFormatDecoder *avfd = static_cast<AvFormatDecoder*>(Context->opaque);
    VideoFrame *videoframe = avfd->GetPlayer()->GetNextVideoFrame();

    // set fields required for directrendering
    for (int i = 0; i < 4; i++)
    {
        Frame->data[i]     = nullptr;
        Frame->linesize[i] = 0;
    }
    Frame->opaque           = videoframe;
    videoframe->pix_fmt     = Context->pix_fmt;
    Frame->reordered_opaque = Context->reordered_opaque;

    int ret = avcodec_default_get_buffer2(Context, Frame, Flags);
    if (ret < 0)
        return ret;

    // avcodec_default_get_buffer2 will retrieve an AVBufferRef from the pool of
    // hardware surfaces stored within AVHWFramesContext. The pointer to the surface is stored
    // in Frame->data[3]. Store this in VideoFrame::buf for the interop class to use.
    videoframe->buf = Frame->data[3];
    // Frame->buf(0) also contains a reference to the buffer. Take an additional reference to this
    // buffer to retain the surface until it has been displayed (otherwise it is
    // reused once the decoder is finished with it).
    videoframe->priv[0] = reinterpret_cast<unsigned char*>(av_buffer_ref(Frame->buf[0]));
    // frame->hw_frames_ctx contains a reference to the AVHWFramesContext. Take an additional
    // reference to ensure AVHWFramesContext is not released until we are finished with it.
    // This also contains the underlying MythOpenGLInterop class reference.
    videoframe->priv[1] = reinterpret_cast<unsigned char*>(av_buffer_ref(Frame->hw_frames_ctx));

    // Set release method
    Frame->buf[1] = av_buffer_create(reinterpret_cast<uint8_t*>(videoframe), 0,
                                     MythHWContext::ReleaseBuffer, avfd, 0);
    return ret;
}


/// \brief A generic hardware buffer initialisation method when AVHWFramesContext is NOT used.
int MythHWContext::GetBuffer2(struct AVCodecContext *Context, AVFrame *Frame, int)
{
    if (!Frame || !Context)
        return -1;

    AvFormatDecoder *avfd = static_cast<AvFormatDecoder*>(Context->opaque);
    VideoFrame *videoframe = avfd->GetPlayer()->GetNextVideoFrame();

    Frame->opaque           = videoframe;
    videoframe->pix_fmt     = Context->pix_fmt;
    Frame->reordered_opaque = Context->reordered_opaque;

    // the hardware surface is stored in Frame->data[3]
    videoframe->buf = Frame->data[3];

    // Frame->buf[0] contains the release method. Take another reference to
    // ensure the frame is not released before it is displayed.
    videoframe->priv[0] = reinterpret_cast<unsigned char*>(av_buffer_ref(Frame->buf[0]));

    // Retrieve and set the interop class
    AVHWDeviceContext *devicectx = reinterpret_cast<AVHWDeviceContext*>(Context->hw_device_ctx->data);
    videoframe->priv[1] = reinterpret_cast<unsigned char*>(devicectx->user_opaque);

    // Set release method
    Frame->buf[1] = av_buffer_create(reinterpret_cast<uint8_t*>(videoframe), 0,
                                     MythHWContext::ReleaseBuffer, avfd, 0);
    return 0;
}

/// \brief A generic hardware buffer initialisation method when the frame will be copied back to main memory.
int MythHWContext::GetBuffer3(struct AVCodecContext *Context, AVFrame *Frame, int Flags)
{
    static_cast<AvFormatDecoder*>(Context->opaque)->m_directrendering = false;
    return avcodec_default_get_buffer2(Context, Frame, Flags);
}

void MythHWContext::ReleaseBuffer(void *Opaque, uint8_t *Data)
{
    AvFormatDecoder *decoder = static_cast<AvFormatDecoder*>(Opaque);
    VideoFrame *frame = reinterpret_cast<VideoFrame*>(Data);
    if (decoder && decoder->GetPlayer())
        decoder->GetPlayer()->DeLimboFrame(frame);
}

void MythHWContext::DestroyInteropCallback(void *Wait, void *Interop, void*)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Destroy interop callback");
    QWaitCondition *wait = reinterpret_cast<QWaitCondition*>(Wait);
    MythOpenGLInterop *interop = reinterpret_cast<MythOpenGLInterop*>(Interop);
    if (interop)
        interop->DecrRef();
    if (wait)
        wait->wakeAll();
}

void MythHWContext::FramesContextFinished(AVHWFramesContext *Context)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Frames context finished");
    MythOpenGLInterop *interop = reinterpret_cast<MythOpenGLInterop*>(Context->user_opaque);
    if (interop)
        DestroyInterop(interop);
}

void MythHWContext::DeviceContextFinished(AVHWDeviceContext* Context)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Device context finished");
    MythOpenGLInterop *interop = reinterpret_cast<MythOpenGLInterop*>(Context->user_opaque);
    if (interop)
        DestroyInterop(interop);
}

void MythHWContext::DestroyInterop(MythOpenGLInterop *Interop)
{
    if (gCoreContext->IsUIThread())
        Interop->DecrRef();
    else
        MythMainWindow::HandleCallback("destroy OpenGL interop", MythHWContext::DestroyInteropCallback, Interop, nullptr);
}

void MythHWContext::CreateDecoderCallback(void *Wait, void *Context, void *Callback)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Create decoder callback");
    QWaitCondition *wait     = reinterpret_cast<QWaitCondition*>(Wait);
    AVCodecContext *context  = reinterpret_cast<AVCodecContext*>(Context);
    CreateHWDecoder callback = reinterpret_cast<CreateHWDecoder>(Callback);
    if (context && callback)
        (void)callback(context);
    if (wait)
        wait->wakeAll();
}

/// \brief Initialise a hardware decoder that is expected to use AVHWFramesContext
int MythHWContext::InitialiseDecoder(AVCodecContext *Context, CreateHWDecoder Callback,
                                     const QString &Debug)
{
    if (!Context || !Callback)
        return -1;
    if (gCoreContext->IsUIThread())
        return Callback(Context);
    MythMainWindow::HandleCallback(Debug, MythHWContext::CreateDecoderCallback,
                                   Context, reinterpret_cast<void*>(Callback));
    return Context->hw_frames_ctx ? 0 : -1;
}

/// \brief Initialise a hardware decoder that is NOT expected to use AVHWFramesContext
int MythHWContext::InitialiseDecoder2(AVCodecContext *Context, CreateHWDecoder Callback,
                                      const QString &Debug)
{
    if (!Context || !Callback)
        return -1;
    if (gCoreContext->IsUIThread())
        return Callback(Context);
    MythMainWindow::HandleCallback(Debug, MythHWContext::CreateDecoderCallback,
                                   Context, reinterpret_cast<void*>(Callback));
    return Context->hw_device_ctx ? 0 : -1;
}

AVBufferRef* MythHWContext::CreateDevice(AVHWDeviceType Type, const QString &Device)
{
    AVBufferRef* result = nullptr;
    int res = av_hwdevice_ctx_create(&result, Type, Device.isEmpty() ? nullptr :
                                     Device.toLocal8Bit().constData(), nullptr, 0);
    if (res == 0)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Created hardware device '%1'%2")
            .arg(av_hwdevice_get_type_name(Type))
            .arg(Device == nullptr ? "" : QString(" (%1)").arg(Device)));
        return result;
    }

    char error[AV_ERROR_MAX_STRING_SIZE];
    LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to create hardware device '%1'%2 Error '%3'")
        .arg(av_hwdevice_get_type_name(Type))
        .arg(Device == nullptr ? "" : QString(" (%1)").arg(Device))
        .arg(av_make_error_string(error, sizeof(error), res)));
    return nullptr;
}
