// Qt
#include <QGuiApplication>
#include <qpa/qplatformnativeinterface.h>

// MythTV
#include "libmythbase/mythlogging.h"
#include "libmythui/mythmainwindow.h"
#include "mythframe.h"
#include "mythvideocolourspace.h"
#include "libmythui/platforms/mythdisplaydrm.h"
#include "libmythui/platforms/drm/mythdrmframebuffer.h"
#include "drm/mythvideodrmutils.h"
#include "drm/mythvideodrm.h"

// Std
#include <unistd.h>

#define LOC QString("DRMVideo: ")

MythVideoDRM::MythVideoDRM(MythVideoColourSpace* ColourSpace)
  : m_colourSpace(ColourSpace)
{
    if (m_colourSpace)
        m_colourSpace->IncrRef();

    if (auto *drmdisplay = HasMythMainWindow() ? dynamic_cast<MythDisplayDRM*>(GetMythMainWindow()->GetDisplay()) : nullptr; drmdisplay)
    {
        if (m_device = drmdisplay->GetDevice(); m_device && m_device->Atomic() && m_device->Authenticated())
        {
            if (m_videoPlane = m_device->GetVideoPlane(); m_videoPlane.get() && m_videoPlane->m_id)
            {
                m_valid = m_videoPlane->m_fbIdProp  && m_videoPlane->m_crtcIdProp &&
                          m_videoPlane->m_srcWProp  && m_videoPlane->m_srcHProp   &&
                          m_videoPlane->m_srcXProp  && m_videoPlane->m_srcYProp   &&
                          m_videoPlane->m_crtcXProp && m_videoPlane->m_crtcYProp  &&
                          m_videoPlane->m_crtcWProp && m_videoPlane->m_crtcHProp;
            }
        }
    }

    if (m_valid)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Using Plane #%1 for video").arg(m_videoPlane->m_id));

        // Set the CRTC for the plane. No need to do this for every frame.
        m_device->QueueAtomics({{ m_videoPlane->m_id, m_videoPlane->m_crtcIdProp->m_id, m_device->GetCrtc()->m_id }});

        // Set colourspace and listen for changes
        if (m_colourSpace)
        {
            // Disable colour adjustments.
            // Note: If DRM subsequently fails, these should be re-enabled by
            // the OpenGL code.
            m_colourSpace->SetSupportedAttributes(kPictureAttributeSupported_None);
            connect(m_colourSpace, &MythVideoColourSpace::Updated, this, &MythVideoDRM::ColourSpaceUpdated);
            ColourSpaceUpdated(true/*??*/);
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to setup DRM video");
    }
}

MythVideoDRM::~MythVideoDRM()
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "Closing");

    // Release colourspace
    if (m_colourSpace)
        m_colourSpace->DecrRef();

    // Disable our video plane
    //m_device->DisableVideoPlane();
}

void MythVideoDRM::ColourSpaceUpdated(bool /*PrimariesChanged*/)
{
    if (!(m_colourSpace && m_device && m_videoPlane.get()))
        return;

    MythAtomics queue; // NOLINT(cppcoreguidelines-init-variables)
    if (auto range = MythDRMProperty::GetProperty("COLOR_RANGE", m_videoPlane->m_properties); range.get())
    {
        auto rangev = MythVideoDRMUtils::FFmpegColorRangeToDRM(range, m_colourSpace->GetRange());
        queue.emplace_back(m_videoPlane->m_id, range->m_id, rangev);
    }

    if (auto encoding = MythDRMProperty::GetProperty("COLOR_ENCODING", m_videoPlane->m_properties); encoding.get())
    {
        auto encv = MythVideoDRMUtils::FFmpegColorEncodingToDRM(encoding, m_colourSpace->GetColourSpace());
        queue.emplace_back(m_videoPlane->m_id, encoding->m_id, encv);
    }

    if (!queue.empty())
        m_device->QueueAtomics(queue);
}

bool MythVideoDRM::RenderFrame(AVDRMFrameDescriptor* DRMDesc, MythVideoFrame* Frame)
{
    if (!m_valid)
        return false;

    if (!(DRMDesc && Frame))
    {
        m_valid = false;
        return false;
    }

    if (!m_handles.contains(DRMDesc))
    {
        auto buffer = MythVideoDRMBuffer::Create(m_device, DRMDesc, { Frame->m_width, Frame->m_height });
        if (!buffer.get())
        {
            m_valid = false;
            return false;
        }
        m_handles.insert(DRMDesc, buffer);
    }

    Frame->m_displayed = true;
    auto handle = m_handles[DRMDesc];
    auto id = m_videoPlane->m_id;

    if (m_lastSrc != Frame->m_srcRect)
    {
        m_lastSrc = Frame->m_srcRect;
        m_device->QueueAtomics({
            { id, m_videoPlane->m_srcXProp->m_id, m_lastSrc.left() << 16 },
            { id, m_videoPlane->m_srcYProp->m_id, m_lastSrc.top() << 16 },
            { id, m_videoPlane->m_srcWProp->m_id, m_lastSrc.width() << 16 },
            { id, m_videoPlane->m_srcHProp->m_id, m_lastSrc.height() << 16 }});
    }

    if (m_lastDst != Frame->m_dstRect)
    {
        m_lastDst = Frame->m_dstRect;
        m_device->QueueAtomics({
           { id, m_videoPlane->m_crtcXProp->m_id, (m_lastDst.left() + 1) & ~1 },
           { id, m_videoPlane->m_crtcYProp->m_id, (m_lastDst.top() + 1) & ~1 },
           { id, m_videoPlane->m_crtcWProp->m_id, (m_lastDst.width() + 1) & ~1 },
           { id, m_videoPlane->m_crtcHProp->m_id, (m_lastDst.height() + 1) & ~1 }});
    }

    return m_device->QueueAtomics({{ id, m_videoPlane->m_fbIdProp->m_id, handle->GetFB() }});
}
