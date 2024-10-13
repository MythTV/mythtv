// Qt
#include <QtGlobal>
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
#include <QtAndroidExtras>
#include <QAndroidJniEnvironment>
#else
#include <QCoreApplication>
#include <QJniEnvironment>
#include <QJniObject>
#define QAndroidJniEnvironment QJniEnvironment
#define QAndroidJniObject QJniObject
#endif

// MythTV
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "libmythui/mythmainwindow.h"

#include "avformatdecoder.h"
#include "mythmediacodeccontext.h"
#include "mythplayerui.h"
#include "opengl/mythmediacodecinterop.h"

// FFmpeg
extern "C" {
#include "libavutil/pixfmt.h"
#include "libavutil/hwcontext_mediacodec.h"
#include "libavcodec/mediacodec.h"
#include "libavcodec/avcodec.h"
}

#define LOC QString("MediaCodec: ")

// MedicaCodec profile constants from MediaCodecInfo.CodecProfileLevel
#define MC_MPEG2_SIMPLE           (0x0)
#define MC_MPEG2_MAIN             (0x1)
#define MC_MPEG2_422              (0x2)
#define MC_MPEG2_SNR              (0x3)
#define MC_MPEG2_SPATIAL          (0x4)
#define MC_MPEG2_HIGH             (0x5)
#define MC_MPEG4_SIMPLE           (0x0001)
#define MC_MPEG4_SIMPLE_SCALEABLE (0x0002)
#define MC_MPEG4_CORE             (0x0004)
#define MC_MPEG4_MAIN             (0x0008)
#define MC_MPEG4_NBIT             (0x0010)
#define MC_MPEG4_SCALEABLE_TEX    (0x0020)
#define MC_MPEG4_SIMPLE_FACE      (0x0040)
#define MC_MPEG4_SIMPLE_FBA       (0x0080)
#define MC_MPEG4_BASIC_ANIMATED   (0x0100)
#define MC_MPEG4_HYBRID           (0x0200)
#define MC_MPEG4_ADV_REALTIME     (0x0400)
#define MC_MPEG4_CORE_SCALEABLE   (0x0800)
#define MC_MPEG4_ADV_CODING       (0x1000)
#define MC_MPEG4_ADV_CORE         (0x2000)
#define MC_MPEG4_ADV_SCALEABLE    (0x4000)
#define MC_MPEG4_ADV_SIMPLE       (0x8000)
#define MC_H264_BASELINE          (0x00001)
#define MC_H264_MAIN              (0x00002)
#define MC_H264_EXTENDED          (0x00004)
#define MC_H264_HIGH              (0x00008)
#define MC_H264_HIGH10            (0x00010)
#define MC_H264_HIGH422           (0x00020)
#define MC_H264_HIGH444           (0x00040)
#define MC_H264_CONST_BASELINE    (0x10000)
#define MC_H264_CONST_HIGH        (0x80000)
#define MC_HEVC_MAIN              (0x0001)
#define MC_HEVC_MAIN10            (0x0002)
#define MC_HEVC_MAIN_STILL        (0x0004)
#define MC_HEVC_MAIN10HDR10       (0x1000)
#define MC_HEVC_MMAIN10HDR10PLUS  (0x2000)
#define MC_VP8_MAIN               (0x0001)
#define MC_VP9_0                  (0x0001)
#define MC_VP9_1                  (0x0002)
#define MC_VP9_2                  (0x0004)
#define MC_VP9_3                  (0x0008)
#define MC_VP9_2HDR               (0x1000)
#define MC_VP9_3HDR               (0x2000)
#define MC_VP9_2HDRPLUS           (0x4000)
#define MC_VP9_3HDRPLUS           (0x8000)

