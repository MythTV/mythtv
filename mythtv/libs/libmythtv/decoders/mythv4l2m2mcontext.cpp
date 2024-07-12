// Sys
#include <sys/ioctl.h>

// Qt
#include <QDir>

// MythTV
#include "libmythbase/mythlogging.h"
#include "libmythui/opengl/mythrenderopengl.h"

#include "avformatdecoder.h"
#include "fourcc.h"
#include "mythplayerui.h"
#include "v4l2util.h"

#ifdef USING_EGL
#include "opengl/mythdrmprimeinterop.h"
#endif
#include "decoders/mythv4l2m2mcontext.h"

#ifdef USING_MMAL
#include "decoders/mythmmalcontext.h"
#endif

// FFmpeg
extern "C" {
#include "libavutil/opt.h"
}

#define LOC QString("V4L2_M2M: ")

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

bool MythV4L2M2MContext::DecoderWillResetOnFlush()
{
    return codec_is_v4l2(m_codecID);
}

MythCodecID MythV4L2M2MContext::GetSupportedCodec(AVCodecContext **Context,
                                                  const AVCodec **Codec,
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

    if (!decodeonly)
        if (!FrameTypeIsSupported(*Context, FMT_DRMPRIME))
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

    bool request = false;
    const auto & standard = MythV4L2M2MContext::GetStandardProfiles();
    if (!standard.contains(mythprofile))
    {
        const V4L2Profiles& requests = MythV4L2M2MContext::GetRequestProfiles();
        if (!requests.contains(mythprofile))
            return failure;
        request = true;
    }

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

    if (request)
    {
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

    if (codec_is_v4l2_dec(m_codecID))
        return 0;

    return MythDRMPRIMEContext::HwDecoderInit(Context);
}

void MythV4L2M2MContext::InitVideoCodec(AVCodecContext *Context, bool SelectedStream, bool &DirectRendering)
{
    // Fairly circular check of whether our codec id is using the request API.
    // N.B. As for other areas of this class, this assumes there is no overlap
    // between standard and request API codec support - though both can be used
    // but for different codecs (as is expected on the Pi 4)
    CodecProfile profile = NoProfile;
    switch (m_codecID)
    {
        case kCodec_MPEG2_V4L2: profile = MPEG2; break;
        case kCodec_H264_V4L2:  profile = H264;  break;
        case kCodec_VP8_V4L2:   profile = VP8;   break;
        case kCodec_VP9_V4L2:   profile = VP9;   break;
        case kCodec_HEVC_V4L2:  profile = HEVC;  break;
        default: break;
    }

    m_request = profile != NoProfile && GetRequestProfiles().contains(profile);

    if (codec_is_v4l2_dec(m_codecID))
    {
        DirectRendering = false;
        return;
    }

    if (m_request && codec_is_v4l2(m_codecID))
    {
        DirectRendering = false; // Surely true ?? And then an issue for regular V4L2 as well
        Context->get_format = MythV4L2M2MContext::GetV4L2RequestFormat;
        return;
    }

    MythDRMPRIMEContext::InitVideoCodec(Context, SelectedStream, DirectRendering);
}

bool MythV4L2M2MContext::RetrieveFrame(AVCodecContext *Context, MythVideoFrame *Frame, AVFrame *AvFrame)
{
    if (codec_is_v4l2_dec(m_codecID))
        return GetBuffer(Context, Frame, AvFrame, 0);

    if (m_request)
        return MythDRMPRIMEContext::GetDRMBuffer(Context, Frame, AvFrame, 0);

    return MythDRMPRIMEContext::RetrieveFrame(Context, Frame, AvFrame);
}

/*! \brief Reduce the number of capture buffers
 *
 * Testing on Pi 3, the default of 20 is too high and leads to memory allocation
 * failures in the the kernel driver.
*/
void MythV4L2M2MContext::SetDecoderOptions(AVCodecContext* Context, const AVCodec* Codec)
{
    if (m_request)
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
bool MythV4L2M2MContext::GetBuffer(AVCodecContext *Context, MythVideoFrame *Frame, AVFrame *AvFrame, int /*Flags*/)
{
    // Sanity checks
    if (!Context || !AvFrame || !Frame)
        return false;

    // Ensure we can render this format
    auto * decoder = static_cast<AvFormatDecoder*>(Context->opaque);
    auto type = MythAVUtil::PixelFormatToFrameType(static_cast<AVPixelFormat>(AvFrame->format));
    const auto * supported = Frame->m_renderFormats;
    auto found = std::find(supported->cbegin(), supported->cend(), type);
    // No fallback currently (unlikely)
    if (found == supported->end())
        return false;

    // Re-allocate if necessary
    if ((Frame->m_type != type) || (Frame->m_width != AvFrame->width) || (Frame->m_height != AvFrame->height))
        if (!VideoBuffers::ReinitBuffer(Frame, type, decoder->GetVideoCodecID(), AvFrame->width, AvFrame->height))
            return false;

    // Copy data
    uint count = MythVideoFrame::GetNumPlanes(Frame->m_type);
    for (uint plane = 0; plane < count; ++plane)
    {
        MythVideoFrame::CopyPlane(Frame->m_buffer + Frame->m_offsets[plane],Frame->m_pitches[plane],
                                  AvFrame->data[plane], AvFrame->linesize[plane],
                                  MythVideoFrame::GetPitchForPlane(Frame->m_type, AvFrame->width, plane),
                                  MythVideoFrame::GetHeightForPlane(Frame->m_type, AvFrame->height, plane));
    }

    return true;
}

#ifndef V4L2_PIX_FMT_HEVC
#define V4L2_PIX_FMT_HEVC v4l2_fourcc('H', 'E', 'V', 'C')
#endif

#ifndef V4L2_PIX_FMT_VP9
#define V4L2_PIX_FMT_VP9 v4l2_fourcc('V', 'P', '9', '0')
#endif

#ifndef V4L2_PIX_FMT_NV12_COL128
#define V4L2_PIX_FMT_NV12_COL128 v4l2_fourcc('N', 'C', '1', '2')
#endif

#ifndef V4L2_PIX_FMT_NV12_10_COL128
#define V4L2_PIX_FMT_NV12_10_COL128 v4l2_fourcc('N', 'C', '3', '0')
#endif

const V4L2Profiles& MythV4L2M2MContext::GetStandardProfiles()
{
    static const std::vector<V4L2Mapping> s_map
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

    static QRecursiveMutex lock;
    static bool s_initialised = false;
    static V4L2Profiles s_profiles;

    QMutexLocker locker(&lock);
    if (!s_initialised)
        s_profiles = GetProfiles(s_map);
    s_initialised = true;
    return s_profiles;
}

V4L2Profiles MythV4L2M2MContext::GetProfiles(const std::vector<V4L2Mapping>& Profiles)
{
    static const std::vector<uint32_t> s_formats
    {
        V4L2_PIX_FMT_YUV420,  V4L2_PIX_FMT_YVU420, V4L2_PIX_FMT_YUV420M,
        V4L2_PIX_FMT_YVU420M, V4L2_PIX_FMT_NV12,   V4L2_PIX_FMT_NV12M,
        V4L2_PIX_FMT_NV21,    V4L2_PIX_FMT_NV21M,  V4L2_PIX_FMT_NV12_COL128,
	V4L2_PIX_FMT_NV12_10_COL128
    };

    V4L2Profiles result;

    const QString root("/dev/");
    QDir dir(root);
    QStringList namefilters;
    namefilters.append("video*");
    auto devices = dir.entryList(namefilters, QDir::Files |QDir::System);
    for (const QString& device : std::as_const(devices))
    {
        V4L2util v4l2dev(root + device);
        uint32_t caps = v4l2dev.GetCapabilities();
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Device: %1 Driver: '%2' Capabilities: 0x%3")
            .arg(v4l2dev.GetDeviceName(), v4l2dev.GetDriverName(), QString::number(caps, 16)));

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
        QSize dummy{0, 0};

        for (const auto & profile : Profiles)
        {
            bool found = false;
            uint32_t v4l2pixfmt = profile.first;
            auto mythprofile = profile.second;
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
                    if (std::find(s_formats.cbegin(), s_formats.cend(), fdesc.pixelformat) != s_formats.cend())
                    {
                        if (!result.contains(mythprofile))
                            result.append(mythprofile);
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
                        .arg(MythCodecContext::GetProfileDescription(mythprofile, dummy), pixformats.join((","))));
                }
            }
        }
    }

    return result;
}

