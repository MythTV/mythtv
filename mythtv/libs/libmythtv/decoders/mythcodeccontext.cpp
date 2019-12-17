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
#include "mythvdpauhelper.h"
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
#ifdef USING_V4L2
#include "mythv4l2m2mcontext.h"
#endif
#ifdef USING_MMAL
#include "mythmmalcontext.h"
#endif
#ifdef USING_EGL
#include "mythdrmprimecontext.h"
#endif
#include "mythcodeccontext.h"

extern "C" {
#include "libavutil/pixdesc.h"
}

#define LOC QString("MythCodecContext: ")

QAtomicInt MythCodecContext::s_hwFramesContextCount(0);

MythCodecContext::MythCodecContext(DecoderBase *Parent, MythCodecID CodecID)
  : m_parent(Parent),
    m_codecID(CodecID)
{
}

MythCodecContext *MythCodecContext::CreateContext(DecoderBase *Parent, MythCodecID Codec)
{
    MythCodecContext *mctx = nullptr;
#ifdef USING_VAAPI
    if (codec_is_vaapi(Codec) || codec_is_vaapi_dec(Codec))
        mctx = new MythVAAPIContext(Parent, Codec);
#endif
#ifdef USING_VDPAU
    if (codec_is_vdpau_hw(Codec) || codec_is_vdpau_dechw(Codec))
        mctx = new MythVDPAUContext(Parent, Codec);
#endif
#ifdef USING_NVDEC
    if (codec_is_nvdec_dec(Codec) || codec_is_nvdec(Codec))
        mctx = new MythNVDECContext(Parent, Codec);
#endif
#ifdef USING_VTB
    if (codec_is_vtb_dec(Codec) || codec_is_vtb(Codec))
        mctx = new MythVTBContext(Parent, Codec);
#endif
#ifdef USING_MEDIACODEC
    if (codec_is_mediacodec(Codec) || codec_is_mediacodec_dec(Codec))
        mctx = new MythMediaCodecContext(Parent, Codec);
#endif
#ifdef USING_V4L2
    if (codec_is_v4l2_dec(Codec) || codec_is_v4l2(Codec))
        mctx = new MythV4L2M2MContext(Parent, Codec);
#endif
#ifdef USING_MMAL
    if (codec_is_mmal_dec(Codec) || codec_is_mmal(Codec))
        mctx = new MythMMALContext(Parent, Codec);
#endif
#ifdef USING_EGL
    if (codec_is_drmprime(Codec))
        mctx = new MythDRMPRIMEContext(Parent, Codec);
#endif
    Q_UNUSED(Codec);

    if (!mctx)
        mctx = new MythCodecContext(Parent, Codec);
    return mctx;
}

