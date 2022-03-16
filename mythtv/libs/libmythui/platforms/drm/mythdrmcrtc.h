#ifndef MYTHDRMCRTC_H
#define MYTHDRMCRTC_H

// MythTV
#include "libmythui/platforms/drm/mythdrmproperty.h"
#include "libmythui/platforms/drm/mythdrmmode.h"

using DRMCrtc  = std::shared_ptr<class MythDRMCrtc>;
using DRMCrtcs = std::vector<DRMCrtc>;

class MUI_PUBLIC MythDRMCrtc
{
  public:
    static DRMCrtc  Create(int FD, uint32_t Id, int Index = -1);
    static DRMCrtc  GetCrtc(const DRMCrtcs& Crtcs, uint32_t Id);
    static DRMCrtcs GetCrtcs(int FD);

    int      m_index  { -1 };
    uint32_t m_id     { 0 };
    uint32_t m_fbId   { 0 };
    DRMMode  m_mode   { nullptr };
    uint32_t m_x      { 0 };
    uint32_t m_y      { 0 };
    uint32_t m_width  { 0 };
    uint32_t m_height { 0 };
    DRMProps m_properties;

  protected:
    MythDRMCrtc(int FD, uint32_t Id, int Index);
    static int RetrieveCRTCIndex(int FD, uint32_t Id);

  private:
    Q_DISABLE_COPY(MythDRMCrtc)
};

#endif
