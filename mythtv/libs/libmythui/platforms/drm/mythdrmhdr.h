#ifndef MYTHDRMHDR_H
#define MYTHDRMHDR_H

// MythTV
#include "mythhdr.h"
#include "platforms/mythdrmdevice.h"

class MythDRMHDR : public MythHDR
{
  public:
    static MythHDRPtr Create(class MythDisplay* MDisplay, const MythHDRDesc& Desc);
   ~MythDRMHDR() override;
    void SetHDRMetadata(HDRType Type, const MythHDRMetaPtr& Metadata) override;

  protected:
    MythDRMHDR(const MythDRMPtr& Device, DRMProp HDRProp, const MythHDRDesc& Desc);

  private:
    void Cleanup();
    MythDRMPtr m_device     { nullptr };
    DRMConn    m_connector  { nullptr };
    DRMProp    m_hdrProp    { nullptr };
    DRMCrtc    m_crtc       { nullptr };
    DRMProp    m_activeProp { nullptr };
    uint32_t   m_hdrBlob    { 0 };
};

#endif
