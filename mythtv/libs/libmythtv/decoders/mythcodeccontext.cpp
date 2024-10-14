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

#include "libmyth/mythaverror.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "libmythui/mythmainwindow.h"
#include "mythinteropgpu.h"
#include "avformatdecoder.h"
#include "mythplayerui.h"

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
#ifdef USING_DXVA2
#include "videoout_d3d.h"
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

MythCodecContext *MythCodecContext::CreateContext(DecoderBase *Parent,
                                                  [[maybe_unused]] MythCodecID Codec)
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

    if (!mctx)
        mctx = new MythCodecContext(Parent, Codec);
    return mctx;
}

QStringList MythCodecContext::GetDecoderDescription(void)
{
    QStringList decoders;

#ifdef USING_VDPAU
    MythVDPAUHelper::GetDecoderList(decoders);
#endif
#ifdef USING_VAAPI
    MythVAAPIContext::GetDecoderList(decoders);
#endif
#ifdef USING_MEDIACODEC
    MythMediaCodecContext::GetDecoderList(decoders);
#endif
#ifdef USING_NVDEC
    MythNVDECContext::GetDecoderList(decoders);
#endif
#ifdef USING_MMAL
    MythMMALContext::GetDecoderList(decoders);
#endif
#ifdef USING_V4L2
    MythV4L2M2MContext::GetDecoderList(decoders);
#endif
#ifdef USING_VTB
    MythVTBContext::GetDecoderList(decoders);
#endif
    return decoders;
}

void MythCodecContext::GetDecoders(RenderOptions &Opts, bool Reinit /*=false*/)
{
    if (!gCoreContext->IsUIThread())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Must be called from UI thread");
        return;
    }

    if (!HasMythMainWindow())
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "No window: Ignoring hardware decoders");
        return;
    }

    Opts.decoders->append("ffmpeg");
    (*Opts.equiv_decoders)["ffmpeg"].append("dummy");

#ifdef USING_VDPAU
    // Only enable VDPAU support if it is actually present
    if (MythVDPAUHelper::HaveVDPAU(Reinit))
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
    if (!MythVAAPIContext::HaveVAAPI(Reinit).isEmpty())
    {
        Opts.decoders->append("vaapi");
        (*Opts.equiv_decoders)["vaapi"].append("dummy");
        Opts.decoders->append("vaapi-dec");
        (*Opts.equiv_decoders)["vaapi-dec"].append("dummy");
    }
#endif
#ifdef USING_NVDEC
    // Only enable NVDec support if it is actually present
    if (MythNVDECContext::HaveNVDEC(Reinit))
    {
        Opts.decoders->append("nvdec");
        (*Opts.equiv_decoders)["nvdec"].append("dummy");
        Opts.decoders->append("nvdec-dec");
        (*Opts.equiv_decoders)["nvdec-dec"].append("dummy");
    }
#endif
#ifdef USING_MEDIACODEC
    if (MythMediaCodecContext::HaveMediaCodec(Reinit))
    {
        Opts.decoders->append("mediacodec");
        (*Opts.equiv_decoders)["mediacodec"].append("dummy");
        Opts.decoders->append("mediacodec-dec");
        (*Opts.equiv_decoders)["mediacodec-dec"].append("dummy");
    }
#endif
#ifdef USING_VTB
    if (MythVTBContext::HaveVTB(Reinit))
    {
        Opts.decoders->append("vtb");
        Opts.decoders->append("vtb-dec");
        (*Opts.equiv_decoders)["vtb"].append("dummy");
        (*Opts.equiv_decoders)["vtb-dec"].append("dummy");
    }
#endif
#ifdef USING_V4L2
    if (MythV4L2M2MContext::HaveV4L2Codecs(Reinit))
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
    if (MythDRMPRIMEContext::HavePrimeDecoders(Reinit))
    {
        Opts.decoders->append("drmprime");
        (*Opts.equiv_decoders)["drmprime"].append("dummy");
    }
#endif
#ifdef USING_MMAL
    if (MythMMALContext::HaveMMAL(Reinit))
    {
        Opts.decoders->append("mmal-dec");
        (*Opts.equiv_decoders)["mmal-dec"].append("dummy");
        auto types = MythInteropGPU::GetTypes(MythRenderOpenGL::GetOpenGLRender());
        if (auto mmal = types.find(FMT_MMAL); (mmal != types.end()) && !mmal->second.empty())
        {
            Opts.decoders->append("mmal");
            (*Opts.equiv_decoders)["mmal"].append("dummy");
        }
    }
