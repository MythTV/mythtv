// MythTV
#include "libmythbase/mythlogging.h"
#include "mythvideocolourspace.h"
#include "mythvdpauhelper.h"
#include "libmythui/platforms/mythxdisplay.h" // always last

// Std
#include <cmath>

#define LOC QString("VDPAUHelp: ")

#define INIT_ST \
VdpStatus status; \
bool ok = true;

// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define CHECK_ST \
ok &= (status == VDP_STATUS_OK); \
if (!ok) \
{ \
  LOG(VB_PLAYBACK, LOG_ERR, LOC + QString("Error at %1:%2 (#%3, %4)") \
          .arg(__FILE__).arg( __LINE__).arg(status) \
          .arg(m_vdpGetErrorString(status))); \
}

#define GET_PROC(FUNC_ID, PROC) \
status = m_vdpGetProcAddress(m_device, FUNC_ID, reinterpret_cast<void **>(&(PROC))); CHECK_ST
// NOLINTEND(cppcoreguidelines-macro-usage)

VDPAUCodec::VDPAUCodec(MythCodecContext::CodecProfile Profile, QSize Size, uint32_t Macroblocks, uint32_t Level)
  : m_maxSize(Size),
    m_maxMacroBlocks(Macroblocks),
    m_maxLevel(Level)
{
    // Levels don't work for MPEG1/2
    if (MythCodecContext::MPEG1 <= Profile && Profile <= MythCodecContext::MPEG2SNR)
        m_maxLevel = 1000;
}

bool VDPAUCodec::Supported(int Width, int Height, int Level) const
{
    // Note - level checks are now ignored here and in FFmpeg
    uint32_t macros = static_cast<uint32_t>(((Width + 15) & ~15) * ((Height + 15) & ~15)) / 256;
    bool result = (Width <= m_maxSize.width()) && (Height <= m_maxSize.height()) &&
                  (macros <= m_maxMacroBlocks) /*&& (static_cast<uint32_t>(Level) <= m_maxLevel)*/;
    if (!result)
    {
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("Not supported: Size %1x%2 > %3x%4, MBs %5 > %6, Level %7 > %8")
                .arg(Width).arg(Height).arg(m_maxSize.width()).arg(m_maxSize.height())
                .arg(macros).arg(m_maxMacroBlocks).arg(Level).arg(m_maxLevel));
    }
    return result;
}

bool MythVDPAUHelper::HaveVDPAU(bool Reinit /*=false*/)
{
    static QMutex s_mutex;
    static bool s_checked = false;
    static bool s_available = false;

    QMutexLocker locker(&s_mutex);
    if (s_checked && !Reinit)
        return s_available;

    {
        MythVDPAUHelper vdpau;
        s_available = vdpau.IsValid();
    }

    s_checked = true;
    if (s_available)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "Supported/available VDPAU decoders:");
        const VDPAUProfiles& profiles = MythVDPAUHelper::GetProfiles();
        for (const auto& profile : std::as_const(profiles))
            LOG(VB_GENERAL, LOG_INFO, LOC +
                MythCodecContext::GetProfileDescription(profile.first, profile.second.m_maxSize));
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "VDPAU is NOT available");
    }
    return s_available;
}

bool MythVDPAUHelper::ProfileCheck(VdpDecoderProfile Profile, uint32_t &Level,
                                   uint32_t &Macros, uint32_t &Width, uint32_t &Height)
{
    if (!m_device)
        return false;

    INIT_ST
    VdpBool supported = VDP_FALSE;
    status = m_vdpDecoderQueryCapabilities(m_device, Profile, &supported,
                                           &Level, &Macros, &Width, &Height);
    CHECK_ST

    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("ProfileCheck: Prof %1 Supp %2 Level %3 Macros %4 Width %5 Height %6 Status %7")
        .arg(Profile).arg(supported).arg(Level).arg(Macros).arg(Width).arg(Height).arg(status));

    if (((supported != VDP_TRUE) || (status != VDP_STATUS_OK)) &&
        (Profile == VDP_DECODER_PROFILE_H264_CONSTRAINED_BASELINE ||
         Profile == VDP_DECODER_PROFILE_H264_BASELINE))
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Driver does not report support for H264 %1Baseline")
            .arg(Profile == VDP_DECODER_PROFILE_H264_CONSTRAINED_BASELINE ? "Constrained " : ""));

        // H264 Constrained baseline is reported as not supported on older chipsets but
        // works due to support for H264 Main. Test for H264 main if constrained baseline
        // fails - which mimics the fallback in FFmpeg.
        // Updated to included baseline... not so sure about that:)
        status = m_vdpDecoderQueryCapabilities(m_device, VDP_DECODER_PROFILE_H264_MAIN, &supported,
                                               &Level, &Macros, &Width, &Height);
        CHECK_ST
        if (supported == VDP_TRUE)
            LOG(VB_GENERAL, LOG_INFO, LOC + "... but assuming available as H264 Main is supported");
    }

    return supported == VDP_TRUE;
}

