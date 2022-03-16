// MythTV
#include "libmythbase/mythlogging.h"
#include "platforms/drm/mythdrmplane.h"

#define LOC QString("DRMPlane: ")

/*! \class MythDRMPlane
 * \brief A wrapper around a DRM plane object.
 *
 * The full list of available planes can be retrieved with GetPlanes.
*/
QString MythDRMPlane::PlaneTypeToString(uint64_t Type)
{
    if (Type == DRM_PLANE_TYPE_OVERLAY) return "Overlay";
    if (Type == DRM_PLANE_TYPE_CURSOR)  return "Cursor";
    if (Type == DRM_PLANE_TYPE_PRIMARY) return "Primary";
    return "Unknown";
}

DRMPlane MythDRMPlane::Create(int FD, uint32_t Id, uint32_t Index)
{
    if (FD && Id)
        if (auto plane = std::shared_ptr<MythDRMPlane>(new MythDRMPlane(FD, Id, Index)); plane && plane->m_id)
            return plane;

    return nullptr;
}

MythDRMPlane::MythDRMPlane(int FD, uint32_t Id, uint32_t Index)
{
    if (auto * plane = drmModeGetPlane(FD, Id); plane)
    {
        auto id  = plane->plane_id;
        m_index  = Index;
        m_id     = id;
        m_fbId   = plane->fb_id;
        m_crtcId = plane->crtc_id;
        m_possibleCrtcs = plane->possible_crtcs;

        for (uint32_t j = 0; j < plane->count_formats; ++j)
        {
            m_formats.emplace_back(plane->formats[j]);
            if (FormatIsVideo(plane->formats[j]))
                m_videoFormats.emplace_back(plane->formats[j]);
        }

        m_properties = MythDRMProperty::GetProperties(FD, m_id, DRM_MODE_OBJECT_PLANE);

        // Add generic properties
        if (auto type = MythDRMProperty::GetProperty("type", m_properties); type)
            if (auto * enumt = dynamic_cast<MythDRMEnumProperty*>(type.get()); enumt)
                m_type = enumt->m_value;

        // Add video specific properties
        if (!m_videoFormats.empty())
        {
            m_fbIdProp   = MythDRMProperty::GetProperty("FB_ID",   m_properties);
            m_crtcIdProp = MythDRMProperty::GetProperty("CRTC_ID", m_properties);
            m_srcXProp   = MythDRMProperty::GetProperty("SRC_X",   m_properties);
            m_srcYProp   = MythDRMProperty::GetProperty("SRC_Y",   m_properties);
            m_srcWProp   = MythDRMProperty::GetProperty("SRC_W",   m_properties);
            m_srcHProp   = MythDRMProperty::GetProperty("SRC_H",   m_properties);
            m_crtcXProp  = MythDRMProperty::GetProperty("CRTC_X",  m_properties);
            m_crtcYProp  = MythDRMProperty::GetProperty("CRTC_Y",  m_properties);
            m_crtcWProp  = MythDRMProperty::GetProperty("CRTC_W",  m_properties);
            m_crtcHProp  = MythDRMProperty::GetProperty("CRTC_H",  m_properties);
        }

        drmModeFreePlane(plane);
    }
}

QString MythDRMPlane::Description() const
{
    return QString("Plane #%1 %2 Index: %3 FB: %4 CRTC: %5 Formats: %6")
        .arg(m_id).arg(MythDRMPlane::PlaneTypeToString(m_type)).arg(m_index)
        .arg(m_fbId).arg(m_crtcId).arg(MythDRMPlane::FormatsToString(m_formats));
}

DRMPlanes MythDRMPlane::GetPlanes(int FD, int CRTCFilter)
{
    DRMPlanes result;
    auto * planes = drmModeGetPlaneResources(FD);
    if (!planes)
    {
        LOG(VB_GENERAL, LOG_ERR, QString(drmGetDeviceNameFromFd2(FD)) + ": Failed to retrieve planes");
        return result;
    }

    for (uint32_t index = 0; index < planes->count_planes; ++index)
    {
        if (auto plane = MythDRMPlane::Create(FD, planes->planes[index], index); plane)
        {
            if ((CRTCFilter > -1) && !(plane->m_possibleCrtcs & (1 << CRTCFilter)))
                continue;
            result.emplace_back(plane);
        }
    }

    drmModeFreePlaneResources(planes);
    return result;
}

DRMPlanes MythDRMPlane::FilterPrimaryPlanes(const DRMPlanes &Planes)
{
    DRMPlanes result;
    for (const auto & plane : Planes)
        if ((plane->m_type == DRM_PLANE_TYPE_PRIMARY) && !plane->m_videoFormats.empty())
            result.emplace_back(plane);

    return result;
}

