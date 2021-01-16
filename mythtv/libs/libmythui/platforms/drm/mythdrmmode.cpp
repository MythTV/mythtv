// MythTV
#include "platforms/drm/mythdrmmode.h"

/*! \class MythDRMMode
 * \brief A simple object representing a DRM video mode.
*/
DRMMode MythDRMMode::Create(drmModeModeInfoPtr Mode, int Index)
{
    if (auto mode = std::shared_ptr<MythDRMMode>(new MythDRMMode(Mode, Index)); mode && mode->m_rate > 1.0)
        return mode;
    return nullptr;
}

MythDRMMode::MythDRMMode(drmModeModeInfoPtr Mode, int Index)
{
    if (Mode)
    {
        m_index     = Index;
        m_rate      = (Mode->clock * 1000.0) / (Mode->htotal * Mode->vtotal);
        m_width     = Mode->hdisplay;
        m_height    = Mode->vdisplay;
        m_flags     = Mode->flags;
        m_name      = Mode->name;
        if (Mode->flags & DRM_MODE_FLAG_INTERLACE)
            m_rate *= 2.0;
        if (Mode->flags & DRM_MODE_FLAG_DBLSCAN)
            m_rate /= 2.0;
    }
}