const VDPAUProfiles& MythVDPAUHelper::GetProfiles(void)
{
    static const std::array<const VdpDecoderProfile,15> MainProfiles
    {
        VDP_DECODER_PROFILE_MPEG1, VDP_DECODER_PROFILE_MPEG2_SIMPLE, VDP_DECODER_PROFILE_MPEG2_MAIN,
        VDP_DECODER_PROFILE_MPEG4_PART2_SP, VDP_DECODER_PROFILE_MPEG4_PART2_ASP,
        VDP_DECODER_PROFILE_VC1_SIMPLE, VDP_DECODER_PROFILE_VC1_MAIN, VDP_DECODER_PROFILE_VC1_ADVANCED,
        VDP_DECODER_PROFILE_H264_BASELINE, VDP_DECODER_PROFILE_H264_MAIN, VDP_DECODER_PROFILE_H264_HIGH,
        VDP_DECODER_PROFILE_H264_EXTENDED, VDP_DECODER_PROFILE_H264_CONSTRAINED_BASELINE,
        VDP_DECODER_PROFILE_H264_CONSTRAINED_HIGH, VDP_DECODER_PROFILE_H264_HIGH_444_PREDICTIVE
    };

    static const std::array<const VdpDecoderProfile,4> HEVCProfiles
    {
        VDP_DECODER_PROFILE_HEVC_MAIN, VDP_DECODER_PROFILE_HEVC_MAIN_10,
        VDP_DECODER_PROFILE_HEVC_MAIN_STILL, VDP_DECODER_PROFILE_HEVC_MAIN_444
    };

    auto VDPAUToMythProfile = [](VdpDecoderProfile Profile)
    {
        switch (Profile)
        {
            case VDP_DECODER_PROFILE_MPEG1:           return MythCodecContext::MPEG1;
            case VDP_DECODER_PROFILE_MPEG2_SIMPLE:    return MythCodecContext::MPEG2Simple;
            case VDP_DECODER_PROFILE_MPEG2_MAIN:      return MythCodecContext::MPEG2Main;

            case VDP_DECODER_PROFILE_MPEG4_PART2_SP:  return MythCodecContext::MPEG4Simple;
            case VDP_DECODER_PROFILE_MPEG4_PART2_ASP: return MythCodecContext::MPEG4AdvancedSimple;

            case VDP_DECODER_PROFILE_VC1_SIMPLE:      return MythCodecContext::VC1Simple;
            case VDP_DECODER_PROFILE_VC1_MAIN:        return MythCodecContext::VC1Main;
            case VDP_DECODER_PROFILE_VC1_ADVANCED:    return MythCodecContext::VC1Advanced;

            case VDP_DECODER_PROFILE_H264_BASELINE:   return MythCodecContext::H264Baseline;
            case VDP_DECODER_PROFILE_H264_MAIN:       return MythCodecContext::H264Main;
            case VDP_DECODER_PROFILE_H264_HIGH:       return MythCodecContext::H264High;
            case VDP_DECODER_PROFILE_H264_EXTENDED:   return MythCodecContext::H264Extended;
            case VDP_DECODER_PROFILE_H264_CONSTRAINED_BASELINE: return MythCodecContext::H264ConstrainedBaseline;
            case VDP_DECODER_PROFILE_H264_CONSTRAINED_HIGH:     return MythCodecContext::H264ConstrainedHigh;
            case VDP_DECODER_PROFILE_H264_HIGH_444_PREDICTIVE:  return MythCodecContext::H264High444; // ?

            case VDP_DECODER_PROFILE_HEVC_MAIN:       return MythCodecContext::HEVCMain;
            case VDP_DECODER_PROFILE_HEVC_MAIN_10:    return MythCodecContext::HEVCMain10;
            case VDP_DECODER_PROFILE_HEVC_MAIN_STILL: return MythCodecContext::HEVCMainStill;
            case VDP_DECODER_PROFILE_HEVC_MAIN_444:   return MythCodecContext::HEVCRext;
        }
        return MythCodecContext::NoProfile;
    };

    static QRecursiveMutex lock;
    static bool s_initialised = false;
    static VDPAUProfiles s_profiles;

    QMutexLocker locker(&lock);
    if (s_initialised)
        return s_profiles;
    s_initialised = true;

    MythVDPAUHelper helper;
    if (!helper.IsValid())
        return s_profiles;

    uint32_t level  = 0;
    uint32_t macros = 0;
    uint32_t width  = 0;
    uint32_t height = 0;
    for (VdpDecoderProfile profile : MainProfiles)
    {
        if (helper.ProfileCheck(profile, level, macros, width, height))
        {
            MythCodecContext::CodecProfile prof = VDPAUToMythProfile(profile);
            s_profiles.emplace_back(prof,
                VDPAUCodec(prof, QSize(static_cast<int>(width), static_cast<int>(height)), macros, level));
        }
    }

    if (helper.HEVCSupported())
    {
        for (VdpDecoderProfile profile : HEVCProfiles)
        {
            if (helper.ProfileCheck(profile, level, macros, width, height))
            {
                MythCodecContext::CodecProfile prof = VDPAUToMythProfile(profile);
                s_profiles.emplace_back(prof,
                    VDPAUCodec(prof, QSize(static_cast<int>(width), static_cast<int>(height)), macros, level));
            }
        }
    }

    return s_profiles;
}