#endif
}

MythCodecID MythCodecContext::FindDecoder(const QString &Decoder,
                                          [[maybe_unused]] AVStream *Stream,
                                          AVCodecContext **Context,
                                          const AVCodec **Codec)
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
    result = MythVTBContext::GetSupportedCodec(Context, Codec, Decoder, streamtype);
    if (codec_is_vtb(result) || codec_is_vtb_dec(result))
        return result;
#endif
#ifdef USING_DXVA2
    result = VideoOutputD3D::GetSupportedCodec(Context, Codec, Decoder, streamtype);
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
                    "codec %2").arg(av_get_pix_fmt_name(Context->pix_fmt),
                                    avcodec_get_name(Context->codec_id)));
    }
}

/// \brief A generic hardware buffer initialisation method when using AVHWFramesContext.
int MythCodecContext::GetBuffer(struct AVCodecContext *Context, AVFrame *Frame, int Flags)
{
    auto *avfd = static_cast<AvFormatDecoder*>(Context->opaque);
    MythVideoFrame *videoframe = avfd->GetPlayer()->GetNextVideoFrame();

    // set fields required for directrendering
    for (int i = 0; i < 4; i++)
    {
        Frame->data[i]     = nullptr;
        Frame->linesize[i] = 0;
    }
    Frame->opaque           = videoframe;
    videoframe->m_pixFmt    = Context->pix_fmt;

    int ret = avcodec_default_get_buffer2(Context, Frame, Flags);
    if (ret < 0)
        return ret;

    // set the underlying pixel format. Set here rather than guessing later.
    if (Frame->hw_frames_ctx)
    {
        auto *context = reinterpret_cast<AVHWFramesContext*>(Frame->hw_frames_ctx->data);
        if (context)
            videoframe->m_swPixFmt = context->sw_format;
    }

    // VAAPI 'fixes' 10/12/16bit colour values. Irrelevant for VDPAU.
    videoframe->m_colorshifted = true;

    // avcodec_default_get_buffer2 will retrieve an AVBufferRef from the pool of
    // hardware surfaces stored within AVHWFramesContext. The pointer to the surface is stored
    // in Frame->data[3]. Store this in VideoFrame::buf for the interop class to use.
    videoframe->m_buffer = Frame->data[3];
    // Frame->buf(0) also contains a reference to the buffer. Take an additional reference to this
    // buffer to retain the surface until it has been displayed (otherwise it is
    // reused once the decoder is finished with it).
    videoframe->m_priv[0] = reinterpret_cast<unsigned char*>(av_buffer_ref(Frame->buf[0]));
    // frame->hw_frames_ctx contains a reference to the AVHWFramesContext. Take an additional
    // reference to ensure AVHWFramesContext is not released until we are finished with it.
    // This also contains the underlying MythInteropGPU class reference.
    videoframe->m_priv[1] = reinterpret_cast<unsigned char*>(av_buffer_ref(Frame->hw_frames_ctx));

    // Set release method
    Frame->buf[1] = av_buffer_create(reinterpret_cast<uint8_t*>(videoframe), 0,
                                     MythCodecContext::ReleaseBuffer, avfd, 0);
    return ret;
}


