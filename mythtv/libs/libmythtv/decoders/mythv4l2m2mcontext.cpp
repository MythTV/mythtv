// Qt
#include <QDir>

// MythTV
#include "mythlogging.h"
#include "v4l2util.h"
#include "fourcc.h"
#include "avformatdecoder.h"
#include "opengl/mythrenderopengl.h"
#ifdef USING_EGL
#include "opengl/mythdrmprimeinterop.h"
#endif
#include "mythv4l2m2mcontext.h"

#ifdef USING_MMAL
#include "mythmmalcontext.h"
#endif

// Sys
#include <sys/ioctl.h>

// FFmpeg
extern "C" {
#include "libavutil/opt.h"
}

#define LOC QString("V4L2_M2M: ")

static bool s_useV4L2Request = !qEnvironmentVariableIsEmpty("MYTHTV_V4L2_REQUEST");

/*! \class MythV4L2M2MContext
 * \brief A handler for V4L2 Memory2Memory codecs.
 *
 * The bulk of the 'direct rendering' support is in MythDRMPRIMEContext. This
 * sub-class handles v4l2 specific functionality checks and support for software
 * frame formats.
*/
MythV4L2M2MContext::MythV4L2M2MContext(DecoderBase *Parent, MythCodecID CodecID)
  : MythDRMPRIMEContext(Parent, CodecID)
{
}

bool MythV4L2M2MContext::DecoderWillResetOnFlush(void)
{
    return codec_is_v4l2(m_codecID);
}

MythCodecID MythV4L2M2MContext::GetSupportedCodec(AVCodecContext **Context,
                                                  AVCodec **Codec,
                                                  const QString &Decoder,
                                                  AVStream *Stream,
                                                  uint StreamType)
{
    bool decodeonly = Decoder == "v4l2-dec";
    auto success = static_cast<MythCodecID>((decodeonly ? kCodec_MPEG1_V4L2_DEC : kCodec_MPEG1_V4L2) + (StreamType - 1));
    auto failure = static_cast<MythCodecID>(kCodec_MPEG1 + (StreamType - 1));

    // not us
    if (!Decoder.startsWith("v4l2"))
        return failure;

    // supported by device driver?
    MythCodecContext::CodecProfile mythprofile = MythCodecContext::NoProfile;
    switch ((*Codec)->id)
    {
        case AV_CODEC_ID_MPEG1VIDEO: mythprofile = MythCodecContext::MPEG1; break;
        case AV_CODEC_ID_MPEG2VIDEO: mythprofile = MythCodecContext::MPEG2; break;
        case AV_CODEC_ID_MPEG4:      mythprofile = MythCodecContext::MPEG4; break;
        case AV_CODEC_ID_H263:       mythprofile = MythCodecContext::H263;  break;
        case AV_CODEC_ID_H264:       mythprofile = MythCodecContext::H264;  break;
        case AV_CODEC_ID_VC1:        mythprofile = MythCodecContext::VC1;   break;
        case AV_CODEC_ID_VP8:        mythprofile = MythCodecContext::VP8;   break;
        case AV_CODEC_ID_VP9:        mythprofile = MythCodecContext::VP9;   break;
        case AV_CODEC_ID_HEVC:       mythprofile = MythCodecContext::HEVC;  break;
        default: break;
    }

    if (mythprofile == MythCodecContext::NoProfile)
        return failure;

    const V4L2Profiles& profiles = MythV4L2M2MContext::GetProfiles();
    if (!profiles.contains(mythprofile))
        return failure;

#ifdef USING_MMAL
    // If MMAL is available, assume this is a Raspberry Pi and check the supported
    // video sizes
    if (!MythMMALContext::CheckCodecSize((*Context)->width, (*Context)->height, mythprofile))
        return failure;
    // As for MMAL, don't try and decode 10bit H264
    if ((*Codec)->id == AV_CODEC_ID_H264)
    {
        if ((*Context)->profile == FF_PROFILE_H264_HIGH_10 ||
            (*Context)->profile == FF_PROFILE_H264_HIGH_10_INTRA)
        {
            return failure;
        }
    }
#endif

    if (s_useV4L2Request && !decodeonly)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Forcing support for %1 v42l_request")
            .arg(ff_codec_id_string((*Context)->codec_id)));
        (*Context)->pix_fmt = AV_PIX_FMT_DRM_PRIME;
        return success;
    }

    return MythDRMPRIMEContext::GetPrimeCodec(Context, Codec, Stream,
                                              success, failure, "v4l2m2m",
                                              decodeonly ? (*Context)->pix_fmt : AV_PIX_FMT_DRM_PRIME);
}