void MythVDPAUHelper::GetDecoderList(QStringList &Decoders)
{
    const VDPAUProfiles& profiles = MythVDPAUHelper::GetProfiles();
    if (profiles.empty())
        return;

    Decoders.append("VDPAU:");
    for (const auto& profile : std::as_const(profiles))
        if (profile.first != MythCodecContext::MJPEG)
            Decoders.append(MythCodecContext::GetProfileDescription(profile.first, profile.second.m_maxSize));
}

static void vdpau_preemption_callback(VdpDevice /*unused*/, void* Opaque)
{
    auto* helper = static_cast<MythVDPAUHelper*>(Opaque);
    if (helper)
        helper->SetPreempted();
}

/*! \class MythVDPAUHelper
 * \brief A simple wrapper around VDPAU functionality.
*/
MythVDPAUHelper::MythVDPAUHelper(AVVDPAUDeviceContext* Context)
  : m_device(Context->device),
    m_vdpGetProcAddress(Context->get_proc_address),
    m_valid(InitProcs())
{
    if (m_valid)
    {
        INIT_ST
        status = m_vdpPreemptionCallbackRegister(m_device, vdpau_preemption_callback, this);
        CHECK_ST
        if (!ok)
            LOG(VB_PLAYBACK, LOG_ERR, LOC + "Failed to register preemption callback");
    }
}

static const char* DummyGetError(VdpStatus /*status*/)
{
    return "Unknown";
}

MythVDPAUHelper::MythVDPAUHelper(void)
  : m_display(MythXDisplay::OpenMythXDisplay(false)),
    m_createdDevice(true)
{
    if (!m_display)
        return;

    INIT_ST
    m_vdpGetErrorString = &DummyGetError;
    m_display->Lock();
    status = vdp_device_create_x11(m_display->GetDisplay(),
                                   m_display->GetScreen(),
                                   &m_device, &m_vdpGetProcAddress);
    m_display->Unlock();
    CHECK_ST
    if (!ok)
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC + "Failed to create VDPAU device.");
        return;
    }
    m_valid = InitProcs();
}

MythVDPAUHelper::~MythVDPAUHelper(void)
{
    if (m_vdpPreemptionCallbackRegister)
        m_vdpPreemptionCallbackRegister(m_device, nullptr, nullptr);
    if (m_createdDevice && m_vdpDeviceDestroy)
        m_vdpDeviceDestroy(m_device);
    delete m_display;
}