/// \brief A generic hardware buffer initialisation method when AVHWFramesContext is NOT used.
bool MythCodecContext::GetBuffer2(struct AVCodecContext *Context, MythVideoFrame* Frame,
                                 AVFrame *AvFrame, int /*Flags*/)
{
    if (!AvFrame || !Context || !Frame)
        return false;

    auto *avfd = static_cast<AvFormatDecoder*>(Context->opaque);

    Frame->m_pixFmt = Context->pix_fmt;
    Frame->m_directRendering = true;
    Frame->m_colorshifted = true;

    AvFrame->opaque = Frame;

    // retrieve the software format
    if (AvFrame->hw_frames_ctx)
    {
        auto *context = reinterpret_cast<AVHWFramesContext*>(AvFrame->hw_frames_ctx->data);
        if (context)
            Frame->m_swPixFmt = context->sw_format;
    }

    // the hardware surface is stored in Frame->data[3]
    Frame->m_buffer = AvFrame->data[3];

    // Frame->buf[0] contains the release method. Take another reference to
    // ensure the frame is not released before it is displayed.
    Frame->m_priv[0] = reinterpret_cast<unsigned char*>(av_buffer_ref(AvFrame->buf[0]));

    // Retrieve and set the interop class
    auto *devicectx = reinterpret_cast<AVHWDeviceContext*>(Context->hw_device_ctx->data);
    Frame->m_priv[1] = reinterpret_cast<unsigned char*>(devicectx->user_opaque);

    // Set release method
    AvFrame->buf[1] = av_buffer_create(reinterpret_cast<uint8_t*>(Frame), 0,
                                       MythCodecContext::ReleaseBuffer, avfd, 0);
    return true;
}

void MythCodecContext::ReleaseBuffer(void *Opaque, uint8_t *Data)
{
    auto *decoder = static_cast<AvFormatDecoder*>(Opaque);
    auto *frame = reinterpret_cast<MythVideoFrame*>(Data);
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
    auto * interop = reinterpret_cast<MythInteropGPU*>(Context->user_opaque);
    if (interop)
        DestroyInterop(interop);
}

void MythCodecContext::DeviceContextFinished(AVHWDeviceContext* Context)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("%1 device context finished")
        .arg(av_hwdevice_get_type_name(Context->type)));
    auto * interop = reinterpret_cast<MythInteropGPU*>(Context->user_opaque);
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

void MythCodecContext::DestroyInterop(MythInteropGPU* Interop)
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
        auto *interop = reinterpret_cast<MythInteropGPU*>(Interop2);
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
    Interop->GetPlayer()->HandleDecoderCallback("Destroy OpenGL interop",
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

MythPlayerUI* MythCodecContext::GetPlayerUI(AVCodecContext *Context)
{
    MythPlayerUI* result = nullptr;
    auto* decoder = reinterpret_cast<AvFormatDecoder*>(Context->opaque);
    if (decoder)
        result = dynamic_cast<MythPlayerUI*>(decoder->GetPlayer());
    return result;
}

bool MythCodecContext::FrameTypeIsSupported(AVCodecContext* Context, VideoFrameType Format)
{
    if (auto * player = GetPlayerUI(Context); player != nullptr)
    {
        const auto & supported = player->GetInteropTypes();
        return supported.find(Format) != supported.cend();
    }
    return false;
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
    MythPlayerUI* player = GetPlayerUI(Context);
    if (player)
        player->HandleDecoderCallback(Debug, MythCodecContext::CreateDecoderCallback,
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
    MythPlayerUI* player = GetPlayerUI(Context);
    if (player)
        player->HandleDecoderCallback(Debug, MythCodecContext::CreateDecoderCallback,
                                      Context, reinterpret_cast<void*>(Callback));
    return Context->hw_device_ctx ? 0 : -1;
}

AVBufferRef* MythCodecContext::CreateDevice(AVHWDeviceType Type, MythInteropGPU* Interop, const QString& Device)
{
    AVBufferRef* result = nullptr;
    int res = av_hwdevice_ctx_create(&result, Type, Device.isEmpty() ? nullptr :
                                     Device.toLocal8Bit().constData(), nullptr, 0);
    if (res == 0)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Created hardware device '%1'%2")
            .arg(av_hwdevice_get_type_name(Type),
                 Device == nullptr ? "" : QString(" (%1)").arg(Device)));
        auto *context = reinterpret_cast<AVHWDeviceContext*>(result->data);

        if ((context->free || context->user_opaque) && !Interop)
        {
            LOG(VB_PLAYBACK, LOG_INFO, "Creating dummy interop");
            Interop = MythInteropGPU::CreateDummy();
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

    std::string error;
    LOG(VB_PLAYBACK, LOG_ERR, LOC + QString("Failed to create hardware device '%1'%2 Error '%3'")
        .arg(av_hwdevice_get_type_name(Type),
             Device == nullptr ? "" : QString(" (%1)").arg(Device),
             av_make_error_stdstring(error, res)));
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

bool MythCodecContext::RetrieveHWFrame(MythVideoFrame *Frame, AVFrame *AvFrame)
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
        AVPixelFormat best = DecoderBase::GetBestVideoFormat(pixelformats, Frame->m_renderFormats);
        if (best != AV_PIX_FMT_NONE)
        {
            VideoFrameType type = MythAVUtil::PixelFormatToFrameType(best);
            bool valid = Frame->m_type == type;
            if (!valid || (Frame->m_width != AvFrame->width) || (Frame->m_height != AvFrame->height))
                valid = VideoBuffers::ReinitBuffer(Frame, type, m_parent->GetVideoCodecID(),
                                                   AvFrame->width, AvFrame->height);

            if (valid)
            {
                // Retrieve the picture directly into the VideoFrame Buffer
                temp->format = best;
                uint max = MythVideoFrame::GetNumPlanes(Frame->m_type);
                for (uint i = 0; i < 3; i++)
                {
                    temp->data[i]     = (i < max) ? (Frame->m_buffer + Frame->m_offsets[i]) : nullptr;
                    temp->linesize[i] = Frame->m_pitches[i];
                }

                // Dummy release method - we do not want to free the buffer
                temp->buf[0] = av_buffer_create(reinterpret_cast<uint8_t*>(Frame), 0,
                                                [](void* /*unused*/, uint8_t* /*unused*/){}, this, 0);
                temp->width = AvFrame->width;
                temp->height = AvFrame->height;
            }
        }
    }
    av_freep(reinterpret_cast<void*>(&pixelformats));

    // retrieve data from GPU to CPU
    if (ret >= 0)
    {
        ret = av_hwframe_transfer_data(temp, AvFrame, 0);
        if (ret < 0)
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("Error %1 transferring the data to system memory").arg(ret));
    }

    Frame->m_colorshifted = true;
    av_frame_free(&temp);
    return ret >= 0;
}