void MythV4L2M2MContext::GetDecoderList(QStringList &Decoders)
{
    const auto & profiles = MythV4L2M2MContext::GetStandardProfiles();
    if (!profiles.isEmpty())
    {
        QSize size(0, 0);
        Decoders.append("V4L2:");
        for (MythCodecContext::CodecProfile profile : profiles)
            Decoders.append(MythCodecContext::GetProfileDescription(profile, size));
    }

    const V4L2Profiles& requests = MythV4L2M2MContext::GetRequestProfiles();
    if (!requests.isEmpty())
    {
        QSize size(0, 0);
        Decoders.append("V4L2 Request:");
        for (MythCodecContext::CodecProfile profile : requests)
            Decoders.append(MythCodecContext::GetProfileDescription(profile, size));
    }

}

bool MythV4L2M2MContext::HaveV4L2Codecs(bool Reinit /*=false*/)
{
    static QRecursiveMutex lock;
    QMutexLocker locker(&lock);
    static bool s_checked = false;
    static bool s_available = false;

    if (s_checked && !Reinit)
        return s_available;
    s_checked = true;

    const auto & standard = MythV4L2M2MContext::GetStandardProfiles();
    const auto & request  = MythV4L2M2MContext::GetRequestProfiles();
    if (standard.isEmpty() && request.isEmpty())
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "No V4L2 decoders found");
        return s_available;
    }

    LOG(VB_GENERAL, LOG_INFO, LOC + "Supported/available V4L2 decoders:");
    s_available = true;
    QSize size {0, 0};
    for (auto profile : std::as_const(standard))
        LOG(VB_GENERAL, LOG_INFO, LOC + MythCodecContext::GetProfileDescription(profile, size));
    for (auto profile : std::as_const(request))
        LOG(VB_GENERAL, LOG_INFO, LOC + MythCodecContext::GetProfileDescription(profile, size) + "(Request)");
    return s_available;
}