bool MythVDPAUHelper::InitProcs(void)
{
    INIT_ST
    GET_PROC(VDP_FUNC_ID_GET_ERROR_STRING, m_vdpGetErrorString)
    if (!ok)
    {
        m_vdpGetErrorString = &DummyGetError;
        ok = true;
    }
    GET_PROC(VDP_FUNC_ID_GET_INFORMATION_STRING, m_vdpGetInformationString)
    GET_PROC(VDP_FUNC_ID_DEVICE_DESTROY, m_vdpDeviceDestroy)
    GET_PROC(VDP_FUNC_ID_DECODER_QUERY_CAPABILITIES, m_vdpDecoderQueryCapabilities)
    GET_PROC(VDP_FUNC_ID_DECODER_CREATE, m_vdpDecoderCreate)
    GET_PROC(VDP_FUNC_ID_DECODER_DESTROY, m_vdpDecoderDestroy)
    GET_PROC(VDP_FUNC_ID_VIDEO_MIXER_CREATE, m_vdpVideoMixerCreate)
    GET_PROC(VDP_FUNC_ID_VIDEO_MIXER_DESTROY, m_vdpVideoMixerDestroy)
    GET_PROC(VDP_FUNC_ID_VIDEO_MIXER_RENDER, m_vdpVideoMixerRender)
    GET_PROC(VDP_FUNC_ID_VIDEO_MIXER_SET_ATTRIBUTE_VALUES, m_vdpVideoMixerSetAttributeValues)
    GET_PROC(VDP_FUNC_ID_VIDEO_MIXER_SET_FEATURE_ENABLES, m_vdpVideoMixerSetFeatureEnables)
    GET_PROC(VDP_FUNC_ID_VIDEO_MIXER_QUERY_FEATURE_SUPPORT, m_vdpVideoMixerQueryFeatureSupport)
    GET_PROC(VDP_FUNC_ID_VIDEO_MIXER_QUERY_ATTRIBUTE_SUPPORT, m_vdpVideoMixerQueryAttributeSupport)
    GET_PROC(VDP_FUNC_ID_OUTPUT_SURFACE_CREATE, m_vdpOutputSurfaceCreate)
    GET_PROC(VDP_FUNC_ID_OUTPUT_SURFACE_DESTROY, m_vdpOutputSurfaceDestroy)
    GET_PROC(VDP_FUNC_ID_VIDEO_SURFACE_GET_PARAMETERS, m_vdpVideoSurfaceGetParameters)
    GET_PROC(VDP_FUNC_ID_PREEMPTION_CALLBACK_REGISTER, m_vdpPreemptionCallbackRegister)

    return ok;
}

bool MythVDPAUHelper::IsValid(void) const
{
    return m_valid;
}

void MythVDPAUHelper::SetPreempted(void)
{
    emit DisplayPreempted();
}

bool MythVDPAUHelper::HEVCSupported(void)
{
    if (!m_valid)
        return false;

    // FFmpeg will disallow HEVC VDPAU for driver versions < 410
    const char* infostring = nullptr;
    INIT_ST
    status = m_vdpGetInformationString(&infostring);
    CHECK_ST
    if (!ok || !infostring)
        return false;

    if (!QString(infostring).contains("NVIDIA", Qt::CaseInsensitive))
        return true;

    int driver = 0;
    sscanf(infostring, "NVIDIA VDPAU Driver Shared Library  %d", &driver);
    return !(driver < 410);
}

bool MythVDPAUHelper::CheckH264Decode(AVCodecContext *Context)
{
    if (!Context)
        return false;

    int mbs = static_cast<int>(ceil(static_cast<double>(Context->width) / 16.0));
    if (mbs != 49  && mbs != 54  && mbs != 59 && mbs != 64 &&
        mbs != 113 && mbs != 118 &&mbs != 123 && mbs != 128)
    {
        return true;
    }

    VdpDecoderProfile profile = 0;
    switch (Context->profile & ~FF_PROFILE_H264_INTRA)
    {
        case FF_PROFILE_H264_BASELINE: profile = VDP_DECODER_PROFILE_H264_BASELINE; break;
        case FF_PROFILE_H264_CONSTRAINED_BASELINE: profile = VDP_DECODER_PROFILE_H264_CONSTRAINED_BASELINE; break;
        case FF_PROFILE_H264_MAIN: profile = VDP_DECODER_PROFILE_H264_MAIN; break;
        case FF_PROFILE_H264_HIGH: profile = VDP_DECODER_PROFILE_H264_HIGH; break;
#ifdef VDP_DECODER_PROFILE_H264_EXTENDED
        case FF_PROFILE_H264_EXTENDED: profile = VDP_DECODER_PROFILE_H264_EXTENDED; break;
#endif
        case FF_PROFILE_H264_HIGH_10: profile = VDP_DECODER_PROFILE_H264_HIGH; break;
#ifdef VDP_DECODER_PROFILE_H264_HIGH_444_PREDICTIVE
        case FF_PROFILE_H264_HIGH_422:
        case FF_PROFILE_H264_HIGH_444_PREDICTIVE:
        case FF_PROFILE_H264_CAVLC_444: profile = VDP_DECODER_PROFILE_H264_HIGH_444_PREDICTIVE; break;
#endif
        default: return false;
    }

    // Create an instance
    MythVDPAUHelper helper;
    if (helper.IsValid())
        return helper.H264DecodeCheck(profile, Context);
    return false;
}

