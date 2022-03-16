#ifndef MYTHDRMRESOURCES_H
#define MYTHDRMRESOURCES_H

// Qt
#include <QString>

// MythTV
#include "libmythui/mythuiexp.h"

// libdrm
extern "C" {
#include <xf86drm.h>
#include <xf86drmMode.h>
}

// Std
#include <map>
#include <array>
#include <vector>
#include <memory>

using DRMArray = std::array<uint32_t,4>;

class MUI_PUBLIC MythDRMResources
{
  public:
    MythDRMResources(int FD);
   ~MythDRMResources();
    drmModeResPtr operator->() const;
    drmModeResPtr operator*() const;

  private:
    drmModeResPtr m_resources { nullptr };
};

#endif