void MythCodecContext::GetDecoders(RenderOptions &Opts)
{
#ifdef USING_VDPAU
    // Only enable VDPAU support if it is actually present
    if (MythVDPAUHelper::HaveVDPAU())
    {
        Opts.decoders->append("vdpau");
        (*Opts.equiv_decoders)["vdpau"].append("dummy");
        Opts.decoders->append("vdpau-dec");
        (*Opts.equiv_decoders)["vdpau-dec"].append("dummy");
    }
#endif
#ifdef USING_DXVA2
    Opts.decoders->append("dxva2");
    (*Opts.equiv_decoders)["dxva2"].append("dummy");
#endif

#ifdef USING_VAAPI
    // Only enable VAAPI if it is actually present and isn't actually VDPAU
    if (MythVAAPIContext::HaveVAAPI())
    {
        Opts.decoders->append("vaapi");
        (*Opts.equiv_decoders)["vaapi"].append("dummy");
        Opts.decoders->append("vaapi-dec");
        (*Opts.equiv_decoders)["vaapi-dec"].append("dummy");
    }
#endif
#ifdef USING_NVDEC
    // Only enable NVDec support if it is actually present
    if (MythNVDECContext::HaveNVDEC())
    {
        Opts.decoders->append("nvdec");
        (*Opts.equiv_decoders)["nvdec"].append("dummy");
        Opts.decoders->append("nvdec-dec");
        (*Opts.equiv_decoders)["nvdec-dec"].append("dummy");
    }
#endif
#ifdef USING_MEDIACODEC
    Opts.decoders->append("mediacodec");
    (*Opts.equiv_decoders)["mediacodec"].append("dummy");
    Opts.decoders->append("mediacodec-dec");
    (*Opts.equiv_decoders)["mediacodec-dec"].append("dummy");
#endif
#ifdef USING_VTB
    Opts.decoders->append("vtb");
    Opts.decoders->append("vtb-dec");
    (*Opts.equiv_decoders)["vtb"].append("dummy");
    (*Opts.equiv_decoders)["vtb-dec"].append("dummy");
#endif
#ifdef USING_V4L2
    if (MythV4L2M2MContext::HaveV4L2Codecs())
    {
#ifdef USING_V4L2PRIME
        Opts.decoders->append("v4l2");
        (*Opts.equiv_decoders)["v4l2"].append("dummy");
#endif
        Opts.decoders->append("v4l2-dec");
        (*Opts.equiv_decoders)["v4l2-dec"].append("dummy");
    }
#endif
#ifdef USING_EGL
    if (MythDRMPRIMEContext::HavePrimeDecoders())
    {
        Opts.decoders->append("drmprime");
        (*Opts.equiv_decoders)["drmprime"].append("dummy");
    }
#endif
#ifdef USING_MMAL
    Opts.decoders->append("mmal-dec");
    (*Opts.equiv_decoders)["mmal-dec"].append("dummy");
    if (MythOpenGLInterop::GetInteropType(FMT_MMAL, nullptr) != MythOpenGLInterop::Unsupported)
    {
        Opts.decoders->append("mmal");
        (*Opts.equiv_decoders)["mmal"].append("dummy");
    }
#endif
}

MythCodecID MythCodecContext::FindDecoder(const QString &Decoder, AVStream *Stream,
                                          AVCodecContext **Context, AVCodec **Codec)
{
    MythCodecID result = kCodec_NONE;
    uint streamtype = mpeg_version((*Context)->codec_id);

#ifdef USING_VDPAU
    result = MythVDPAUContext::GetSupportedCodec(Context, Codec, Decoder, streamtype);
    if (codec_is_vdpau_hw(result) || codec_is_vdpau_dechw(result))
        return result;
#endif
#ifdef USING_VAAPI
    result = MythVAAPIContext::GetSupportedCodec(Context, Codec, Decoder, streamtype);
    if (codec_is_vaapi(result) || codec_is_vaapi_dec(result))
        return result;
#endif
#ifdef USING_VTB
    (void)Stream;
    result = MythVTBContext::GetSupportedCodec(Context, Codec, Decoder, streamtype);
    if (codec_is_vtb(result) || codec_is_vtb_dec(result))
        return result;
#endif
#ifdef USING_DXVA2
    result = VideoOutputD3D::GetBestSupportedCodec(width, height, Decoder, streamtype, false);
    if (codec_is_dxva2(result))
        return result;
#endif
#ifdef USING_MEDIACODEC
    result = MythMediaCodecContext::GetBestSupportedCodec(Context, Codec, Decoder, Stream, streamtype);
    if (codec_is_mediacodec(result) || codec_is_mediacodec_dec(result))
        return result;
#endif
#ifdef USING_NVDEC
    result = MythNVDECContext::GetSupportedCodec(Context, Codec, Decoder, Stream, streamtype);
    if (codec_is_nvdec(result) || codec_is_nvdec_dec(result))
        return result;
#endif
#ifdef USING_V4L2
    result = MythV4L2M2MContext::GetSupportedCodec(Context, Codec, Decoder, Stream, streamtype);
    if (codec_is_v4l2_dec(result) || codec_is_v4l2(result))
        return result;
#endif
#ifdef USING_MMAL
    result = MythMMALContext::GetSupportedCodec(Context, Codec, Decoder, Stream, streamtype);
    if (codec_is_mmal_dec(result) || codec_is_mmal(result))
        return result;
#endif
#ifdef USING_EGL
    result = MythDRMPRIMEContext::GetSupportedCodec(Context, Codec, Decoder, Stream, streamtype);
    if (codec_is_drmprime(result))
        return result;
#endif

    return kCodec_NONE;
}

