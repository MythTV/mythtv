// MythTV
#include "platforms/drm/mythdrmresources.h"

/*! \class MythDRMResources
 * \brief A simple wrapper around a drmModeResPtr that ensures proper cleanup.
*/
MythDRMResources::MythDRMResources(int FD)
  : m_resources(drmModeGetResources(FD))
{
}

MythDRMResources::~MythDRMResources()
{
    if (m_resources)
        drmModeFreeResources(m_resources);
}

drmModeResPtr MythDRMResources::operator->() const
{
    return m_resources;
}

drmModeResPtr MythDRMResources::operator*() const
{
    return m_resources;
}


