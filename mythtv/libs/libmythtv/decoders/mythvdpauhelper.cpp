// MythTV
#include "mythlogging.h"
#include "videocolourspace.h"
#include "mythvdpauhelper.h"
#include "platforms/mythxdisplay.h" // always last

// Std
#include <cmath>

QMutex MythVDPAUHelper::gVDPAULock(QMutex::Recursive);
bool   MythVDPAUHelper::gVDPAUAvailable = false;
bool   MythVDPAUHelper::gVDPAUMPEG4Available = false;

#define LOC QString("VDPAUHelp: ")

#define INIT_ST \
VdpStatus status; \
bool ok = true;

#define CHECK_ST \
ok &= (status == VDP_STATUS_OK); \
if (!ok) \
{ \
  LOG(VB_GENERAL, LOG_ERR, LOC + QString("Error at %1:%2 (#%3, %4)") \
          .arg(__FILE__).arg( __LINE__).arg(status) \
          .arg(m_vdpGetErrorString(status))); \
}

#define GET_PROC(FUNC_ID, PROC) \
status = m_vdpGetProcAddress(m_device, FUNC_ID, reinterpret_cast<void **>(&(PROC))); CHECK_ST

bool MythVDPAUHelper::HaveVDPAU(void)
{
    QMutexLocker locker(&gVDPAULock);
    static bool s_checked = false;
    if (s_checked)
        return gVDPAUAvailable;

    MythVDPAUHelper vdpau;
    gVDPAUAvailable = vdpau.IsValid();
    s_checked = true;
    if (gVDPAUAvailable)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "VDPAU is available");
        gVDPAUMPEG4Available = vdpau.CheckMPEG4();
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "VDPAU functionality checked failed");
    }
    return gVDPAUAvailable;
}