void MythCodecContext::InitVideoCodec(AVCodecContext *Context,
                                      bool SelectedStream, bool &DirectRendering)
{
    const AVCodec *codec1 = Context->codec;
    if (codec1 && codec1->capabilities & AV_CODEC_CAP_DR1)
    {
        // Context->flags |= CODEC_FLAG_EMU_EDGE;
    }
    else
    {
        if (SelectedStream)
            DirectRendering = false;
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("Using software scaling to convert pixel format %1 for "
                    "codec %2").arg(av_get_pix_fmt_name(Context->pix_fmt))
                .arg(ff_codec_id_string(Context->codec_id)));
    }
}

/// \brief A generic hardware buffer initialisation method when using AVHWFramesContext.
int MythCodecContext::GetBuffer(struct AVCodecContext *Context, AVFrame *Frame, int Flags)
{
    auto *avfd = static_cast<AvFormatDecoder*>(Context->opaque);
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
        auto *context = reinterpret_cast<AVHWFramesContext*>(Frame->hw_frames_ctx->data);
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
bool MythCodecContext::GetBuffer2(struct AVCodecContext *Context, VideoFrame* Frame,
                                 AVFrame *AvFrame, int /*Flags*/)
{
    if (!AvFrame || !Context || !Frame)
        return false;

    auto *avfd = static_cast<AvFormatDecoder*>(Context->opaque);

    Frame->pix_fmt = Context->pix_fmt;
    Frame->directrendering = 1;
    Frame->colorshifted = 1;

    AvFrame->reordered_opaque = Context->reordered_opaque;
    AvFrame->opaque = Frame;

    // retrieve the software format
    if (AvFrame->hw_frames_ctx)
    {
        auto *context = reinterpret_cast<AVHWFramesContext*>(AvFrame->hw_frames_ctx->data);
        if (context)
            Frame->sw_pix_fmt = context->sw_format;
    }

    // the hardware surface is stored in Frame->data[3]
    Frame->buf = AvFrame->data[3];

    // Frame->buf[0] contains the release method. Take another reference to
    // ensure the frame is not released before it is displayed.
    Frame->priv[0] = reinterpret_cast<unsigned char*>(av_buffer_ref(AvFrame->buf[0]));

    // Retrieve and set the interop class
    auto *devicectx = reinterpret_cast<AVHWDeviceContext*>(Context->hw_device_ctx->data);
    Frame->priv[1] = reinterpret_cast<unsigned char*>(devicectx->user_opaque);

    // Set release method
    AvFrame->buf[1] = av_buffer_create(reinterpret_cast<uint8_t*>(Frame), 0,
                                       MythCodecContext::ReleaseBuffer, avfd, 0);
    return true;
}

void MythCodecContext::ReleaseBuffer(void *Opaque, uint8_t *Data)
{
    auto *decoder = static_cast<AvFormatDecoder*>(Opaque);
    auto *frame = reinterpret_cast<VideoFrame*>(Data);
    if (decoder && decoder->GetPlayer())
        decoder->GetPlayer()->DeLimboFrame(frame);
}

/*! \brief Track the number of concurrent frames contexts
 *
 * More than one hardware frames context is indicative of wider issues and needs
 * to be avoided.
 *
 * This currently only applies to VDPAU and VAAPI
*/
void MythCodecContext::NewHardwareFramesContext(void)
{
    int count = ++s_hwFramesContextCount;
    if (count != 1)
        LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Error: %1 concurrent hardware frames contexts").arg(count));
}

void MythCodecContext::FramesContextFinished(AVHWFramesContext *Context)
{
    s_hwFramesContextCount--;
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("%1 frames context finished")
        .arg(av_hwdevice_get_type_name(Context->device_ctx->type)));
    auto *interop = reinterpret_cast<MythOpenGLInterop*>(Context->user_opaque);
    if (interop)
        DestroyInterop(interop);
}