int MythV4L2M2MContext::HwDecoderInit(AVCodecContext *Context)
{
    if (!Context)
        return -1;
    if (s_useV4L2Request && codec_is_v4l2(m_codecID))
        return 0;
    if (codec_is_v4l2_dec(m_codecID))
        return 0;
    return MythDRMPRIMEContext::HwDecoderInit(Context);
}

void MythV4L2M2MContext::InitVideoCodec(AVCodecContext *Context, bool SelectedStream, bool &DirectRendering)
{
    if (s_useV4L2Request && codec_is_v4l2(m_codecID))
    {
        Context->get_format  = MythV4L2M2MContext::GetV4L2RequestFormat;
        return;
    }

    if (codec_is_v4l2_dec(m_codecID))
    {
        DirectRendering = false;
        return;
    }
    return MythDRMPRIMEContext::InitVideoCodec(Context, SelectedStream, DirectRendering);
}

bool MythV4L2M2MContext::RetrieveFrame(AVCodecContext *Context, VideoFrame *Frame, AVFrame *AvFrame)
{
    if (s_useV4L2Request && codec_is_v4l2(m_codecID))
        return MythCodecContext::GetBuffer2(Context, Frame, AvFrame, 0);

    if (codec_is_v4l2_dec(m_codecID))
        return GetBuffer(Context, Frame, AvFrame, 0);
    return MythDRMPRIMEContext::RetrieveFrame(Context, Frame, AvFrame);
}

/*! \brief Reduce the number of capture buffers
 *
 * Testing on Pi 3, the default of 20 is too high and leads to memory allocation
 * failures in the the kernel driver.
*/
void MythV4L2M2MContext::SetDecoderOptions(AVCodecContext* Context, AVCodec* Codec)
{
    if (s_useV4L2Request && codec_is_v4l2(m_codecID))
        return;

    if (!(Context && Codec))
        return;
    if (!(Codec->priv_class && Context->priv_data))
        return;

    // best guess currently - this matches the number of capture buffers to the
    // number of output buffers - and hence to the number of video buffers for
    // direct rendering
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Setting number of capture buffers to 6");
    av_opt_set_int(Context->priv_data, "num_capture_buffers", 6, 0);
}

/*! \brief Retrieve a frame from CPU memory
 *
 * This is similar to the default, direct render supporting, get_av_buffer in
 * AvFormatDecoder but we copy the data from the AVFrame rather than providing
 * our own buffer (the codec does not support direct rendering).
*/
bool MythV4L2M2MContext::GetBuffer(AVCodecContext *Context, VideoFrame *Frame, AVFrame *AvFrame, int /*Flags*/)
{
    // Sanity checks
    if (!Context || !AvFrame || !Frame)
        return false;

    // Ensure we can render this format
    auto *decoder = static_cast<AvFormatDecoder*>(Context->opaque);
    VideoFrameType type = PixelFormatToFrameType(static_cast<AVPixelFormat>(AvFrame->format));
    const VideoFrameTypeVec* supported = decoder->GetPlayer()->DirectRenderFormats();
    auto foundIt = std::find(supported->cbegin(), supported->cend(), type);
    // No fallback currently (unlikely)
    if (foundIt == supported->end())
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

    return true;
}

#ifndef V4L2_PIX_FMT_HEVC
#define V4L2_PIX_FMT_HEVC v4l2_fourcc('H', 'E', 'V', 'C')
#endif

#ifndef V4L2_PIX_FMT_VP9
#define V4L2_PIX_FMT_VP9 v4l2_fourcc('V', 'P', '9', '0')
#endif

