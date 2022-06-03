// MythTV
#include "libmythbase/mythlogging.h"
#include "libmythui/mythedid.h"
#include "drm/mythvideodrmutils.h"

// libavutil
extern "C" {
#include "libavutil/pixfmt.h"
#include "libavutil/pixdesc.h"
}

#define LOC QString("DRMUtils: ")

uint64_t MythVideoDRMUtils::FFmpegColorRangeToDRM(const DRMProp& Property, int Range)
{
    // Default to limited range
    uint64_t result = 0;

    // Pull out enums
    auto *rangeprop = dynamic_cast<MythDRMEnumProperty*>(Property.get());
    if (!rangeprop)
        return result;

    const auto *searchstring = (Range == AVCOL_RANGE_MPEG) ? "limited" : "full";
    // cppcheck-suppress unassignedVariable
    for (const auto & [value,name] : rangeprop->m_enums)
    {
        if (name.contains(searchstring, Qt::CaseInsensitive))
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Using '%1' as color range for '%2'")
                .arg(name, av_color_range_name(static_cast<AVColorRange>(Range))));
            return value;
        }
    }
    return result;
}

uint64_t MythVideoDRMUtils::FFmpegColorEncodingToDRM(const DRMProp& Property, int Encoding)
{
    // Default to BT.601
    uint64_t result = 0;

    // Pull out enums
    auto *rangeprop = dynamic_cast<MythDRMEnumProperty*>(Property.get());
    if (!rangeprop)
        return result;

    const auto *searchstring = "601";
    switch (Encoding)
    {
        case AVCOL_SPC_BT709: searchstring = "709"; break;
        // I've only seen 2020 available on amdgpu with a Ryzen3400G. If selected
        // it produces an EINVAL - which breaks Qt rendering. Probably needs to be
        // used in conjunction with HDR. Needs investigating.
        //case AVCOL_SPC_BT2020_NCL:
        //case AVCOL_SPC_BT2020_CL:
        //case AVCOL_SPC_CHROMA_DERIVED_NCL:
        //case AVCOL_SPC_CHROMA_DERIVED_CL: searchstring = "2020"; break;
        default: break;
    }

    // cppcheck-suppress unassignedVariable
    for (const auto & [value,name] : rangeprop->m_enums)
    {
        if (name.contains(searchstring, Qt::CaseInsensitive))
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Using '%1' as color encoding for '%2'")
                .arg(name, av_color_space_name(static_cast<AVColorSpace>(Encoding))));
            return value;
        }
    }
    return result;
}
