#ifndef MYTHDRMMODE_H
#define MYTHDRMMODE_H

// MythTV
#include "libmythui/platforms/drm/mythdrmresources.h"

using DRMMode  = std::shared_ptr<class MythDRMMode>;
using DRMModes = std::vector<DRMMode>;

class MUI_PUBLIC MythDRMMode
{
  public:
    static DRMMode Create(drmModeModeInfoPtr Mode, int Index);

    int      m_index  { 0 };
    double   m_rate   { 0.0 };
    uint16_t m_width  { 0 };
    uint16_t m_height { 0 };
    uint32_t m_flags  { 0 };
    QString  m_name;

  protected:
    explicit MythDRMMode(drmModeModeInfoPtr Mode, int Index);

  private:
    Q_DISABLE_COPY(MythDRMMode)
};

#endif
