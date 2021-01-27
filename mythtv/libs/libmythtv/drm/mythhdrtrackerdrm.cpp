// MythTV
#include "mythlogging.h"
#include "mythframe.h"
#include "drm/mythhdrtrackerdrm.h"
#include "platforms/mythdisplaydrm.h"

#define LOC QString("HDRDrm: ")

HDRTracker MythHDRTrackerDRM::Create(MythDisplay* _Display, int HDRSupport)
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
        if (auto hdrprop = MythDRMProperty::GetProperty("HDR_OUTPUT_METADATA", connector->m_properties); hdrprop.get())
            return std::shared_ptr<MythHDRTracker>(new MythHDRTrackerDRM(device, connector, hdrprop, HDRSupport));

    return nullptr;
}

MythHDRTrackerDRM::MythHDRTrackerDRM(MythDRMPtr Device, DRMConn Connector, DRMProp HDRProp, int HDRSupport)
  : MythHDRTracker(HDRSupport),
    m_device(Device),
    m_connector(Connector),
    m_hdrProp(HDRProp)
{
    m_crtc = m_device->GetCrtc();
    m_activeProp = MythDRMProperty::GetProperty("ACTIVE", m_crtc->m_properties);
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Tracking HDR signalling for: %1")
        .arg(MythEDID::EOTFToStrings(m_hdrSupport).join(",")));
}

MythHDRTrackerDRM::~MythHDRTrackerDRM()
{
    MythHDRTrackerDRM::Reset();
}

void MythHDRTrackerDRM::Reset()
{
    if (m_hdrBlob)
    {
        m_device->QueueAtomics({{ m_connector->m_id, m_hdrProp->m_id, 0 }});
        if (m_crtc.get() && m_activeProp.get())
            m_device->QueueAtomics({{ m_crtc->m_id, m_activeProp->m_id, 1 }});
        LOG(VB_GENERAL, LOG_INFO, LOC + "Disabling HDR");
        drmModeDestroyPropertyBlob(m_device->GetFD(), m_hdrBlob);
        m_hdrBlob = 0;
    }
}

void MythHDRTrackerDRM::Update(MythVideoFrame* Frame)
{
    if (!Frame)
        return;

    // Do we have an HDR EOTF that is supported by the display and HDR Metadata blob support
    if (auto eotf = MythVideoDRMUtils::FFmpegTransferToEOTF(Frame->m_colortransfer, m_hdrSupport);
        eotf >= HDMI_EOTF_SMPTE_ST2084)
    {
        bool needhdrblob = false;
        // HLG has no static metadata
        if (HDMI_EOTF_BT_2100_HLG == eotf)
        {
            if (!(m_hdrBlob && m_drmMetadata.hdmi_metadata_type1.eotf == HDMI_EOTF_BT_2100_HLG))
            {
                needhdrblob = true;
                m_ffmpegMetadata = nullptr;
                m_drmMetadata = MythVideoDRMUtils::s_defaultMetadata;
                m_drmMetadata.hdmi_metadata_type1.eotf = eotf;
            }
        }
        else
        {
            if (!m_ffmpegMetadata.get() || !m_ffmpegMetadata.get()->Equals(Frame->m_hdrMetadata.get()) ||
                 m_drmMetadata.hdmi_metadata_type1.eotf != HDMI_EOTF_SMPTE_ST2084)
            {
                needhdrblob = true;
                m_ffmpegMetadata = Frame->m_hdrMetadata.get() ?
                    Frame->m_hdrMetadata : std::make_shared<MythHDRMetadata>();
                m_drmMetadata = MythVideoDRMUtils::s_defaultMetadata;
                m_drmMetadata.hdmi_metadata_type1.eotf = eotf;
                m_drmMetadata.hdmi_metadata_type1.max_display_mastering_luminance = m_ffmpegMetadata->m_maxMasteringLuminance;
                m_drmMetadata.hdmi_metadata_type1.min_display_mastering_luminance = m_ffmpegMetadata->m_minMasteringLuminance;
                m_drmMetadata.hdmi_metadata_type1.max_cll = m_ffmpegMetadata->m_maxContentLightLevel;
                m_drmMetadata.hdmi_metadata_type1.max_fall = m_ffmpegMetadata->m_maxFrameAverageLightLevel;
                m_drmMetadata.hdmi_metadata_type1.white_point.x = m_ffmpegMetadata->m_whitePoint[0];
                m_drmMetadata.hdmi_metadata_type1.white_point.y = m_ffmpegMetadata->m_whitePoint[1];
                for (size_t i = 0; i < 3; ++i)
                {
                    m_drmMetadata.hdmi_metadata_type1.display_primaries[i].x = m_ffmpegMetadata->m_displayPrimaries[i][0];
                    m_drmMetadata.hdmi_metadata_type1.display_primaries[i].y = m_ffmpegMetadata->m_displayPrimaries[i][1];
                }
            }
        }

        if (needhdrblob)
        {
            if (m_hdrBlob)
                drmModeDestroyPropertyBlob(m_device->GetFD(), m_hdrBlob);
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("Creating HDR info frame for %1")
                .arg(eotf == HDMI_EOTF_BT_2100_HLG ? "HLG" : "HDR10"));
            if (drmModeCreatePropertyBlob(m_device->GetFD(), &m_drmMetadata, sizeof(m_drmMetadata), &m_hdrBlob) == 0)
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("HDR blob id %1").arg(m_hdrBlob));
                m_device->QueueAtomics({{ m_connector->m_id, m_hdrProp->m_id, m_hdrBlob }});
                if (m_crtc.get() && m_activeProp.get())
                    m_device->QueueAtomics({{ m_crtc->m_id, m_activeProp->m_id, 1 }});
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create HDR blob id");
            }
        }
    }
    else
    {
        Reset();
    }
}
