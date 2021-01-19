// MythTV
#include "platforms/drm/mythdrmcrtc.h"

/*! \class MythDRMCrtc
 * \brief A simple wrapper around a DRM CRTC object.
 *
 * The full list of available CRTCs can be retrieved with GetCrtcs. An individual
 * CRTC can be retrieved from a given list with GetCrtc.
*/
DRMCrtc MythDRMCrtc::Create(int FD, uint32_t Id, int Index)
{
    if (FD && Id)
        if (auto crtc = std::shared_ptr<MythDRMCrtc>(new MythDRMCrtc(FD, Id, Index)); crtc.get() && crtc->m_id)
            return crtc;

    return nullptr;
}

DRMCrtc MythDRMCrtc::GetCrtc(const DRMCrtcs &Crtcs, uint32_t Id)
{
    auto match = [&Id](const auto & Crtc) { return Crtc->m_id == Id; };
    if (auto found = std::find_if(Crtcs.cbegin(), Crtcs.cend(), match); found != Crtcs.cend())
        return *found;
    return nullptr;
}

DRMCrtcs MythDRMCrtc::GetCrtcs(int FD)
{
    DRMCrtcs result;
    if (auto resources = MythDRMResources(FD); *resources)
    {
        for (auto i = 0; i < resources->count_crtcs; ++i)
            if (auto crtc = Create(FD, resources->crtcs[i], i); crtc.get())
                result.emplace_back(crtc);
    }
    return result;
}

MythDRMCrtc::MythDRMCrtc(int FD, uint32_t Id, int Index)
{
    if (auto * crtc = drmModeGetCrtc(FD, Id); crtc)
    {
        m_index  = Index;
        m_id     = crtc->crtc_id;
        m_fbId   = crtc->buffer_id;
        m_x      = crtc->x;
        m_y      = crtc->y;
        m_width  = crtc->width;
        m_height = crtc->height;
        if (crtc->mode_valid)
            m_mode = MythDRMMode::Create(&crtc->mode, 0);
        m_properties = MythDRMProperty::GetProperties(FD, m_id, DRM_MODE_OBJECT_CRTC);
        drmModeFreeCrtc(crtc);

        if (m_index < 0)
            m_index = RetrieveCRTCIndex(FD, Id);
    }
}

int MythDRMCrtc::RetrieveCRTCIndex(int FD, uint32_t Id)
{
    int result = -1;
    if (auto resources = MythDRMResources(FD); *resources)
    {
        for (auto i = 0; i < resources->count_crtcs; ++i)
        {
            if (auto * crtc = drmModeGetCrtc(FD, resources->crtcs[i]); crtc)
            {
                bool match = crtc->crtc_id == Id;
                drmModeFreeCrtc(crtc);
                if (match)
                {
                    result = i;
                    break;
                }
            }
        }
    }
    return result;
}
