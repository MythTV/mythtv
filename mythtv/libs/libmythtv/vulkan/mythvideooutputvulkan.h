#ifndef MYTHVULKANVIDEOOUTPUT_H
#define MYTHVULKANVIDEOOUTPUT_H

// MythTV
#include "mythvideooutgpu.h"
#include "vulkan/mythrendervulkan.h"

class MythPainterVulkan;
class MythVideoVulkan;

#define VULKAN_RENDERER QString("vulkan")

class MythVideoOutputVulkan : public MythVideoOutputGPU, public MythVulkanObject
{
  public:
    static void        GetRenderOptions    (RenderOptions& Options);
    static QStringList GetAllowedRenderers (MythCodecID CodecId);
    static VideoFrameTypeVec s_vulkanFrameTypes;

    MythVideoOutputVulkan(QString& Profile);
   ~MythVideoOutputVulkan() override = default;

    bool StereoscopicModesAllowed () const override { return false; }

    bool Init(const QSize& VideoDim, const QSize& VideoDispDim, float Aspect,
              const QRect& DisplayVisibleRect, MythCodecID CodecId) override;
    void PrepareFrame (VideoFrame* Frame, FrameScanType Scan) override;
    void RenderFrame  (VideoFrame* Frame, FrameScanType Scan, OSD* Osd) override;
    void EndFrame     () override;
};

#endif