void MythCodecContext::DeviceContextFinished(AVHWDeviceContext* Context)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("%1 device context finished")
        .arg(av_hwdevice_get_type_name(Context->type)));
    auto *interop = reinterpret_cast<MythOpenGLInterop*>(Context->user_opaque);
    if (interop)
    {
        DestroyInterop(interop);
        FreeAVHWDeviceContext free = interop->GetDefaultFree();
        if (free)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC + "Calling default device context free");
            Context->user_opaque = interop->GetDefaultUserOpaque();
            free(Context);
        }
    }
}

void MythCodecContext::DestroyInterop(MythOpenGLInterop *Interop)
{
    if (gCoreContext->IsUIThread())
    {
        Interop->DecrRef();
        return;
    }

    auto destroy = [](void *Wait, void *Interop2, void* /*unused*/)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Destroy interop callback");
        auto *wait = reinterpret_cast<QWaitCondition*>(Wait);
        auto *interop = reinterpret_cast<MythOpenGLInterop*>(Interop2);
        if (interop)
            interop->DecrRef();
        if (wait)
            wait->wakeAll();
    };

    if (!Interop->GetPlayer())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Cannot destroy interop - no player");
        return;
    }
    MythPlayer::HandleDecoderCallback(Interop->GetPlayer(), "Destroy OpenGL interop",
                                      destroy, Interop, nullptr);
}

void MythCodecContext::CreateDecoderCallback(void *Wait, void *Context, void *Callback)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Create decoder callback");
    auto *wait     = reinterpret_cast<QWaitCondition*>(Wait);
    auto *context  = reinterpret_cast<AVCodecContext*>(Context);
    auto callback  = reinterpret_cast<CreateHWDecoder>(Callback);
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

    // Callback to MythPlayer (which will fail without a MythPlayer instance)
    MythPlayer *player = nullptr;
    AvFormatDecoder *decoder = reinterpret_cast<AvFormatDecoder*>(Context->opaque);
    if (decoder)
        player = decoder->GetPlayer();
    MythPlayer::HandleDecoderCallback(player, Debug, MythCodecContext::CreateDecoderCallback,
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

    // Callback to MythPlayer (which will fail without a MythPlayer instance)
    MythPlayer *player = nullptr;
    AvFormatDecoder *decoder = reinterpret_cast<AvFormatDecoder*>(Context->opaque);
    if (decoder)
        player = decoder->GetPlayer();
    MythPlayer::HandleDecoderCallback(player, Debug, MythCodecContext::CreateDecoderCallback,
                                      Context, reinterpret_cast<void*>(Callback));
    return Context->hw_device_ctx ? 0 : -1;
}

AVBufferRef* MythCodecContext::CreateDevice(AVHWDeviceType Type, MythOpenGLInterop *Interop, const QString &Device)
{
    AVBufferRef* result = nullptr;
    int res = av_hwdevice_ctx_create(&result, Type, Device.isEmpty() ? nullptr :
                                     Device.toLocal8Bit().constData(), nullptr, 0);
    if (res == 0)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Created hardware device '%1'%2")
            .arg(av_hwdevice_get_type_name(Type))
            .arg(Device == nullptr ? "" : QString(" (%1)").arg(Device)));
        auto *context = reinterpret_cast<AVHWDeviceContext*>(result->data);

        if ((context->free || context->user_opaque) && !Interop)
        {
            LOG(VB_PLAYBACK, LOG_INFO, "Creating dummy interop");
            Interop = MythOpenGLInterop::CreateDummy();
        }

        if (Interop)
        {
            Interop->SetDefaultFree(context->free);
            Interop->SetDefaultUserOpaque(context->user_opaque);
            Interop->IncrRef();
        }

        context->free = MythCodecContext::DeviceContextFinished;
        context->user_opaque = Interop;
        return result;
    }

    char error[AV_ERROR_MAX_STRING_SIZE];
    LOG(VB_PLAYBACK, LOG_ERR, LOC + QString("Failed to create hardware device '%1'%2 Error '%3'")
        .arg(av_hwdevice_get_type_name(Type))
        .arg(Device == nullptr ? "" : QString(" (%1)").arg(Device))
        .arg(av_make_error_string(error, sizeof(error), res)));
    return nullptr;
}

