#ifndef MYTHVULKANVIDEOOUTPUT_H
#define MYTHVULKANVIDEOOUTPUT_H

// MythTV
#include "mythvideooutgpu.h"
#include "libmythui/vulkan/mythrendervulkan.h"

class MythPainterVulkan;
class MythVideoVulkan;

#define VULKAN_RENDERER QString("vulkan")

class MythVideoOutputVulkan : public MythVideoOutputGPU, public MythVulkanObject
{
    Q_OBJECT

  public:
    static void        GetRenderOptions    (RenderOptions& Options);
    static QStringList GetAllowedRenderers (MythCodecID CodecId);

    MythVideoOutputVulkan(MythMainWindow* MainWindow, MythRenderVulkan* Render,
                          MythPainterVulkan* Painter, MythDisplay* Display,
                          const MythVideoProfilePtr& VideoProfile, QString& Profile);
   ~MythVideoOutputVulkan() override = default;

    bool Init(QSize VideoDim, QSize VideoDispDim, float Aspect,
              QRect DisplayVisibleRect, MythCodecID CodecId) override;
    void PrepareFrame (MythVideoFrame* Frame, FrameScanType Scan) override;
    void RenderFrame  (MythVideoFrame* Frame, FrameScanType Scan) override;
    void RenderEnd    () override;
    void EndFrame     () override;
};

#endif