const V4L2Profiles& MythV4L2M2MContext::GetProfiles(void)
{
    using V4L2Mapping = QPair<const uint32_t, const MythCodecContext::CodecProfile>;
    static const std::array<const V4L2Mapping,9> s_map
    {{
        { V4L2_PIX_FMT_MPEG1,       MythCodecContext::MPEG1 },
        { V4L2_PIX_FMT_MPEG2,       MythCodecContext::MPEG2 },
        { V4L2_PIX_FMT_MPEG4,       MythCodecContext::MPEG4 },
        { V4L2_PIX_FMT_H263,        MythCodecContext::H263  },
        { V4L2_PIX_FMT_H264,        MythCodecContext::H264  },
        { V4L2_PIX_FMT_VC1_ANNEX_G, MythCodecContext::VC1   },
        { V4L2_PIX_FMT_VP8,         MythCodecContext::VP8   },
        { V4L2_PIX_FMT_VP9,         MythCodecContext::VP9   },
        { V4L2_PIX_FMT_HEVC,        MythCodecContext::HEVC  }
    }};

    static QMutex lock(QMutex::Recursive);
    static bool s_initialised = false;
    static V4L2Profiles s_profiles;

    QMutexLocker locker(&lock);
    if (s_initialised)
        return s_profiles;
    s_initialised = true;

    if (s_useV4L2Request)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "V4L2Request support endabled - assuming all available");
        for (auto profile : s_map)
            s_profiles.append(profile.second);
        return s_profiles;
    }

    const QString root("/dev/");
    QDir dir(root);
    QStringList namefilters;
    namefilters.append("video*");
    QStringList devices = dir.entryList(namefilters, QDir::Files |QDir::System);
    for (const QString& device : qAsConst(devices))
    {
        V4L2util v4l2dev(root + device);
        uint32_t caps = v4l2dev.GetCapabilities();
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Device: %1 Driver: '%2' Capabilities: 0x%3")
            .arg(v4l2dev.GetDeviceName()).arg(v4l2dev.GetDriverName()).arg(caps, 0, 16));

        // check capture and output support
        // these mimic the device checks in v4l2_m2m.c
        bool mplanar = ((caps & (V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_VIDEO_OUTPUT_MPLANE)) != 0U) &&
                       ((caps & V4L2_CAP_STREAMING) != 0U);
        bool mplanarm2m = (caps & V4L2_CAP_VIDEO_M2M_MPLANE) != 0U;
        bool splanar = ((caps & (V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_VIDEO_OUTPUT)) != 0U) &&
                       ((caps & V4L2_CAP_STREAMING) != 0U);
        bool splanarm2m = (caps & V4L2_CAP_VIDEO_M2M) != 0U;

        if (!(mplanar || mplanarm2m || splanar || splanarm2m))
            continue;

        v4l2_buf_type capturetype = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        v4l2_buf_type outputtype = V4L2_BUF_TYPE_VIDEO_OUTPUT;

        if (mplanar || mplanarm2m)
        {
            capturetype = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
            outputtype = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        }

        // check codec support
        QStringList debug;
        QSize dummy{0, 0};
        for (auto profile : s_map)
        {
            bool found = false;
            uint32_t v4l2pixfmt = profile.first;
            MythCodecContext::CodecProfile mythprofile = profile.second;
            struct v4l2_fmtdesc fdesc {};
            memset(&fdesc, 0, sizeof(fdesc));

            // check output first
            fdesc.type = outputtype;
            while (!found)
            {
                int res = ioctl(v4l2dev.FD(), VIDIOC_ENUM_FMT, &fdesc);
                if (res)
                    break;
                if (fdesc.pixelformat == v4l2pixfmt)
                    found = true;
                fdesc.index++;
            }

            if (found)
            {
                QStringList pixformats;
                bool foundfmt = false;
                // check capture
                memset(&fdesc, 0, sizeof(fdesc));
                fdesc.type = capturetype;
                while (true)
                {
                    int res = ioctl(v4l2dev.FD(), VIDIOC_ENUM_FMT, &fdesc);
                    if (res)
                        break;
                    pixformats.append(fourcc_str(static_cast<int>(fdesc.pixelformat)));

                    // this is a bit of a shortcut
                    if (fdesc.pixelformat == V4L2_PIX_FMT_YUV420 ||
                        fdesc.pixelformat == V4L2_PIX_FMT_YVU420 ||
                        fdesc.pixelformat == V4L2_PIX_FMT_YUV420M ||
                        fdesc.pixelformat == V4L2_PIX_FMT_YVU420M ||
                        fdesc.pixelformat == V4L2_PIX_FMT_NV12   ||
                        fdesc.pixelformat == V4L2_PIX_FMT_NV12M  ||
                        fdesc.pixelformat == V4L2_PIX_FMT_NV21   ||
                        fdesc.pixelformat == V4L2_PIX_FMT_NV21M)
                    {
                        if (!s_profiles.contains(mythprofile))
                            s_profiles.append(mythprofile);
                        foundfmt = true;
                        break;
                    }
                    fdesc.index++;
                }

                if (!foundfmt)
                {
                    if (pixformats.isEmpty())
                        pixformats.append("None");
                    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Codec '%1' has no supported formats (Supported: %2)")
                        .arg(MythCodecContext::GetProfileDescription(mythprofile, dummy)).arg(pixformats.join((","))));
                }
            }
        }
    }

    return s_profiles;
}