inline MythCodecContext::CodecProfile MediaCodecToMythProfile(int Codec, int Profile)
{
    if (Codec == MythCodecContext::MPEG2)
    {
        switch (Profile)
        {
            case MC_MPEG2_SIMPLE:           return MythCodecContext::MPEG2Simple;
            case MC_MPEG2_MAIN:             return MythCodecContext::MPEG2Main;
            case MC_MPEG2_422:              return MythCodecContext::MPEG2422;
            case MC_MPEG2_SNR:              return MythCodecContext::MPEG2SNR;
            case MC_MPEG2_SPATIAL:          return MythCodecContext::MPEG2Spatial;
            case MC_MPEG2_HIGH:             return MythCodecContext::MPEG2High;
            default: return MythCodecContext::MPEG2;
        }
    }

    if (Codec == MythCodecContext::MPEG4)
    {
        switch (Profile)
        {
            case MC_MPEG4_SIMPLE:           return MythCodecContext::MPEG4Simple;
            case MC_MPEG4_SIMPLE_SCALEABLE: return MythCodecContext::MPEG4SimpleScaleable;
            case MC_MPEG4_CORE:             return MythCodecContext::MPEG4Core;
            case MC_MPEG4_MAIN:             return MythCodecContext::MPEG4Main;
            case MC_MPEG4_NBIT:             return MythCodecContext::MPEG4NBit;
            case MC_MPEG4_SCALEABLE_TEX:    return MythCodecContext::MPEG4ScaleableTexture;
            case MC_MPEG4_SIMPLE_FACE:      return MythCodecContext::MPEG4SimpleFace;
            case MC_MPEG4_SIMPLE_FBA:       return MythCodecContext::MPEG4SimpleStudio; // Is this correct?
            case MC_MPEG4_BASIC_ANIMATED:   return MythCodecContext::MPEG4BasicAnimated;
            case MC_MPEG4_HYBRID:           return MythCodecContext::MPEG4Hybrid;
            case MC_MPEG4_ADV_REALTIME:     return MythCodecContext::MPEG4AdvancedRT;
            case MC_MPEG4_CORE_SCALEABLE:   return MythCodecContext::MPEG4CoreScaleable;
            case MC_MPEG4_ADV_CODING:       return MythCodecContext::MPEG4AdvancedCoding;
            case MC_MPEG4_ADV_CORE:         return MythCodecContext::MPEG4AdvancedCore;
            case MC_MPEG4_ADV_SCALEABLE:    return MythCodecContext::MPEG4AdvancedScaleableTexture;
            case MC_MPEG4_ADV_SIMPLE:       return MythCodecContext::MPEG4AdvancedSimple;
            default: return MythCodecContext::MPEG4;
        }
    }

    if (Codec == MythCodecContext::H264)
    {
        switch (Profile)
        {
            case MC_H264_BASELINE:       return MythCodecContext::H264Baseline;
            case MC_H264_MAIN:           return MythCodecContext::H264Main;
            case MC_H264_EXTENDED:       return MythCodecContext::H264Extended;
            case MC_H264_HIGH:           return MythCodecContext::H264High;
            case MC_H264_HIGH10:         return MythCodecContext::H264High10;
            case MC_H264_HIGH422:        return MythCodecContext::H264High422;
            case MC_H264_HIGH444:        return MythCodecContext::H264High444;
            case MC_H264_CONST_BASELINE: return MythCodecContext::H264ConstrainedBaseline;
            case MC_H264_CONST_HIGH:     return MythCodecContext::H264ConstrainedHigh;
            default: return MythCodecContext::H264;
        }
    }

    if (Codec == MythCodecContext::HEVC)
    {
        switch (Profile)
        {
            case MC_HEVC_MAIN:              return MythCodecContext::HEVCMain;
            case MC_HEVC_MAIN10:            return MythCodecContext::HEVCMain10;
            case MC_HEVC_MAIN_STILL:        return MythCodecContext::HEVCMainStill;
            case MC_HEVC_MAIN10HDR10:       return MythCodecContext::HEVCMain10HDR;
            case MC_HEVC_MMAIN10HDR10PLUS:  return MythCodecContext::HEVCMain10HDRPlus;
            default: return MythCodecContext::HEVC;
        }
    }

    if (Codec == MythCodecContext::VP8)
        return MythCodecContext::VP8;

    if (Codec == MythCodecContext::VP9)
    {
        switch (Profile)
        {
            case MC_VP9_0:        return MythCodecContext::VP9_0;
            case MC_VP9_1:        return MythCodecContext::VP9_1;
            case MC_VP9_2:        return MythCodecContext::VP9_2;
            case MC_VP9_3:        return MythCodecContext::VP9_3;
            case MC_VP9_2HDR:     return MythCodecContext::VP9_2HDR;
            case MC_VP9_3HDR:     return MythCodecContext::VP9_3HDR;
            case MC_VP9_2HDRPLUS: return MythCodecContext::VP9_2HDRPlus;
            case MC_VP9_3HDRPLUS: return MythCodecContext::VP9_3HDRPlus;
            default: return MythCodecContext::VP9;
        }
    }

    return MythCodecContext::NoProfile;
}

