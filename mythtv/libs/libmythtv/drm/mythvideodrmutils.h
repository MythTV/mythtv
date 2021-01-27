#ifndef MYTHVIDEODRMUTILS_H
#define MYTHVIDEODRMUTILS_H

// MythTV
#include "config.h"
#include "platforms/drm/mythdrmproperty.h"

// libdrm
extern "C" {
enum hdmi_eotf
{
    HDMI_EOTF_TRADITIONAL_GAMMA_SDR = 0,
    HDMI_EOTF_TRADITIONAL_GAMMA_HDR,
    HDMI_EOTF_SMPTE_ST2084,
    HDMI_EOTF_BT_2100_HLG,
};

#if HAVE_STRUCT_HDR_METADATA_INFOFRAME
#include <drm_mode.h>
#else
enum hdmi_metadata_type
{
    HDMI_STATIC_METADATA_TYPE1 = 1,
};

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

class MythVideoDRMUtils
{
  public:
    static uint64_t FFmpegColorRangeToDRM    (DRMProp Property, int Range);
    static uint64_t FFmpegColorEncodingToDRM (DRMProp Property, int Encoding);
    static uint8_t  FFmpegTransferToEOTF     (int Transfer, int Supported);
    static inline hdr_output_metadata s_defaultMetadata =
    {
        HDMI_STATIC_METADATA_TYPE1, { 0, HDMI_STATIC_METADATA_TYPE1, {{0,0}}, {0,0}, 0, 0, 0, 0}
    };
};

#endif