/// Most hardware decoders do not support these codecs/profiles
bool MythCodecContext::IsUnsupportedProfile(AVCodecContext *Context)
{
    switch (Context->codec_id)
    {
        case AV_CODEC_ID_H264:
            switch (Context->profile)
            {
                case FF_PROFILE_H264_HIGH_10:
                case FF_PROFILE_H264_HIGH_10_INTRA:
                case FF_PROFILE_H264_HIGH_422:
                case FF_PROFILE_H264_HIGH_422_INTRA:
                case FF_PROFILE_H264_HIGH_444_PREDICTIVE:
                case FF_PROFILE_H264_HIGH_444_INTRA:
                case FF_PROFILE_H264_CAVLC_444: return true;
                default: break;
            }
            break;
        default: break;
    }
    return false;
}

/*! \brief Retrieve and process/filter AVFrame
 * \note This default implementation implements no processing/filtering
*/
int MythCodecContext::FilteredReceiveFrame(AVCodecContext *Context, AVFrame *Frame)
{
    return avcodec_receive_frame(Context, Frame);
}

bool MythCodecContext::RetrieveHWFrame(VideoFrame *Frame, AVFrame *AvFrame)
{
    if (!Frame || !AvFrame)
        return false;

    AVFrame *temp = av_frame_alloc();
    if (!temp)
        return false;

    AVPixelFormat *pixelformats = nullptr;
    int ret = av_hwframe_transfer_get_formats(AvFrame->hw_frames_ctx,
                                              AV_HWFRAME_TRANSFER_DIRECTION_FROM,
                                              &pixelformats, 0);
    if (ret == 0)
    {
        AVPixelFormat best = m_parent->GetBestVideoFormat(pixelformats);
        if (best != AV_PIX_FMT_NONE)
        {
            VideoFrameType type = PixelFormatToFrameType(best);
            bool valid = Frame->codec == type;
            if (!valid || (Frame->width != AvFrame->width) || (Frame->height != AvFrame->height))
                valid = VideoBuffers::ReinitBuffer(Frame, type, m_parent->GetVideoCodecID(),
                                                   AvFrame->width, AvFrame->height);

            if (valid)
            {
                // Retrieve the picture directly into the VideoFrame Buffer
                temp->format = best;
                uint max = planes(Frame->codec);
                for (uint i = 0; i < 3; i++)
                {
                    temp->data[i]     = (i < max) ? (Frame->buf + Frame->offsets[i]) : nullptr;
                    temp->linesize[i] = Frame->pitches[i];
                }

                // Dummy release method - we do not want to free the buffer
                temp->buf[0] = av_buffer_create(reinterpret_cast<uint8_t*>(Frame), 0,
                                                [](void* /*unused*/, uint8_t* /*unused*/){}, this, 0);
                temp->width = AvFrame->width;
                temp->height = AvFrame->height;
            }
        }
    }
    av_freep(&pixelformats);

    // retrieve data from GPU to CPU
    if (ret >= 0)
        if ((ret = av_hwframe_transfer_data(temp, AvFrame, 0)) < 0)
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("Error %1 transferring the data to system memory").arg(ret));

    Frame->colorshifted = 1;
    av_frame_free(&temp);
    return ret >= 0;
}