DRMPlanes MythDRMPlane::FilterOverlayPlanes(const DRMPlanes &Planes)
{
    DRMPlanes result;
    for (const auto & plane : Planes)
        if ((plane->m_type ==  DRM_PLANE_TYPE_OVERLAY) && HasOverlayFormat(plane->m_formats))
            result.emplace_back(plane);

    return result;
}

QString MythDRMPlane::FormatToString(uint32_t Format)
{
    switch (Format)
    {
        // Note: These match the string literals in QKmsDevice config parsing (but
        // must be used lower case in the KMS config file)
        case DRM_FORMAT_RGB565:        return "RGB565";
        case DRM_FORMAT_XRGB8888:      return "XRGB8888";
        case DRM_FORMAT_XBGR8888:      return "XBGR8888";
        case DRM_FORMAT_ARGB8888:      return "ARGB8888";
        case DRM_FORMAT_RGBA8888:      return "RGBA8888";
        case DRM_FORMAT_ABGR8888:      return "ABGR8888";
        case DRM_FORMAT_BGRA8888:      return "BGRA8888";
        case DRM_FORMAT_XRGB2101010:   return "XRGB2101010";
        case DRM_FORMAT_XBGR2101010:   return "XBGR2101010";
        case DRM_FORMAT_ARGB2101010:   return "ARGB2101010";
        case DRM_FORMAT_ABGR2101010:   return "ABGR2101010";
        // Not supported by Qt as overlay formats
        case DRM_FORMAT_XRGB16161616F: return "XRGB16F";
        case DRM_FORMAT_XBGR16161616F: return "XBGR16F";
        case DRM_FORMAT_ARGB16161616F: return "ARGB16F";
        case DRM_FORMAT_ABGR16161616F: return "ABGR16F";
        default: break;
    }
    return QString::asprintf("%c%c%c%c", Format, Format >> 8, Format >> 16, Format >> 24);
}

QString MythDRMPlane::FormatsToString(const FOURCCVec &Formats)
{
    QStringList formats;
    for (auto format : Formats)
        formats.append(FormatToString(format));
    return formats.join(",");
}

bool MythDRMPlane::FormatIsVideo(uint32_t Format)
{
    static const FOURCCVec s_yuvFormats =
    {
        // Bi-planar
        DRM_FORMAT_NV12, DRM_FORMAT_NV21, DRM_FORMAT_NV16, DRM_FORMAT_NV61,
        DRM_FORMAT_NV24, DRM_FORMAT_NV42, DRM_FORMAT_P210, DRM_FORMAT_P010,
        DRM_FORMAT_P012, DRM_FORMAT_P016, DRM_FORMAT_P030, DRM_FORMAT_NV15,
        DRM_FORMAT_NV20,
        // Tri-planar formats
        DRM_FORMAT_YUV410, DRM_FORMAT_YVU410, DRM_FORMAT_YUV411, DRM_FORMAT_YVU411,
        DRM_FORMAT_YUV420, DRM_FORMAT_YVU420, DRM_FORMAT_YUV422, DRM_FORMAT_YVU422,
        DRM_FORMAT_YUV444, DRM_FORMAT_YVU444,
        // Packed formats
        DRM_FORMAT_YUYV, DRM_FORMAT_YVYU, DRM_FORMAT_UYVY, DRM_FORMAT_VYUY
    };

    return std::find(s_yuvFormats.cbegin(), s_yuvFormats.cend(), Format) != s_yuvFormats.cend();
}

/*! \brief Enusure list of supplied formats contains a format that is suitable for OpenGL/Vulkan.
*/
bool MythDRMPlane::HasOverlayFormat(const FOURCCVec &Formats)
{
    // Formats as deemed suitable by QKmsDevice
    static const FOURCCVec s_rgbFormats =
    {
        DRM_FORMAT_XRGB8888,    DRM_FORMAT_XRGB8888,    DRM_FORMAT_XBGR8888,
        DRM_FORMAT_ARGB8888,    DRM_FORMAT_ABGR8888,    DRM_FORMAT_RGB565,
        DRM_FORMAT_BGR565,      DRM_FORMAT_XRGB2101010, DRM_FORMAT_XBGR2101010,
        DRM_FORMAT_ARGB2101010, DRM_FORMAT_ABGR2101010
    };

    for (auto format : Formats)
        if (std::any_of(s_rgbFormats.cbegin(), s_rgbFormats.cend(), [&format](auto Format) { return Format == format; }))
            return true;

    return false;
}

uint32_t MythDRMPlane::GetAlphaFormat(const FOURCCVec &Formats)
{
    // N.B. Prioritised list
    static const FOURCCVec s_alphaFormats =
    {
        DRM_FORMAT_ARGB8888, DRM_FORMAT_ABGR8888, DRM_FORMAT_ARGB2101010, DRM_FORMAT_ABGR2101010
    };

    for (auto format : s_alphaFormats)
        if (std::any_of(Formats.cbegin(), Formats.cend(), [&format](auto Format) { return Format == format; }))
            return format;

    return DRM_FORMAT_INVALID;
}