int MythMediaCodecContext::InitialiseDecoder(AVCodecContext *Context)
{
    if (!Context || !gCoreContext->IsUIThread())
        return -1;

    // The interop must have a reference to the ui player so it can be deleted
    // from the main thread.
    auto * player = GetPlayerUI(Context);
    if (!player)
        return -1;

    // Retrieve OpenGL render context
    auto * render = dynamic_cast<MythRenderOpenGL*>(player->GetRender());
    if (!render)
        return -1;
    OpenGLLocker locker(render);

    // Create interop - NB no interop check here or in MythMediaCodecInterop
    QSize size(Context->width, Context->height);
    auto * interop = MythMediaCodecInterop::CreateMediaCodec(player, render, size);
    if (!interop)
        return -1;
    if (!interop->GetSurface())
    {
        interop->DecrRef();
        return -1;
    }

    // Create the hardware context
    AVBufferRef *hwdeviceref = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_MEDIACODEC);
    AVHWDeviceContext *hwdevicectx = reinterpret_cast<AVHWDeviceContext*>(hwdeviceref->data);
    hwdevicectx->free = &MythCodecContext::DeviceContextFinished;
    hwdevicectx->user_opaque = interop;
    AVMediaCodecDeviceContext *hwctx = reinterpret_cast<AVMediaCodecDeviceContext*>(hwdevicectx->hwctx);
    hwctx->surface = interop->GetSurface();
    if (av_hwdevice_ctx_init(hwdeviceref) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "av_hwdevice_ctx_init failed");
        av_buffer_unref(&hwdeviceref);
        return -1;
    }

    Context->hw_device_ctx = hwdeviceref;
    LOG(VB_GENERAL, LOG_INFO, LOC + "Created MediaCodec hardware device context");
    return 0;
}

MythCodecID MythMediaCodecContext::GetBestSupportedCodec(AVCodecContext **Context,
                                                         const AVCodec **Codec,
                                                         const QString  &Decoder,
                                                         AVStream       *Stream,
                                                         uint            StreamType)
{
    bool decodeonly = Decoder == "mediacodec-dec";
    MythCodecID success = static_cast<MythCodecID>((decodeonly ? kCodec_MPEG1_MEDIACODEC_DEC : kCodec_MPEG1_MEDIACODEC) + (StreamType - 1));
    MythCodecID failure = static_cast<MythCodecID>(kCodec_MPEG1 + (StreamType - 1));

    if (!Decoder.startsWith("mediacodec"))
        return failure;

    if (!HaveMediaCodec())
        return failure;

    if (!decodeonly)
        if (!FrameTypeIsSupported(*Context, FMT_MEDIACODEC))
            return failure;

    bool found = false;
    MCProfiles& profiles = MythMediaCodecContext::GetProfiles();
    MythCodecContext::CodecProfile mythprofile =
            MythCodecContext::FFmpegToMythProfile((*Context)->codec_id, (*Context)->profile);
    for (auto profile : std::as_const(profiles))
    {
        if (profile.first == mythprofile &&
            profile.second.width() >= (*Context)->width &&
            profile.second.height() >= (*Context)->height)
        {
            found = true;
            break;
        }
    }

    AvFormatDecoder *decoder = dynamic_cast<AvFormatDecoder*>(reinterpret_cast<DecoderBase*>((*Context)->opaque));
    QString profilestr = MythCodecContext::GetProfileDescription(mythprofile, QSize());
    if (found && decoder)
    {
        QString decodername = QString((*Codec)->name) + "_mediacodec";
        if (decodername == "mpeg2video_mediacodec")
            decodername = "mpeg2_mediacodec";
        const AVCodec *newCodec = avcodec_find_decoder_by_name (decodername.toLocal8Bit());
        if (newCodec)
        {
            *Codec = newCodec;
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("HW device type '%1' supports decoding '%2' (%3)")
                    .arg(av_hwdevice_get_type_name(AV_HWDEVICE_TYPE_MEDIACODEC)).arg((*Codec)->name).arg(profilestr));
            decoder->CodecMap()->FreeCodecContext(Stream);
            *Context = decoder->CodecMap()->GetCodecContext(Stream, *Codec);
            (*Context)->pix_fmt = AV_PIX_FMT_MEDIACODEC;
            return success;
        }
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("HW device type '%1' does not support decoding '%2' (%3)")
            .arg(av_hwdevice_get_type_name(AV_HWDEVICE_TYPE_MEDIACODEC)).arg((*Codec)->name).arg(profilestr));
    return failure;
}

