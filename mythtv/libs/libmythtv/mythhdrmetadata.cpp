// MythTV
#include "mythhdrmetadata.h"
#include "mythframe.h"

// Std
#include <cmath>

// FFmpeg
extern "C" {
#include "libavutil/mastering_display_metadata.h"
}

#define UINT16(X) static_cast<uint16_t>(std::round(X))

/*! \class MythHDRMetadata
 *  \brief A wrapper around static HDR metadata.
 *
 * This class acts as an intermediate format between FFmpeg's HDR metadata
 * structures and the single structure used by the linux kernel. Values
 * are currently scaled/converted as appropriate for those expected by the
 * linux DRM subsystem and may need adjusting for other platforms/APIs.
 *
 * \note The linux kernel structures are aligned with the structures defined
 * in the HDMI standards (e.g. CTA-861-G).
*/
void MythHDRMetadata::Update(const AVMasteringDisplayMetadata* Display,
                             const AVContentLightMetadata* Light)
{
    bool luminance = Display && Display->has_luminance;
    bool primaries = Display && Display->has_primaries;
    m_maxMasteringLuminance = luminance ? UINT16(av_q2d(Display->max_luminance)) : 0;
    m_minMasteringLuminance = luminance ? UINT16(av_q2d(Display->min_luminance) * 10000) : 0;
    m_whitePoint[0] = primaries ? UINT16(av_q2d(Display->white_point[0]) * 50000) : 0;
    m_whitePoint[1] = primaries ? UINT16(av_q2d(Display->white_point[1]) * 50000) : 0;
    for (size_t i = 0; i < 3; i++)
    {
        m_displayPrimaries[i][0] = primaries ? UINT16(av_q2d(Display->display_primaries[i][0]) * 50000) : 0;
        m_displayPrimaries[i][1] = primaries ? UINT16(av_q2d(Display->display_primaries[i][1]) * 50000) : 0;
    }
    m_maxContentLightLevel = Light ? static_cast<uint16_t>(Light->MaxCLL) : 0;
    m_maxFrameAverageLightLevel = Light ? static_cast<uint16_t>(Light->MaxFALL) : 0;
}

/*! \brief Create, update or destroy HDR metadata for the given MythVideoFrame.
*/
void MythHDRMetadata::Populate(MythVideoFrame *Frame, AVFrame *AvFrame)
{
    if (!Frame)
        return;

    if (AvFrame)
    {
        const auto * display = av_frame_get_side_data(AvFrame, AV_FRAME_DATA_MASTERING_DISPLAY_METADATA);
        const auto * ddata   = display ? reinterpret_cast<AVMasteringDisplayMetadata*>(display->data) : nullptr;
        const auto * content = av_frame_get_side_data(AvFrame, AV_FRAME_DATA_CONTENT_LIGHT_LEVEL);
        const auto * cdata   = content ? reinterpret_cast<AVContentLightMetadata*>(content->data) : nullptr;
        if (ddata || cdata)
        {
            if (!Frame->m_hdrMetadata.get())
                Frame->m_hdrMetadata = std::shared_ptr<MythHDRMetadata>(new MythHDRMetadata());
            Frame->m_hdrMetadata->Update(ddata, cdata);
            return;
        }
    }
    Frame->m_hdrMetadata = nullptr;
}

bool MythHDRMetadata::Equals(MythHDRMetadata* Other)
{
    return Other &&
           m_maxMasteringLuminance     == Other->m_maxMasteringLuminance     &&
           m_minMasteringLuminance     == Other->m_minMasteringLuminance     &&
           m_maxContentLightLevel      == Other->m_maxContentLightLevel      &&
           m_maxFrameAverageLightLevel == Other->m_maxFrameAverageLightLevel &&
           m_whitePoint[0]             == Other->m_whitePoint[0]             &&
           m_whitePoint[1]             == Other->m_whitePoint[1]             &&
           m_displayPrimaries[0][0]    == Other->m_displayPrimaries[0][0]    &&
           m_displayPrimaries[0][1]    == Other->m_displayPrimaries[0][1]    &&
           m_displayPrimaries[1][0]    == Other->m_displayPrimaries[1][0]    &&
           m_displayPrimaries[1][1]    == Other->m_displayPrimaries[1][1]    &&
           m_displayPrimaries[2][0]    == Other->m_displayPrimaries[2][0]    &&
           m_displayPrimaries[2][1]    == Other->m_displayPrimaries[2][1];
}
