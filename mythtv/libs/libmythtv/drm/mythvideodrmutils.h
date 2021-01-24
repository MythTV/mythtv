#ifndef MYTHVIDEODRMUTILS_H
#define MYTHVIDEODRMUTILS_H

// MythTV
#include "platforms/drm/mythdrmproperty.h"

// These are not widely found in drm headers yet - so use a copy for now
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
    HDMI_STATIC_METADATA_TYPE1 = 1,
};

struct hdr_metadata_infoframe
{
    uint8_t eotf;
    hdmi_metadata_type metadata_type;
    struct
    {
        uint16_t x, y;
    } display_primaries[3];
    struct
    {
        uint16_t x, y;
    } white_point;
    uint16_t max_display_mastering_luminance;
    uint16_t min_display_mastering_luminance;
    uint16_t max_cll;
    uint16_t max_fall;
};

struct hdr_output_metadata
{
    hdmi_metadata_type metadata_type;
    union
    {
        struct hdr_metadata_infoframe hdmi_metadata_type1;
    };
};
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
