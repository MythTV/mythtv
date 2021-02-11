#ifndef MYTHDRMHDR_H
#define MYTHDRMHDR_H

// MythTV
#include "config.h"
#include "mythhdr.h"
#include "platforms/mythdrmdevice.h"

// libdrm
extern "C" {
enum hdmi_eotf
{
    HDMI_EOTF_TRADITIONAL_GAMMA_SDR = 0,
    HDMI_EOTF_TRADITIONAL_GAMMA_HDR,
    HDMI_EOTF_SMPTE_ST2084,
    HDMI_EOTF_BT_2100_HLG,
};

enum hdmi_metadata_type
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

using DRMMeta = hdr_output_metadata;

class MythDRMHDR : public MythHDR
{
  public:
    static MythHDRPtr Create(class MythDisplay* _Display, const MythHDRDesc& Desc);
   ~MythDRMHDR() override;
    void SetHDRMetadata(HDRType Type, MythHDRMetaPtr Metadata) override;

  protected:
    MythDRMHDR(MythDRMPtr Device, DRMProp HDRProp, const MythHDRDesc& Desc);

  private:
    void Cleanup();
    MythDRMPtr m_device     { nullptr };
    DRMConn    m_connector  { nullptr };
    DRMProp    m_hdrProp    { nullptr };
    DRMCrtc    m_crtc       { nullptr };
    DRMProp    m_activeProp { nullptr };
    uint32_t   m_hdrBlob    { 0 };
    DRMMeta    m_drmMetadata;
};

#endif
