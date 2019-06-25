//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-19 MythTV Developers <mythtv-dev@mythtv.org>
//
// This is part of MythTV (https://www.mythtv.org)
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#include "mythcorecontext.h"
#include "mythlogging.h"
#include "mythmainwindow.h"
#include "mythopenglinterop.h"
#include "avformatdecoder.h"

#ifdef USING_VAAPI
#include "mythvaapicontext.h"
#endif
#ifdef USING_VDPAU
#include "mythvdpaucontext.h"
#endif
#ifdef USING_NVDEC
#include "mythnvdeccontext.h"
#endif
#ifdef USING_VTB
#include "mythvtbcontext.h"
#endif
#ifdef USING_MEDIACODEC
#include "mythmediacodeccontext.h"
#endif
#include "mythcodeccontext.h"

#define LOC QString("MythCodecContext: ")

MythCodecContext::MythCodecContext(MythCodecID CodecID)
  : m_codecID(CodecID)
{
}

// static
MythCodecContext *MythCodecContext::CreateContext(MythCodecID Codec)
{
    MythCodecContext *mctx = nullptr;
#ifdef USING_VAAPI
    if (codec_is_vaapi(Codec) || codec_is_vaapi_dec(Codec))
        mctx = new MythVAAPIContext(Codec);
#endif
#ifdef USING_VDPAU
    if (codec_is_vdpau_hw(Codec) || codec_is_vdpau_hw(Codec))
        mctx = new MythVDPAUContext(Codec);
#endif
#ifdef USING_NVDEC
    if (codec_is_nvdec_dec(Codec) || codec_is_nvdec(Codec))
        mctx = new MythNVDECContext(Codec);
#endif
#ifdef USING_VTB
    if (codec_is_vtb_dec(Codec) || codec_is_vtb(Codec))
        mctx = new MythVTBContext(Codec);
#endif
#ifdef USING_MEDIACODEC
    if (codec_is_mediacodec(Codec) || codec_is_mediacodec_dec(Codec))
        mctx = new MythMediaCodecContext(Codec);
#endif
    Q_UNUSED(Codec);

    if (!mctx)
        mctx = new MythCodecContext(Codec);
    return mctx;
}

/// \brief A generic hardware buffer initialisation method when using AVHWFramesContext.
int MythCodecContext::GetBuffer(struct AVCodecContext *Context, AVFrame *Frame, int Flags)
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

    // set the underlying pixel format. Set here rather than guessing later.
    if (Frame->hw_frames_ctx)
    {
        AVHWFramesContext *context = reinterpret_cast<AVHWFramesContext*>(Frame->hw_frames_ctx->data);
        if (context)
            videoframe->sw_pix_fmt = context->sw_format;
    }

    // VAAPI 'fixes' 10/12/16bit colour values. Irrelevant for VDPAU.
    videoframe->colorshifted = 1;

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
                                     MythCodecContext::ReleaseBuffer, avfd, 0);
    return ret;
}


/// \brief A generic hardware buffer initialisation method when AVHWFramesContext is NOT used.
int MythCodecContext::GetBuffer2(struct AVCodecContext *Context, AVFrame *Frame, int)
{
    if (!Frame || !Context)
        return -1;

    AvFormatDecoder *avfd = static_cast<AvFormatDecoder*>(Context->opaque);
    VideoFrame *videoframe = avfd->GetPlayer()->GetNextVideoFrame();

    Frame->opaque           = videoframe;
    videoframe->pix_fmt     = Context->pix_fmt;
    Frame->reordered_opaque = Context->reordered_opaque;

    // N.B. this is untested (VideoToolBox and MediaCodec)
    videoframe->colorshifted = 1;

    // retrieve the software format
    if (Frame->hw_frames_ctx)
    {
        AVHWFramesContext *context = reinterpret_cast<AVHWFramesContext*>(Frame->hw_frames_ctx->data);
        if (context)
            videoframe->sw_pix_fmt = context->sw_format;
    }

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
                                     MythCodecContext::ReleaseBuffer, avfd, 0);
    return 0;
}