MythCodecContext::CodecProfile MythCodecContext::FFmpegToMythProfile(AVCodecID CodecID, int Profile)
{
    switch (CodecID)
    {
        case AV_CODEC_ID_MPEG1VIDEO: return MPEG1;
        case AV_CODEC_ID_MPEG2VIDEO:
            switch (Profile)
            {
                case FF_PROFILE_MPEG2_422:          return MPEG2422;
                case FF_PROFILE_MPEG2_HIGH:         return MPEG2High;
                case FF_PROFILE_MPEG2_SS:           return MPEG2Spatial;
                case FF_PROFILE_MPEG2_SNR_SCALABLE: return MPEG2SNR;
                case FF_PROFILE_MPEG2_SIMPLE:       return MPEG2Simple;
                case FF_PROFILE_MPEG2_MAIN:         return MPEG2Main;
                default: break;
            }
            break;
        case AV_CODEC_ID_MPEG4:
            switch (Profile)
            {
                case FF_PROFILE_MPEG4_SIMPLE:             return MPEG4Simple;
                case FF_PROFILE_MPEG4_SIMPLE_SCALABLE:    return MPEG4SimpleScaleable;
                case FF_PROFILE_MPEG4_CORE:               return MPEG4Core;
                case FF_PROFILE_MPEG4_MAIN:               return MPEG4Main;
                case FF_PROFILE_MPEG4_N_BIT:              return MPEG4NBit;
                case FF_PROFILE_MPEG4_SCALABLE_TEXTURE:   return MPEG4ScaleableTexture;
                case FF_PROFILE_MPEG4_SIMPLE_FACE_ANIMATION:  return MPEG4SimpleFace;
                case FF_PROFILE_MPEG4_BASIC_ANIMATED_TEXTURE: return MPEG4BasicAnimated;
                case FF_PROFILE_MPEG4_HYBRID:             return MPEG4Hybrid;
                case FF_PROFILE_MPEG4_ADVANCED_REAL_TIME: return MPEG4AdvancedRT;
                case FF_PROFILE_MPEG4_CORE_SCALABLE:      return MPEG4CoreScaleable;
                case FF_PROFILE_MPEG4_ADVANCED_CODING:    return MPEG4AdvancedCoding;
                case FF_PROFILE_MPEG4_ADVANCED_CORE:      return MPEG4AdvancedCore;
                case FF_PROFILE_MPEG4_ADVANCED_SCALABLE_TEXTURE: return MPEG4AdvancedScaleableTexture;
                case FF_PROFILE_MPEG4_SIMPLE_STUDIO:      return MPEG4SimpleStudio;
                case FF_PROFILE_MPEG4_ADVANCED_SIMPLE:    return MPEG4AdvancedSimple;
            }
            break;
        case AV_CODEC_ID_H263: return H263;
        case AV_CODEC_ID_H264:
            switch (Profile)
            {
                // Mapping of H264MainExtended, H264ConstrainedHigh?
                case FF_PROFILE_H264_BASELINE: return H264Baseline;
                case FF_PROFILE_H264_CONSTRAINED_BASELINE: return H264ConstrainedBaseline;
                case FF_PROFILE_H264_MAIN:     return H264Main;
                case FF_PROFILE_H264_EXTENDED: return H264Extended;
                case FF_PROFILE_H264_HIGH:     return H264High;
                case FF_PROFILE_H264_HIGH_10:  return H264High10;
                //case FF_PROFILE_H264_HIGH_10_INTRA:
                //case FF_PROFILE_H264_MULTIVIEW_HIGH:
                case FF_PROFILE_H264_HIGH_422: return H264High422;
                //case FF_PROFILE_H264_HIGH_422_INTRA:
                //case FF_PROFILE_H264_STEREO_HIGH:
                case FF_PROFILE_H264_HIGH_444: return H264High444;
                //case FF_PROFILE_H264_HIGH_444_PREDICTIVE:
                //case FF_PROFILE_H264_HIGH_444_INTRA:
                //case FF_PROFILE_H264_CAVLC_444:
            }
            break;
        case AV_CODEC_ID_HEVC:
            switch (Profile)
            {
                case FF_PROFILE_HEVC_MAIN:    return HEVCMain;
                case FF_PROFILE_HEVC_MAIN_10: return HEVCMain10;
                case FF_PROFILE_HEVC_MAIN_STILL_PICTURE: return HEVCMainStill;
                case FF_PROFILE_HEVC_REXT:    return HEVCRext;
            }
            break;
        case AV_CODEC_ID_VC1:
            switch (Profile)
            {
                case FF_PROFILE_VC1_SIMPLE:   return VC1Simple;
                case FF_PROFILE_VC1_MAIN:     return VC1Main;
                case FF_PROFILE_VC1_COMPLEX:  return VC1Complex;
                case FF_PROFILE_VC1_ADVANCED: return VC1Advanced;
            }
            break;
        case AV_CODEC_ID_VP8: return VP8;
        case AV_CODEC_ID_VP9:
            switch (Profile)
            {
                case FF_PROFILE_VP9_0: return VP9_0;
                case FF_PROFILE_VP9_1: return VP9_1;
                case FF_PROFILE_VP9_2: return VP9_2;
                case FF_PROFILE_VP9_3: return VP9_3;
            }
            break;
        case AV_CODEC_ID_AV1:
            switch (Profile)
            {
                case FF_PROFILE_AV1_MAIN: return AV1Main;
                case FF_PROFILE_AV1_HIGH: return AV1High;
                case FF_PROFILE_AV1_PROFESSIONAL: return AV1Professional;
            }
            break;
        case AV_CODEC_ID_MJPEG: return MJPEG;
        default: break;
    }

    return NoProfile;
}

