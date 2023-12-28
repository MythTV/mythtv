// MythTV
#include "platforms/drm/mythdrmframebuffer.h"
#include "platforms/drm/mythdrmplane.h"

/*! \class MythDRMFramebuffer
 * \brief A simple object representing a DRM Framebuffer object.
*/
DRMFb MythDRMFramebuffer::Create([[maybe_unused]] int FD,
                                 [[maybe_unused]] uint32_t Id)
{
    /*
    Currently disabled as drmModeGetFB2 is not available on older systems and
    this functionality is not currently an absolute requirement
    if (auto fb = std::shared_ptr<MythDRMFramebuffer>(new MythDRMFramebuffer(FD, Id)); fb.get() && fb->m_id)
        return fb;
    */
    return nullptr;
}

MythDRMFramebuffer::MythDRMFramebuffer([[maybe_unused]] int FD,
                                       [[maybe_unused]] uint32_t Id)
{
    /*
    if (auto fb = drmModeGetFB2(FD, Id); fb)
    {
        m_id        = fb->fb_id;
        m_width     = fb->width;
        m_height    = fb->height;
        m_format    = fb->pixel_format;
        m_modifiers = fb->modifier;
        std::copy(std::begin(fb->handles), std::end(fb->handles), std::begin(m_handles));
        std::copy(std::begin(fb->pitches), std::end(fb->pitches), std::begin(m_pitches));
        std::copy(std::begin(fb->offsets), std::end(fb->offsets), std::begin(m_offsets));
        drmModeFreeFB2(fb);
    }
    */
}

QString MythDRMFramebuffer::Description() const
{
    return QString("Framebuffer #%1 Size: %2x%3 Format: %4 Mods: 0x%5")
        .arg(m_id).arg(m_width).arg(m_height).arg(MythDRMPlane::FormatToString(m_format)).arg(m_modifiers, 0, 16);
}