void MythV4L2M2MContext::GetDecoderList(QStringList &Decoders)
{
    const V4L2Profiles& profiles = MythV4L2M2MContext::GetProfiles();
    if (profiles.isEmpty())
        return;

    QSize size(0, 0);
    Decoders.append("V4L2:");
    for (MythCodecContext::CodecProfile profile : profiles)
        Decoders.append(MythCodecContext::GetProfileDescription(profile, size));
}

bool MythV4L2M2MContext::HaveV4L2Codecs(void)
{
    static QMutex lock(QMutex::Recursive);
    QMutexLocker locker(&lock);
    static bool s_checked = false;
    static bool s_available = false;

    if (s_checked)
        return s_available;
    s_checked = true;

    const V4L2Profiles& profiles = MythV4L2M2MContext::GetProfiles();
    if (profiles.isEmpty())
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "No V4L2 decoders found");
        return s_available;
    }

    LOG(VB_GENERAL, LOG_INFO, LOC + "Supported/available V4L2 decoders:");
    s_available = true;
    QSize size{0, 0};
    for (auto profile : qAsConst(profiles))
        LOG(VB_GENERAL, LOG_INFO, LOC + MythCodecContext::GetProfileDescription(profile, size));
    return s_available;
}

AVPixelFormat MythV4L2M2MContext::GetV4L2RequestFormat(AVCodecContext *Context, const AVPixelFormat *PixFmt)
{
    while (*PixFmt != AV_PIX_FMT_NONE)
    {
        if (*PixFmt == AV_PIX_FMT_DRM_PRIME)
        {
            if (MythCodecContext::InitialiseDecoder(Context, MythV4L2M2MContext::InitialiseV4L2RequestContext,
                                                    "V4L2 request context creation") >= 0)
                return AV_PIX_FMT_DRM_PRIME;
        }
        PixFmt++;
    }
    return AV_PIX_FMT_NONE;
}

int MythV4L2M2MContext::InitialiseV4L2RequestContext(AVCodecContext *Context)
{
    if (!Context || !gCoreContext->IsUIThread())
        return -1;

    // We need a render device
    MythRenderOpenGL* render = MythRenderOpenGL::GetOpenGLRender();
    if (!render)
        return -1;

    // The interop must have a reference to the player so it can be deleted
    // from the main thread.
    MythPlayer *player = nullptr;
    auto *decoder = reinterpret_cast<AvFormatDecoder*>(Context->opaque);
    if (decoder)
        player = decoder->GetPlayer();
    if (!player)
        return -1;

    // Check interop support
    MythOpenGLInterop::Type type = MythOpenGLInterop::GetInteropType(FMT_DRMPRIME, player);
    if (type == MythOpenGLInterop::Unsupported)
        return -1;

    // Create interop
    MythOpenGLInterop *interop = nullptr;
#ifdef USING_EGL
    interop = MythDRMPRIMEInterop::Create(render, type);
#endif
    if (!interop)
        return -1;

    // Set the player required to process interop release
    interop->SetPlayer(player);

    // Allocate the device context
    AVBufferRef* hwdeviceref = MythCodecContext::CreateDevice(AV_HWDEVICE_TYPE_DRM, interop);
    if (!hwdeviceref)
    {
        interop->DecrRef();
        return -1;
    }

    auto* hwdevicecontext = reinterpret_cast<AVHWDeviceContext*>(hwdeviceref->data);
    if (!hwdevicecontext || !hwdevicecontext->hwctx)
    {
        interop->DecrRef();
        return -1;
    }

    // Initialise device context
    if (av_hwdevice_ctx_init(hwdeviceref) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to initialise device context");
        av_buffer_unref(&hwdeviceref);
        interop->DecrRef();
        return -1;
    }

    Context->hw_device_ctx = hwdeviceref;
    return 0;
}