bool MythVDPAUHelper::HaveMPEG4Decode(void)
{
    return gVDPAUMPEG4Available;
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
    m_vdpGetProcAddress(Context->get_proc_address)
{
    m_valid = InitProcs();
    if (m_valid)
    {
        INIT_ST
        status = m_vdpPreemptionCallbackRegister(m_device, vdpau_preemption_callback, this);
        CHECK_ST
        if (!ok)
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to register preemption callback");
    }
}

static const char* DummyGetError(VdpStatus /*status*/)
{
    static constexpr char kDummy[] = "Unknown";
    return &kDummy[0];
}

MythVDPAUHelper::MythVDPAUHelper(void)
  : m_createdDevice(true)
{
    m_display = OpenMythXDisplay(false);
    if (!m_display)
        return;

    INIT_ST
    m_vdpGetErrorString = &DummyGetError;
    XLOCK(m_display, status = vdp_device_create_x11(m_display->GetDisplay(),
                                                    m_display->GetScreen(),
                                                    &m_device, &m_vdpGetProcAddress));
    CHECK_ST
    if (!ok)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create VDPAU device.");
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
    GET_PROC(VDP_FUNC_ID_OUTPUT_SURFACE_CREATE, m_vdpOutputSurfaceCreate)
    GET_PROC(VDP_FUNC_ID_OUTPUT_SURFACE_DESTROY, m_vdpOutputSurfaceDestroy)
    GET_PROC(VDP_FUNC_ID_VIDEO_SURFACE_GET_PARAMETERS, m_vdpVideoSurfaceGetParameters)
    GET_PROC(VDP_FUNC_ID_PREEMPTION_CALLBACK_REGISTER, m_vdpPreemptionCallbackRegister)

    return ok;
}

bool MythVDPAUHelper::IsValid(void)
{
    return m_valid;
}

void MythVDPAUHelper::SetPreempted(void)
{
    emit DisplayPreempted();
}

bool MythVDPAUHelper::CheckMPEG4(void)
{
    if (!m_valid)
        return false;

#ifdef VDP_DECODER_PROFILE_MPEG4_PART2_ASP
    INIT_ST
    VdpBool supported = false;
    uint32_t tmp1;
    uint32_t tmp2;
    uint32_t tmp3;
    uint32_t tmp4;
    status = m_vdpDecoderQueryCapabilities(m_device,
                VDP_DECODER_PROFILE_MPEG4_PART2_ASP, &supported,
                &tmp1, &tmp2, &tmp3, &tmp4);
    CHECK_ST
    return supported;
#else
    return false;
#endif
}

bool MythVDPAUHelper::CheckHEVCDecode(AVCodecContext *Context)
{
    if (!Context)
        return false;

    MythVDPAUHelper vdpau;
    if (vdpau.IsValid())
        return vdpau.HEVCProfileCheck(Context);
    return false;
}

bool MythVDPAUHelper::HEVCProfileCheck(AVCodecContext *Context)
{
    if (!m_valid || !Context)
        return false;

    // FFmpeg will disallow HEVC VDPAU for driver versions < 410
    const char* infostring = nullptr;
    INIT_ST
    status = m_vdpGetInformationString(&infostring);
    CHECK_ST
    if (!ok || !infostring)
        return false;

    int driver = 0;
    sscanf(infostring, "NVIDIA VDPAU Driver Shared Library  %d", &driver);
    if (driver < 410)
        return false;

    VdpDecoderProfile profile;
    switch (Context->profile)
    {
#ifdef VDP_DECODER_PROFILE_HEVC_MAIN
        case FF_PROFILE_HEVC_MAIN: profile = VDP_DECODER_PROFILE_HEVC_MAIN; break;
#endif
#ifdef VDP_DECODER_PROFILE_HEVC_MAIN_10
        case FF_PROFILE_HEVC_MAIN_10: profile = VDP_DECODER_PROFILE_HEVC_MAIN_10; break;
#endif
#ifdef VDP_DECODER_PROFILE_HEVC_MAIN_STILL
        case FF_PROFILE_HEVC_MAIN_STILL_PICTURE: profile = VDP_DECODER_PROFILE_HEVC_MAIN_STILL; break;
#endif
        default: return false;
    }

    VdpBool supported = false;
    uint32_t level;
    uint32_t macros;
    uint32_t width;
    uint32_t height;
    status = m_vdpDecoderQueryCapabilities(m_device, profile, &supported, &level, &macros, &width, &height);
    CHECK_ST
    if (!supported)
        return false;

    return (width  >= static_cast<uint>(Context->width)) &&
           (height >= static_cast<uint>(Context->height)) &&
           (level  >= static_cast<uint>(Context->level));
}

bool MythVDPAUHelper::CheckH264Decode(AVCodecContext *Context)
{
    if (!Context)
        return false;

    VdpDecoderProfile profile;
    switch (Context->profile & ~FF_PROFILE_H264_INTRA)
    {
        case FF_PROFILE_H264_BASELINE: profile = VDP_DECODER_PROFILE_H264_BASELINE; break;
#ifdef VDP_DECODER_PROFILE_H264_CONSTRAINED_BASELINE
        case FF_PROFILE_H264_CONSTRAINED_BASELINE: profile = VDP_DECODER_PROFILE_H264_CONSTRAINED_BASELINE; break;
#endif
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

    int mbs = static_cast<int>(ceil(static_cast<double>(Context->width) / 16.0));
    int check = (mbs == 49 ) || (mbs == 54 ) || (mbs == 59 ) || (mbs == 64) ||
                (mbs == 113) || (mbs == 118) || (mbs == 123) || (mbs == 128);

    // Create an instance
    MythVDPAUHelper helper;
    if (!helper.IsValid())
        return false;
    if (check)
        return helper.H264DecodeCheck(profile, Context);
    return helper.H264ProfileCheck(profile, Context);
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

bool MythVDPAUHelper::H264ProfileCheck(VdpDecoderProfile Profile, AVCodecContext *Context)
{
    if (!m_valid || !Context)
        return false;

    VdpBool supported = false;
    uint32_t level;
    uint32_t macros;
    uint32_t width;
    uint32_t height;
    INIT_ST
    status = m_vdpDecoderQueryCapabilities(m_device, Profile, &supported, &level, &macros, &width, &height);
    CHECK_ST
    if (!supported)
        return false;

    return (width  >= static_cast<uint>(Context->width)) &&
           (height >= static_cast<uint>(Context->height)) &&
           (level  >= static_cast<uint>(Context->level));
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

    VdpVideoMixerParameter parameters[] = {
        VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_WIDTH,
        VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_HEIGHT,
        VDP_VIDEO_MIXER_PARAMETER_CHROMA_TYPE,
        VDP_VIDEO_MIXER_PARAMETER_LAYERS,
    };


    uint width  = static_cast<uint>(Size.width());
    uint height = static_cast<uint>(Size.height());
    uint layers = 0;
    void const * parametervalues[] = { &width, &height, &ChromaType, &layers};

    uint32_t featurecount = 0;
    VdpVideoMixerFeature features[2];
    VdpBool enable = VDP_TRUE;
    const VdpBool enables[2] = { enable, enable };

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
    status = m_vdpVideoMixerCreate(m_device, featurecount, featurecount ? features : nullptr,
                                   4, parameters, parametervalues, &result);
    CHECK_ST

    if (!ok || !result)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create video mixer");
        return result;
    }

    status = m_vdpVideoMixerSetFeatureEnables(result, featurecount, features, enables);
    CHECK_ST
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
        VdpVideoSurface past[2]   = { VDP_INVALID_HANDLE, VDP_INVALID_HANDLE };
        VdpVideoSurface future[1] = { VDP_INVALID_HANDLE };

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
                                       2, past, current, 1, future, nullptr, Dest, nullptr, nullptr, 0, nullptr);
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

void MythVDPAUHelper::SetCSCMatrix(VdpVideoMixer Mixer, VideoColourSpace *ColourSpace)
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
    VdpBool supported = false;
    status = m_vdpVideoMixerQueryFeatureSupport(m_device, Feature, &supported);
    CHECK_ST
    return ok && supported;
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
