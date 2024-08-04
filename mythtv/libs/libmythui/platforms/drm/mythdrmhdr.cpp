// MythTV
#include "libmythbase/mythconfig.h"
#include "libmythbase/mythlogging.h"

#include "platforms/drm/mythdrmhdr.h"
#include "platforms/mythdisplaydrm.h"

// libdrm
extern "C" {
enum hdmi_eotf : std::uint8_t
{
    HDMI_EOTF_TRADITIONAL_GAMMA_SDR = 0,
    HDMI_EOTF_TRADITIONAL_GAMMA_HDR,
    HDMI_EOTF_SMPTE_ST2084,
    HDMI_EOTF_BT_2100_HLG,
};

enum hdmi_metadata_type : std::uint8_t
{
    HDMI_STATIC_METADATA_TYPE1 = 0,
};

#if HAVE_STRUCT_HDR_METADATA_INFOFRAME
#include <drm_mode.h>
#else
struct hdr_metadata_infoframe {
    __u8 eotf;
    __u8 metadata_type;
    struct {
        __u16 x, y;
        } display_primaries[3];
    struct {
        __u16 x, y;
        } white_point;
    __u16 max_display_mastering_luminance;
    __u16 min_display_mastering_luminance;
    __u16 max_cll;
    __u16 max_fall;
};

struct hdr_output_metadata {
    __u32 metadata_type;
    union {
        struct hdr_metadata_infoframe hdmi_metadata_type1;
    };
};
#endif
}

#define LOC QString("HDR: ")

MythHDRPtr MythDRMHDR::Create(MythDisplay* MDisplay, const MythHDRDesc& Desc)
{
    auto * display = dynamic_cast<MythDisplayDRM*>(MDisplay);
    if (!display)
        return nullptr;

    // Display support for HDR formats has already been checked. We need to check
    // here that we have a valid, authenticated DRM device and that the connector
    // supports the HDR_OUTPUT_METADATA property
    auto device = display->GetDevice();
    if (!(device && device->Authenticated() && device->Atomic()))
        return nullptr;

    if (auto connector = device->GetConnector(); connector.get())
        if (auto hdrprop = MythDRMProperty::GetProperty("HDR_OUTPUT_METADATA", connector->m_properties); hdrprop)
            return std::shared_ptr<MythHDR>(new MythDRMHDR(device, hdrprop, Desc));

    return nullptr;
}

MythDRMHDR::MythDRMHDR(const MythDRMPtr& Device, DRMProp HDRProp, const MythHDRDesc& Desc)
  : MythHDR(Desc),
    m_device(Device),
    m_connector(Device->GetConnector()),
    m_hdrProp(std::move(HDRProp)),
    m_crtc(Device->GetCrtc())
{
    m_controllable = true;
    m_activeProp = MythDRMProperty::GetProperty("ACTIVE", m_crtc->m_properties);
}

MythDRMHDR::~MythDRMHDR()
{
    Cleanup();
}

void MythDRMHDR::Cleanup()
{
    if (m_hdrBlob && m_device)
    {
        drmModeDestroyPropertyBlob(m_device->GetFD(), m_hdrBlob);
        m_hdrBlob = 0;
    }
}

static inline hdmi_eotf MythHDRToDRMHDR(MythHDR::HDRType Type)
{
    switch (Type)
    {
        case MythHDR::HDR10: return HDMI_EOTF_SMPTE_ST2084;
        case MythHDR::HLG:   return HDMI_EOTF_BT_2100_HLG;
        default: break;
    }
    return HDMI_EOTF_TRADITIONAL_GAMMA_SDR;
}

void MythDRMHDR::SetHDRMetadata(HDRType Type, const MythHDRMetaPtr& Metadata)
{
    Cleanup();
    auto eotf = MythHDRToDRMHDR(Type);
    hdr_output_metadata drmmetadata =
    {
        HDMI_STATIC_METADATA_TYPE1,
        { static_cast<__u8>(eotf), HDMI_STATIC_METADATA_TYPE1, {{0,0}}, {0,0}, 0, 0, 0, 0}
    };

    if (Metadata)
    {
        drmmetadata.hdmi_metadata_type1.max_display_mastering_luminance = Metadata->m_maxMasteringLuminance;
        drmmetadata.hdmi_metadata_type1.min_display_mastering_luminance = Metadata->m_minMasteringLuminance;
        drmmetadata.hdmi_metadata_type1.max_cll = Metadata->m_maxContentLightLevel;
        drmmetadata.hdmi_metadata_type1.max_fall = Metadata->m_maxFrameAverageLightLevel;
        drmmetadata.hdmi_metadata_type1.white_point.x = Metadata->m_whitePoint[0];
        drmmetadata.hdmi_metadata_type1.white_point.y = Metadata->m_whitePoint[1];
        for (size_t i = 0; i < 3; ++i)
        {
            drmmetadata.hdmi_metadata_type1.display_primaries[i].x = Metadata->m_displayPrimaries[i][0];
            drmmetadata.hdmi_metadata_type1.display_primaries[i].y = Metadata->m_displayPrimaries[i][1];
        }
    }

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Creating HDR info frame for %1")
        .arg(eotf == HDMI_EOTF_BT_2100_HLG ? "HLG" : eotf == HDMI_EOTF_SMPTE_ST2084 ? "HDR10" : "SDR"));
    if (drmModeCreatePropertyBlob(m_device->GetFD(), &drmmetadata, sizeof(drmmetadata), &m_hdrBlob) == 0)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("HDR blob id %1").arg(m_hdrBlob));
        m_device->QueueAtomics({{ m_connector->m_id, m_hdrProp->m_id, m_hdrBlob }});
        if (m_crtc && m_activeProp)
            m_device->QueueAtomics({{ m_crtc->m_id, m_activeProp->m_id, 1 }});
        m_currentType = Type;
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create HDR blob id");
    }
}