MythMediaCodecContext::MythMediaCodecContext(DecoderBase *Parent, MythCodecID CodecID)
  : MythCodecContext(Parent, CodecID)
{
}

void MythMediaCodecContext::InitVideoCodec(AVCodecContext *Context, bool SelectedStream, bool &DirectRendering)
{
    if (CODEC_IS_MEDIACODEC(Context->codec))
    {
        Context->get_format = MythMediaCodecContext::GetFormat;
        Context->slice_flags = SLICE_FLAG_CODED_ORDER | SLICE_FLAG_ALLOW_FIELD;
        DirectRendering = false;
        return;
    }

    MythCodecContext::InitVideoCodec(Context, SelectedStream, DirectRendering);
}

int MythMediaCodecContext::HwDecoderInit(AVCodecContext *Context)
{
    if (codec_is_mediacodec_dec(m_codecID))
        return 0;
    else if (codec_is_mediacodec(m_codecID))
        return MythCodecContext::InitialiseDecoder2(Context, MythMediaCodecContext::InitialiseDecoder, "Create MediaCodec decoder");
    return -1;
}

bool MythMediaCodecContext::RetrieveFrame(AVCodecContext *Context, MythVideoFrame *Frame, AVFrame *AvFrame)
{
    if (AvFrame->format != AV_PIX_FMT_MEDIACODEC)
        return false;
    if (codec_is_mediacodec(m_codecID))
        return GetBuffer2(Context, Frame, AvFrame, 0);
    return false;
}

AVPixelFormat MythMediaCodecContext::GetFormat(AVCodecContext*, const AVPixelFormat *PixFmt)
{
    while (*PixFmt != AV_PIX_FMT_NONE)
    {
        if (*PixFmt == AV_PIX_FMT_MEDIACODEC)
            return AV_PIX_FMT_MEDIACODEC;
        PixFmt++;
    }
    return AV_PIX_FMT_NONE;
}

/*! \brief Mark all MediaCodec decoded frames as progressive,
 *
 * \note This may not be appropriate for all devices
*/
void MythMediaCodecContext::PostProcessFrame(AVCodecContext*, MythVideoFrame* Frame)
{
    if (!Frame)
        return;

    Frame->m_deinterlaceInuse = DEINT_BASIC | DEINT_DRIVER;
    Frame->m_deinterlaceInuse2x = false;
    Frame->m_interlaced = false;
    Frame->m_interlacedReverse = false;
    Frame->m_topFieldFirst = false;
    Frame->m_deinterlaceAllowed = DEINT_NONE;
    Frame->m_alreadyDeinterlaced = true;
}

/*! /brief Say yes
 *
 * \note As for PostProcessFrame this may not be accurate.
*/
bool MythMediaCodecContext::IsDeinterlacing(bool &DoubleRate, bool)
{
    DoubleRate = true;
    return true;
}

