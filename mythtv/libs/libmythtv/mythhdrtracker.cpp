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

    // Check the EDID for HDR support first
    const auto & edid = _Display->GetEDID();
    if (!edid.Valid())
        return nullptr;

    auto [types, metadata] = edid.GetHDRSupport();

    // We only support HDR10 and HLG
    if (types < MythEDID::HDR10)
    {
        LOG(VB_PLAYBACK, LOG_INFO, "No HDR support detected for this display");
        return nullptr;
    }

    // We only know how to deal with HDR Static Metablock Type 1 (which is the only type)
    if ((metadata & MythEDID::Static1) != MythEDID::Static1)
        return nullptr;

    HDRTracker result = nullptr;
#ifdef USING_DRM_VIDEO
    result = MythHDRTrackerDRM::Create(_Display, types);
#endif

    if (!result.get())
        LOG(VB_PLAYBACK, LOG_INFO, "HDR signalling support not available");
    return result;
}

MythHDRTracker::MythHDRTracker(int HDRSupport)
  : m_hdrSupport(HDRSupport)
{
}