bool MythVDPAUHelper::H264DecodeCheck(VdpDecoderProfile Profile, AVCodecContext *Context)
{
    if (!m_valid || !Context)
        return false;

    INIT_ST
    VdpDecoder tmp = 0;
    status = m_vdpDecoderCreate(m_device, Profile, static_cast<uint>(Context->width),
                                static_cast<uint>(Context->height), 2, &tmp);
    CHECK_ST
    if (tmp)
        m_vdpDecoderDestroy(tmp);
    if (!ok)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("No H264 decoder support for %1x%2")
            .arg(Context->width).arg(Context->height));
    }
    return ok;
}

VdpOutputSurface MythVDPAUHelper::CreateOutputSurface(QSize Size)
{
    if (!m_valid)
        return 0;

    VdpOutputSurface result = 0;
    INIT_ST
    status = m_vdpOutputSurfaceCreate(m_device, VDP_RGBA_FORMAT_B8G8R8A8,
                                      static_cast<uint>(Size.width()),
                                      static_cast<uint>(Size.height()), &result);
    CHECK_ST
    return result;
}

void MythVDPAUHelper::DeleteOutputSurface(VdpOutputSurface Surface)
{
    if (!Surface)
        return;

    INIT_ST
    status = m_vdpOutputSurfaceDestroy(Surface);
    CHECK_ST
}

VdpVideoMixer MythVDPAUHelper::CreateMixer(QSize Size, VdpChromaType ChromaType,
                                           MythDeintType Deinterlacer)
{
    if (!m_valid || Size.isEmpty())
        return 0;

    VdpVideoMixer result = 0;

    static const std::array<const VdpVideoMixerParameter,3> parameters {
        VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_WIDTH,
        VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_HEIGHT,
        VDP_VIDEO_MIXER_PARAMETER_CHROMA_TYPE
    };


    uint width  = static_cast<uint>(Size.width());
    uint height = static_cast<uint>(Size.height());
    std::array<void const *,3> parametervalues { &width, &height, &ChromaType};

    uint32_t featurecount = 0;
    std::array<VdpVideoMixerFeature,2> features {};
    VdpBool enable = VDP_TRUE;
    const std::array<VdpBool,2> enables = { enable, enable };

    if (DEINT_MEDIUM == Deinterlacer || DEINT_HIGH == Deinterlacer)
    {
        features[featurecount] = VDP_VIDEO_MIXER_FEATURE_DEINTERLACE_TEMPORAL;
        featurecount++;
    }

    if (DEINT_HIGH== Deinterlacer)
    {
        features[featurecount] = VDP_VIDEO_MIXER_FEATURE_DEINTERLACE_TEMPORAL_SPATIAL;
        featurecount++;
    }

    INIT_ST
    status = m_vdpVideoMixerCreate(m_device, featurecount, featurecount ? features.data() : nullptr,
                                   3, parameters.data(), parametervalues.data(), &result);
    CHECK_ST

    if (!ok || !result)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create video mixer");
        return result;
    }

    if (featurecount)
    {
        status = m_vdpVideoMixerSetFeatureEnables(result, featurecount, features.data(), enables.data());
        CHECK_ST
    }
    return result;
}

