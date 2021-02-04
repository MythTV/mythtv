// MythTV
#ifdef USING_DRM_VIDEO
#include "drm/mythhdrtrackerdrm.h"
#endif
#include "mythhdrtracker.h"
#include "mythedid.h"
#include "mythdisplay.h"
#include "mythlogging.h"

HDRTracker MythHDRTracker::Create(MythDisplay* _Display)
{
    if (!_Display)
        return nullptr;

    // Check for HDR support
    auto hdr = _Display->GetHDRState();
    if (!hdr.get())
        return nullptr;

    // We only support HDR10 and HLG
    if (!hdr->m_supportedTypes)
    {
        LOG(VB_PLAYBACK, LOG_INFO, "No HDR support detected for this display");
        return nullptr;
    }

    // We only know how to deal with HDR Static Metablock Type 1 (which is the only type)
    if (hdr->m_metadataType != MythHDR::StaticType1)
        return nullptr;

    HDRTracker result = nullptr;
#ifdef USING_DRM_VIDEO
    result = MythHDRTrackerDRM::Create(_Display);
#endif

    if (!result.get())
        LOG(VB_PLAYBACK, LOG_INFO, "HDR signalling support not available");
    return result;
}

MythHDRTracker::MythHDRTracker(MythHDRPtr HDR)
  : m_hdrSupport(HDR)
{
}