QString MythCodecContext::GetProfileDescription(CodecProfile Profile, QSize Size,
                                                VideoFrameType Format, uint ColorDepth)
{
    QString profile;
    switch (Profile)
    {
        case NoProfile:    profile = QObject::tr("Unknown/Unsupported"); break;
        case MPEG1:        profile = "MPEG1"; break;
        case MPEG2:        profile = "MPEG2"; break;
        case MPEG2Simple:  profile = "MPEG2 Simple"; break;
        case MPEG2Main:    profile = "MPEG2 Main"; break;
        case MPEG2422:     profile = "MPEG2 422"; break;
        case MPEG2High:    profile = "MPEG2 High"; break;
        case MPEG2Spatial: profile = "MPEG2 Spatial"; break;
        case MPEG2SNR:     profile = "MPEG2 SNR"; break;
        case MPEG4:        profile = "MPEG4"; break;
        case MPEG4Simple:  profile = "MPEG4 Simple"; break;
        case MPEG4SimpleScaleable: profile = "MPEG4 Simple Scaleable"; break;
        case MPEG4Core:    profile = "MPEG4 Core"; break;
        case MPEG4Main:    profile = "MPEG4 Main"; break;
        case MPEG4NBit:    profile = "MPEG4 NBit"; break;
        case MPEG4ScaleableTexture: profile = "MPEG4 Scaleable Texture"; break;
        case MPEG4SimpleFace:     profile = "MPEG4 Simple Face"; break;
        case MPEG4BasicAnimated:  profile = "MPEG4 Basic Animated"; break;
        case MPEG4Hybrid:         profile = "MPEG4 Hybrid"; break;
        case MPEG4AdvancedRT:     profile = "MPEG4 Advanced RT"; break;
        case MPEG4CoreScaleable:  profile = "MPEG4 Core Scaleable"; break;
        case MPEG4AdvancedCoding: profile = "MPEG4 Advanced Coding"; break;
        case MPEG4AdvancedCore:   profile = "MPEG4 Advanced Core"; break;
        case MPEG4AdvancedScaleableTexture: profile = "MPEG4 Advanced Scaleable Texture"; break;
        case MPEG4SimpleStudio:   profile = "MPEG4 Simple Studio"; break;
        case MPEG4AdvancedSimple: profile = "MPEG4 Advanced Simple"; break;
        case H263:         profile = "H263"; break;
        case H264:         profile = "H264"; break;
        case H264Baseline: profile = "H264 Baseline"; break;
        case H264ConstrainedBaseline: profile = "H264 Constrained"; break;
        case H264Main:     profile = "H264 Main"; break;
        case H264MainExtended: profile = "H264 Main Extended"; break;
        case H264High:     profile = "H264 High"; break;
        case H264High10:   profile = "H264 High10"; break;
        case H264Extended: profile = "H264 Extended"; break;
        case H264High422:  profile = "H264 High 422"; break;
        case H264High444:  profile = "H264 High 444"; break;
        case H264ConstrainedHigh: profile = "H264 Constrained High"; break;
        case HEVC:         profile = "HEVC"; break;
        case HEVCMain:     profile = "HEVC Main"; break;
        case HEVCMain10:   profile = "HEVC Main10"; break;
        case HEVCMainStill: profile = "HEVC Main Still"; break;
        case HEVCRext:      profile = "HEVC Rext"; break;
        case HEVCMain10HDR: profile = "HEVC Main10HDR"; break;
        case HEVCMain10HDRPlus: profile = "HEVC Main10HDRPlus"; break;
        case VC1:          profile = "VC1"; break;
        case VC1Simple:    profile = "VC1 Simple"; break;
        case VC1Main:      profile = "VC1 Main"; break;
        case VC1Complex:   profile = "VC1 Complex"; break;
        case VC1Advanced:  profile = "VC1 Advanced"; break;
        case VP8:          profile = "VP8"; break;
        case VP9:          profile = "VP9"; break;
        case VP9_0:        profile = "VP9 Level 0"; break;
        case VP9_1:        profile = "VP9 Level 1"; break;
        case VP9_2:        profile = "VP9 Level 2"; break;
        case VP9_2HDR:     profile = "VP9 Level 2 HDR"; break;
        case VP9_2HDRPlus: profile = "VP9 Level 2 HDRPlus"; break;
        case VP9_3:        profile = "VP9 Level 3"; break;
        case VP9_3HDR:     profile = "VP9 Level 3 HDR"; break;
        case VP9_3HDRPlus: profile = "VP9 Level 3 HDRPlus"; break;
        case AV1:          profile = "AV1"; break;
        case AV1Main:      profile = "AV1 Main"; break;
        case AV1High:      profile = "AV1 High"; break;
        case AV1Professional: profile = "AV1 Professional"; break;
        case MJPEG:        profile = "MJPEG";
    }

    if (Size.isEmpty())
        return profile;

    return QObject::tr("%1%2%3 (Max size: %4x%5)")
            .arg(profile,
                 Format != FMT_NONE ? QString(" %1").arg(MythVideoFrame::FormatDescription(Format)) : "",
                 ColorDepth > 8 ? QString(" %1Bit").arg(ColorDepth) : "")
            .arg(Size.width()).arg(Size.height());
}
