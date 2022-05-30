#ifndef MYTHNVCONTROL_H
#define MYTHNVCONTROL_H

// MythTV
#include "mythvrr.h"

// Qt
#include <QLibrary>

// Std
#include <tuple>
#include <memory>

// X headers always last
#include "platforms/mythxdisplay.h"

using NVControl = std::shared_ptr<class MythNVControl>;
using QueryTargetBinary  = bool(*)(Display*,int,int,unsigned int,unsigned int, unsigned char**,int*);
using QueryScreenAttrib  = bool(*)(Display*,int,unsigned int,unsigned int,int*);
using QueryTargetAttrib  = bool(*)(Display*,int,int,unsigned int,unsigned int,int*);
using SetAttribute       = void(*)(Display*,int,unsigned int,unsigned int,int);

class MythGSync : public MythVRR
{
  public:
    static inline bool s_gsyncResetOnExit  = false;
    static inline bool s_gsyncDefaultValue = false;
    static void ForceGSync(bool Enable);
    static MythVRRPtr CreateGSync(const NVControl& Device, MythVRRRange Range);
   ~MythGSync() override;
    void SetEnabled(bool Enable = true) override;

  protected:
    MythGSync(NVControl Device, VRRType Type, bool Enabled, MythVRRRange Range);
    NVControl m_nvControl { nullptr };
};

class MythNVControl
{
  public:
    static NVControl Create();
   ~MythNVControl();

    int GetDisplayID() const;

  protected:
    MythNVControl(const QString& Path, MythXDisplay* MDisplay);
    QLibrary m_lib;

  public:
    MythXDisplay*     m_display     { nullptr };
    QueryTargetBinary m_queryBinary { nullptr };
    QueryScreenAttrib m_queryScreen { nullptr };
    QueryTargetAttrib m_queryTarget { nullptr };
    SetAttribute      m_setAttrib   { nullptr };
};
#endif