#ifndef V4L2_PIX_FMT_MPEG2_SLICE
#define V4L2_PIX_FMT_MPEG2_SLICE v4l2_fourcc('M', 'G', '2', 'S')
#endif

#ifndef V4L2_PIX_FMT_H264_SLICE
#define V4L2_PIX_FMT_H264_SLICE v4l2_fourcc('S', '2', '6', '4')
#endif

#ifndef V4L2_PIX_FMT_VP8_FRAME
#define V4L2_PIX_FMT_VP8_FRAME v4l2_fourcc('V', 'P', '8', 'F')
#endif

#ifndef V4L2_PIX_FMT_VP9_FRAME
#define V4L2_PIX_FMT_VP9_FRAME v4l2_fourcc('V', 'P', '9', 'F')
#endif

#ifndef V4L2_PIX_FMT_HEVC_SLICE
#define V4L2_PIX_FMT_HEVC_SLICE v4l2_fourcc('S', '2', '6', '5')
#endif

const V4L2Profiles& MythV4L2M2MContext::GetRequestProfiles()
{
    static const std::vector<V4L2Mapping> s_map
    {{
        { V4L2_PIX_FMT_MPEG2_SLICE, MythCodecContext::MPEG2 },
        { V4L2_PIX_FMT_H264_SLICE,  MythCodecContext::H264  },
        { V4L2_PIX_FMT_VP8_FRAME,   MythCodecContext::VP8   },
        { V4L2_PIX_FMT_VP9_FRAME,   MythCodecContext::VP9   },
        { V4L2_PIX_FMT_HEVC_SLICE,  MythCodecContext::HEVC  }
    }};

    static QRecursiveMutex lock;
    static bool s_initialised = false;
    static V4L2Profiles s_profiles;

    QMutexLocker locker(&lock);
    if (!s_initialised)
        s_profiles = GetProfiles(s_map);
    s_initialised = true;
    return s_profiles;
}

AVPixelFormat MythV4L2M2MContext::GetV4L2RequestFormat(AVCodecContext *Context, const AVPixelFormat *PixFmt)
{
    while (*PixFmt != AV_PIX_FMT_NONE)
    {
        if (*PixFmt == AV_PIX_FMT_DRM_PRIME)
        {
            if (MythCodecContext::InitialiseDecoder2(Context, MythV4L2M2MContext::InitialiseV4L2RequestContext,
                                                     "V4L2 request context creation") >= 0)
            {
                return AV_PIX_FMT_DRM_PRIME;
            }
        }
        PixFmt++;
    }
    return AV_PIX_FMT_NONE;
}

int MythV4L2M2MContext::InitialiseV4L2RequestContext(AVCodecContext *Context)
{
    if (!Context || !gCoreContext->IsUIThread())
        return -1;

    // N.B. Interop support should already have been checked
    // Create the device context
    auto * hwdeviceref = MythCodecContext::CreateDevice(AV_HWDEVICE_TYPE_DRM, nullptr);
    if (!hwdeviceref)
        return -1;

    Context->hw_device_ctx = hwdeviceref;
    return 0;
}
