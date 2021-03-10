#ifndef MYTHDRMVRR_H
#define MYTHDRMVRR_H

// MythTV
#include "mythvrr.h"
#include "platforms/mythdrmdevice.h"

class MythDRMVRR : public MythVRR
{
  public:
    static inline bool s_freeSyncResetOnExit  = false;
    static inline bool s_freeSyncDefaultValue = false;

    static void       ForceFreeSync  (const MythDRMPtr& Device, bool Enable);
    static MythVRRPtr CreateFreeSync (const MythDRMPtr& Device, MythVRRRange Range);
   ~MythDRMVRR() override;

    void SetEnabled(bool Enable = true) override;
    DRMProp GetVRRProperty();

  protected:
    MythDRMVRR(MythDRMPtr Device, DRMProp VRRProp, bool Controllable,
               bool Enabled, MythVRRRange Range);

  private:
    MythDRMPtr m_device  { nullptr };
    DRMProp    m_vrrProp { nullptr };
};

#endif
