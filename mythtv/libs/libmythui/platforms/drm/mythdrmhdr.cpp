// MythTV
#include "platforms/drm/mythdrmhdr.h"
#include "platforms/mythdisplaydrm.h"
#include "mythlogging.h"

#define LOC QString("HDR: ")

MythHDRPtr MythDRMHDR::Create(MythDisplay* _Display, const MythHDRDesc& Desc)
{
    auto * display = dynamic_cast<MythDisplayDRM*>(_Display);
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

MythDRMHDR::MythDRMHDR(MythDRMPtr Device, DRMProp HDRProp, const MythHDRDesc& Desc)
  : MythHDR(Desc),
    m_device(Device),
    m_connector(Device->GetConnector()),
    m_hdrProp(HDRProp),
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
    static hdr_output_metadata s_defaultMetadata =
    {
        HDMI_STATIC_METADATA_TYPE1, { 0, HDMI_STATIC_METADATA_TYPE1, {{0,0}}, {0,0}, 0, 0, 0, 0}
    };

    Cleanup();
    auto eotf = MythHDRToDRMHDR(Type);
    m_drmMetadata = s_defaultMetadata;
    m_drmMetadata.hdmi_metadata_type1.eotf = eotf;

    if (Metadata)
    {
        m_drmMetadata.hdmi_metadata_type1.max_display_mastering_luminance = Metadata->m_maxMasteringLuminance;
        m_drmMetadata.hdmi_metadata_type1.min_display_mastering_luminance = Metadata->m_minMasteringLuminance;
        m_drmMetadata.hdmi_metadata_type1.max_cll = Metadata->m_maxContentLightLevel;
        m_drmMetadata.hdmi_metadata_type1.max_fall = Metadata->m_maxFrameAverageLightLevel;
        m_drmMetadata.hdmi_metadata_type1.white_point.x = Metadata->m_whitePoint[0];
        m_drmMetadata.hdmi_metadata_type1.white_point.y = Metadata->m_whitePoint[1];
        for (size_t i = 0; i < 3; ++i)
        {
            m_drmMetadata.hdmi_metadata_type1.display_primaries[i].x = Metadata->m_displayPrimaries[i][0];
            m_drmMetadata.hdmi_metadata_type1.display_primaries[i].y = Metadata->m_displayPrimaries[i][1];
        }
    }

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Creating HDR info frame for %1")
        .arg(eotf == HDMI_EOTF_BT_2100_HLG ? "HLG" : eotf == HDMI_EOTF_SMPTE_ST2084 ? "HDR10" : "SDR"));
    if (drmModeCreatePropertyBlob(m_device->GetFD(), &m_drmMetadata, sizeof(m_drmMetadata), &m_hdrBlob) == 0)
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