void MythVDPAUHelper::MixerRender(VdpVideoMixer Mixer, VdpVideoSurface Source,
                                  VdpOutputSurface Dest, FrameScanType Scan, int TopFieldFirst,
                                  QVector<AVBufferRef*>& Frames)
{
    if (!m_valid || !Mixer || !Source || !Dest)
        return;

    VdpVideoMixerPictureStructure field = VDP_VIDEO_MIXER_PICTURE_STRUCTURE_FRAME;
    if (kScan_Interlaced == Scan)
    {
        field = TopFieldFirst ? VDP_VIDEO_MIXER_PICTURE_STRUCTURE_TOP_FIELD :
                                VDP_VIDEO_MIXER_PICTURE_STRUCTURE_BOTTOM_FIELD;
    }
    else if (kScan_Intr2ndField == Scan)
    {
        field = TopFieldFirst ? VDP_VIDEO_MIXER_PICTURE_STRUCTURE_BOTTOM_FIELD :
                                VDP_VIDEO_MIXER_PICTURE_STRUCTURE_TOP_FIELD;
    }

    int count = Frames.size();
    if ((count < 1) || (field == VDP_VIDEO_MIXER_PICTURE_STRUCTURE_FRAME))
    {
        INIT_ST
        status = m_vdpVideoMixerRender(Mixer, VDP_INVALID_HANDLE, nullptr, field,
                                       0, nullptr, Source, 0, nullptr, nullptr, Dest, nullptr, nullptr, 0, nullptr);
        CHECK_ST
    }
    else
    {
        std::array<VdpVideoSurface,2> past   = { VDP_INVALID_HANDLE, VDP_INVALID_HANDLE };
        std::array<VdpVideoSurface,1> future = { VDP_INVALID_HANDLE };

        auto next    = static_cast<VdpVideoSurface>(reinterpret_cast<uintptr_t>(Frames[0]->data));
        auto current = static_cast<VdpVideoSurface>(reinterpret_cast<uintptr_t>(Frames[count > 1 ? 1 : 0]->data));
        auto last    = static_cast<VdpVideoSurface>(reinterpret_cast<uintptr_t>(Frames[count > 2 ? 2 : 0]->data));

        if (field == VDP_VIDEO_MIXER_PICTURE_STRUCTURE_BOTTOM_FIELD)
        {
            future[0] = current;
            past[0] = past[1] = last;
        }
        else
        {
            future[0] = next;
            past[0] = current;
            past[1] = last;
        }

        INIT_ST
        status = m_vdpVideoMixerRender(Mixer, VDP_INVALID_HANDLE, nullptr, field,
                                       2, past.data(), current, 1, future.data(),
                                       nullptr, Dest, nullptr, nullptr, 0, nullptr);
        CHECK_ST
    }
}

void MythVDPAUHelper::DeleteMixer(VdpVideoMixer Mixer)
{
    if (!Mixer)
        return;

    INIT_ST
    status = m_vdpVideoMixerDestroy(Mixer);
    CHECK_ST
}

void MythVDPAUHelper::SetCSCMatrix(VdpVideoMixer Mixer, MythVideoColourSpace *ColourSpace)
{
    if (!Mixer || !ColourSpace)
        return;

    VdpVideoMixerAttribute attr = { VDP_VIDEO_MIXER_ATTRIBUTE_CSC_MATRIX };
    void const* val = { ColourSpace->data() };

    INIT_ST
    status = m_vdpVideoMixerSetAttributeValues(Mixer, 1, &attr, &val);
    CHECK_ST
}

bool MythVDPAUHelper::IsFeatureAvailable(uint Feature)
{
    if (!m_valid)
        return false;

    INIT_ST
    auto supported = static_cast<VdpBool>(false);
    status = m_vdpVideoMixerQueryFeatureSupport(m_device, Feature, &supported);
    CHECK_ST
    return ok && static_cast<bool>(supported);
}

bool MythVDPAUHelper::IsAttributeAvailable(uint Attribute)
{
    if (!m_valid)
        return false;

    INIT_ST
    auto supported = static_cast<VdpBool>(false);
    status = m_vdpVideoMixerQueryAttributeSupport(m_device, Attribute, &supported);
    CHECK_ST
    return ok && static_cast<bool>(supported);
}

QSize MythVDPAUHelper::GetSurfaceParameters(VdpVideoSurface Surface, VdpChromaType &Chroma)
{
    if (!Surface)
        return {};

    uint width = 0;
    uint height = 0;
    INIT_ST
    status = m_vdpVideoSurfaceGetParameters(Surface, &Chroma, &width, &height);
    CHECK_ST

    return {static_cast<int>(width), static_cast<int>(height)};
}
