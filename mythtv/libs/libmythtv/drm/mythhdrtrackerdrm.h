#ifndef MYTHHDRTRACKERDRM_H
#define MYTHHDRTRACKERDRM_H

// MythTV
#include "mythhdrtracker.h"
#include "drm/mythvideodrmutils.h"
#include "platforms/mythdrmdevice.h"
#include "platforms/drm/mythdrmproperty.h"

using DRMMeta = hdr_output_metadata;

class MythHDRTrackerDRM : public MythHDRTracker
{
  public:
    static HDRTracker Create(MythDisplay* _Display);
   ~MythHDRTrackerDRM() override;
    void Update(MythVideoFrame* Frame) override;
    void Reset() override;

  protected:
    MythHDRTrackerDRM(class MythDisplayDRM* DRMDisplay, DRMProp HDRProp);

    MythDRMPtr m_device     { nullptr };
    DRMConn    m_connector  { nullptr };
    DRMProp    m_hdrProp    { nullptr };
    DRMCrtc    m_crtc       { nullptr };
    DRMProp    m_activeProp { nullptr };
    DRMMeta    m_drmMetadata;
    uint32_t   m_hdrBlob    { 0 };
};

#endif