void MythCodecContext::ReleaseBuffer(void *Opaque, uint8_t *Data)
{
    AvFormatDecoder *decoder = static_cast<AvFormatDecoder*>(Opaque);
    VideoFrame *frame = reinterpret_cast<VideoFrame*>(Data);
    if (decoder && decoder->GetPlayer())
        decoder->GetPlayer()->DeLimboFrame(frame);
}

void MythCodecContext::DestroyInteropCallback(void *Wait, void *Interop, void*)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Destroy interop callback");
    QWaitCondition *wait = reinterpret_cast<QWaitCondition*>(Wait);
    MythOpenGLInterop *interop = reinterpret_cast<MythOpenGLInterop*>(Interop);
    if (interop)
        interop->DecrRef();
    if (wait)
        wait->wakeAll();
}

void MythCodecContext::FramesContextFinished(AVHWFramesContext *Context)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("%1 frames context finished")
        .arg(av_hwdevice_get_type_name(Context->device_ctx->type)));
    MythOpenGLInterop *interop = reinterpret_cast<MythOpenGLInterop*>(Context->user_opaque);
    if (interop)
        DestroyInterop(interop);
}

void MythCodecContext::DeviceContextFinished(AVHWDeviceContext* Context)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("%1 device context finished")
        .arg(av_hwdevice_get_type_name(Context->type)));
    MythOpenGLInterop *interop = reinterpret_cast<MythOpenGLInterop*>(Context->user_opaque);
    if (interop)
        DestroyInterop(interop);
}

void MythCodecContext::DestroyInterop(MythOpenGLInterop *Interop)
{
    if (gCoreContext->IsUIThread())
        Interop->DecrRef();
    else
        MythMainWindow::HandleCallback("destroy OpenGL interop", MythCodecContext::DestroyInteropCallback, Interop, nullptr);
}

void MythCodecContext::CreateDecoderCallback(void *Wait, void *Context, void *Callback)
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
int MythCodecContext::InitialiseDecoder(AVCodecContext *Context, CreateHWDecoder Callback,
                                     const QString &Debug)
{
    if (!Context || !Callback)
        return -1;
    if (gCoreContext->IsUIThread())
        return Callback(Context);
    MythMainWindow::HandleCallback(Debug, MythCodecContext::CreateDecoderCallback,
                                   Context, reinterpret_cast<void*>(Callback));
    return Context->hw_frames_ctx ? 0 : -1;
}

/// \brief Initialise a hardware decoder that is NOT expected to use AVHWFramesContext
int MythCodecContext::InitialiseDecoder2(AVCodecContext *Context, CreateHWDecoder Callback,
                                      const QString &Debug)
{
    if (!Context || !Callback)
        return -1;
    if (gCoreContext->IsUIThread())
        return Callback(Context);
    MythMainWindow::HandleCallback(Debug, MythCodecContext::CreateDecoderCallback,
                                   Context, reinterpret_cast<void*>(Callback));
    return Context->hw_device_ctx ? 0 : -1;
}

AVBufferRef* MythCodecContext::CreateDevice(AVHWDeviceType Type, const QString &Device)
{
    AVBufferRef* result = nullptr;
    int res = av_hwdevice_ctx_create(&result, Type, Device.isEmpty() ? nullptr :
                                     Device.toLocal8Bit().constData(), nullptr, 0);
    if (res == 0)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Created hardware device '%1'%2")
            .arg(av_hwdevice_get_type_name(Type))
            .arg(Device == nullptr ? "" : QString(" (%1)").arg(Device)));
        AVHWDeviceContext *context = reinterpret_cast<AVHWDeviceContext*>(result->data);
        context->free = MythCodecContext::DeviceContextFinished;
        context->user_opaque = nullptr;
        return result;
    }

    char error[AV_ERROR_MAX_STRING_SIZE];
    LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to create hardware device '%1'%2 Error '%3'")
        .arg(av_hwdevice_get_type_name(Type))
        .arg(Device == nullptr ? "" : QString(" (%1)").arg(Device))
        .arg(av_make_error_string(error, sizeof(error), res)));
    return nullptr;
}

/*! \brief Retrieve and process/filter AVFrame
 * \note This default implementation implements no processing/filtering
*/
int MythCodecContext::FilteredReceiveFrame(AVCodecContext *Context, AVFrame *Frame)
{
    return avcodec_receive_frame(Context, Frame);
}
