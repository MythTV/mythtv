#ifndef MYTHVIDEODRMBUFFER_H
#define MYTHVIDEODRMBUFFER_H

// MythTV
#include "libmythui/platforms/mythdrmdevice.h"

// Std
#include <memory>

// FFmpeg
extern "C" {
#include "libavutil/hwcontext_drm.h"
#include "libavutil/mem.h"
}

using DRMHandle = std::shared_ptr<class MythVideoDRMBuffer>;

class MythVideoDRMBuffer
{
  public:
    static DRMHandle Create(MythDRMPtr Device, AVDRMFrameDescriptor* DRMDesc, QSize Size);
   ~MythVideoDRMBuffer();
    uint32_t GetFB() const;

  protected:
    MythVideoDRMBuffer(MythDRMPtr Device, AVDRMFrameDescriptor* DRMDesc, QSize Size);

    bool       m_valid   { false   };
    MythDRMPtr m_device  { nullptr };
    uint32_t   m_fb      { 0 };
    DRMArray   m_handles { 0 };
};

#endif