MCProfiles &MythMediaCodecContext::GetProfiles(void)
{
    // TODO Something tells me this is leakier than a leaky thing
    static QRecursiveMutex lock;
    static bool s_initialised = false;
    static MCProfiles s_profiles;

    QMutexLocker locker(&lock);
    if (s_initialised)
        return s_profiles;
    s_initialised = true;

    static const QPair<QString,QPair<MythCodecContext::CodecProfile, QList<int> > > mimetypes[] =
    {
        { "video/mpeg2", { MythCodecContext::MPEG2,
          { MC_MPEG2_SIMPLE, MC_MPEG2_MAIN, MC_MPEG2_422, MC_MPEG2_SNR,
            MC_MPEG2_SPATIAL, MC_MPEG2_HIGH}}},
        { "video/mp4v-es", { MythCodecContext::MPEG4,
          { MC_MPEG4_SIMPLE, MC_MPEG4_SIMPLE_SCALEABLE, MC_MPEG4_CORE,
            MC_MPEG4_MAIN, MC_MPEG4_NBIT, MC_MPEG4_SCALEABLE_TEX,
            MC_MPEG4_SIMPLE_FACE, MC_MPEG4_SIMPLE_FBA, MC_MPEG4_BASIC_ANIMATED,
            MC_MPEG4_HYBRID, MC_MPEG4_ADV_REALTIME, MC_MPEG4_CORE_SCALEABLE,
            MC_MPEG4_ADV_CODING, MC_MPEG4_ADV_CORE, MC_MPEG4_ADV_SCALEABLE,
            MC_MPEG4_ADV_SIMPLE}}},
        { "video/avc", { MythCodecContext::H264,
          { MC_H264_BASELINE, MC_H264_MAIN, MC_H264_EXTENDED, MC_H264_HIGH,
            MC_H264_HIGH10, MC_H264_HIGH422, MC_H264_HIGH444,
            MC_H264_CONST_BASELINE, MC_H264_CONST_HIGH}}},
        { "video/hevc", { MythCodecContext::HEVC,
          { MC_HEVC_MAIN, MC_HEVC_MAIN10, MC_HEVC_MAIN_STILL,
            MC_HEVC_MAIN10HDR10, MC_HEVC_MMAIN10HDR10PLUS}}},
        { "video/x-vnd.on2.vp8", { MythCodecContext::VP8, { MC_VP8_MAIN }}},
        { "video/x-vnd.on2.vp9", { MythCodecContext::VP9,
          { MC_VP9_0, MC_VP9_1, MC_VP9_2, MC_VP9_3,
            MC_VP9_2HDR, MC_VP9_3HDR, MC_VP9_2HDRPLUS, MC_VP9_3HDRPLUS }}}
        //{ "video/vc1",         { MythCodecContext::VC1  , {}}}, // No FFmpeg support
        //{ "video/3gpp",        { MythCodecContext::H263 , {}}}, // No FFmpeg support
        //{ "video/av01",        { MythCodecContext::AV1  , {}}}  // No FFmpeg support, API Level 29
    };

    // Retrieve MediaCodecList
    QAndroidJniEnvironment env;
    QAndroidJniObject list("android/media/MediaCodecList", "(I)V", 0); // 0 = REGULAR_CODECS
    if (!list.isValid())
        return s_profiles;
    // Retrieve array of MediaCodecInfo's
    QAndroidJniObject qtcodecs = list.callObjectMethod("getCodecInfos", "()[Landroid/media/MediaCodecInfo;");
    if (!qtcodecs.isValid())
        return s_profiles;

    // Iterate over MediaCodecInfo's
    jobjectArray codecs = qtcodecs.object<jobjectArray>();
    jsize codeccount = env->GetArrayLength(codecs);
    for (jsize i = 0; i < codeccount; ++i)
    {
        QAndroidJniObject codec(env->GetObjectArrayElement(codecs, i));
        if (!codec.isValid())
            continue;

        // Ignore encoders
        if (codec.callMethod<jboolean>("isEncoder"))
            continue;

        // Ignore software decoders
        QString name = codec.callObjectMethod<jstring>("getName").toString();
        if (name.contains("OMX.google", Qt::CaseInsensitive))
            continue;

        // Retrieve supported mimetypes (there is usually just one)
        QAndroidJniObject qttypes = codec.callObjectMethod("getSupportedTypes", "()[Ljava/lang/String;");
        jobjectArray types = qttypes.object<jobjectArray>();
        jsize typecount = env->GetArrayLength(types);
        for (jsize j = 0; j < typecount; ++j)
        {
            QAndroidJniObject type(env->GetObjectArrayElement(types, j));
            if (!type.isValid())
                continue;

            // Match mimetype to types supported by FFmpeg
            QString typestr = type.toString();
            for (auto mimetype : mimetypes)
            {
                if (mimetype.first != typestr)
                    continue;

                // Retrieve MediaCodecInfo.CodecCapabilities for mimetype
                QAndroidJniObject caps = codec.callObjectMethod("getCapabilitiesForType",
                        "(Ljava/lang/String;)Landroid/media/MediaCodecInfo$CodecCapabilities;",
                        type.object<jstring>());
                if (!caps.isValid())
                    continue;

                // Retrieve MediaCodecInfo.VideoCapabilities from CodecCapabilities
                QAndroidJniObject videocaps = caps.callObjectMethod("getVideoCapabilities",
                        "()Landroid/media/MediaCodecInfo$VideoCapabilities;");
                QAndroidJniObject widthrange = videocaps.callObjectMethod("getSupportedWidths",
                        "()Landroid/util/Range;");
                QAndroidJniObject heightrange = videocaps.callObjectMethod("getSupportedHeights",
                        "()Landroid/util/Range;");

                QAndroidJniObject widthqt = widthrange.callObjectMethod("getUpper", "()Ljava/lang/Comparable;");
                QAndroidJniObject heightqt = heightrange.callObjectMethod("getUpper", "()Ljava/lang/Comparable;");
                int width  = widthqt.callMethod<jint>("intValue", "()I");
                int height = heightqt.callMethod<jint>("intValue", "()I");

                // Profiles are available from CodecCapabilities.profileLevel field
                QAndroidJniObject profiles = caps.getObjectField("profileLevels",
                        "[Landroid/media/MediaCodecInfo$CodecProfileLevel;");
                jobjectArray profilearr = profiles.object<jobjectArray>();
                jsize profilecount = env->GetArrayLength(profilearr);
                if (profilecount < 1)
                {
                    s_profiles.append(QPair<MythCodecContext::CodecProfile,QSize>(mimetype.second.first, QSize(width, height)));
                    continue;
                }

                for (jsize k = 0; k < profilecount; ++k)
                {
                    jobject profile = env->GetObjectArrayElement(profilearr, k);
                    jclass objclass = env->GetObjectClass(profile);
                    jfieldID id     = env->GetFieldID(objclass, "profile", "I");
                    int value       = static_cast<int>(env->GetIntField(profile, id));
                    QList<int>& mcprofiles = mimetype.second.second;
                    auto sameprof = [value](auto mcprofile) { return value == mcprofile; };
                    if (std::any_of(mcprofiles.cbegin(), mcprofiles.cend(), sameprof))
                    {
                        MythCodecContext::CodecProfile p = MediaCodecToMythProfile(mimetype.second.first, value);
                        s_profiles.append(QPair<MythCodecContext::CodecProfile,QSize>(p, QSize(width, height)));
                    }
                    else
                        s_profiles.append(QPair<MythCodecContext::CodecProfile,QSize>(mimetype.second.first, QSize(width, height)));
                }
            }
        }
    }

    return s_profiles;
}

void MythMediaCodecContext::GetDecoderList(QStringList &Decoders)
{
    MCProfiles& profiles = MythMediaCodecContext::GetProfiles();
    if (profiles.isEmpty())
        return;

    Decoders.append("MediaCodec:");
    for (auto profile : std::as_const(profiles))
        Decoders.append(MythCodecContext::GetProfileDescription(profile.first, profile.second));
}

bool MythMediaCodecContext::HaveMediaCodec(bool Reinit /*=false*/)
{
    static QRecursiveMutex lock;
    static bool s_initialised = false;
    static bool s_available   = false;

    QMutexLocker locker(&lock);
    if (!s_initialised || Reinit)
    {
        MCProfiles& profiles = MythMediaCodecContext::GetProfiles();
        if (profiles.isEmpty())
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + "No MediaCodec decoders found");
        }
        else
        {
            s_available = true;
            LOG(VB_GENERAL, LOG_INFO, LOC + "Supported/available MediaCodec decoders:");
            for (auto profile : std::as_const(profiles))
            {
                LOG(VB_GENERAL, LOG_INFO, LOC +
                    MythCodecContext::GetProfileDescription(profile.first, profile.second));
            }
        }
    }
    s_initialised = true;
    return s_available;
}

