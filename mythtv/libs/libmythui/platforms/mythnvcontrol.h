#ifndef MYTHNVCONTROL_H
#define MYTHNVCONTROL_H

#include <memory>

#include "mythvrr.h"

using NVControl = std::shared_ptr<class MythNVControl>;

class MythGSync : public MythVRR
{
  public:
    static inline bool s_gsyncResetOnExit  = false;
    static inline bool s_gsyncDefaultValue = false;
    static void ForceGSync(bool Enable);
    static MythVRRPtr CreateGSync(MythVRRRange Range);
   ~MythGSync() override;
    void SetEnabled(bool Enable = true) override;

  protected:
    MythGSync(NVControl Device, VRRType Type, bool Enabled, MythVRRRange Range);
    NVControl m_nvControl { nullptr };
};

#endif
