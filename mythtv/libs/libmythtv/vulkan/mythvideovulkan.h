#ifndef MYTHVULKANVIDEO_H
#define MYTHVULKANVIDEO_H

// MythTV
#include "mythframe.h"
#include "videoouttypes.h"
#include "mythvideogpu.h"
#include "vulkan/mythrendervulkan.h"

// Std
#include <vector>
using std::vector;

class MythPainterVulkan;
class MythVideoVulkan;
class MythWindowVulkan;
class MythDebugVulkan;
class MythVideoTextureVulkan;

class MythVideoVulkan : public MythVideoGPU
{
    Q_OBJECT

  public:
    MythVideoVulkan(MythRender* Render, VideoColourSpace* ColourSpace, const VideoOutWindow& Window,
                    bool ViewportControl, QString Profile);
    MythVideoVulkan(MythRender* Render, VideoColourSpace* ColourSpace,
                    QSize VideoDim, QSize VideoDispDim, QRect DisplayVisibleRect,
                    QRect DisplayVideoRect, QRect VideoRect,
                    bool ViewportControl, QString Profile);
   ~MythVideoVulkan() override;

    void    StartFrame       () override;
    void    PrepareFrame     (VideoFrame* Frame, FrameScanType = kScan_Progressive) override;
    void    RenderFrame      (VideoFrame*, bool, FrameScanType, StereoscopicMode, bool = false) override;
    void    EndFrame         () override;
    void    ResetFrameFormat () override;
    void    ResetTextures    () override {}

  protected:
    void    ColourSpaceUpdate(bool /*PrimariesChanged*/) override;

  private:
    void    Init             () override;
    bool    SetupFrameFormat (VideoFrameType InputType, VideoFrameType OutputType, QSize Size,
                              VkCommandBuffer CmdBuffer);

    MythRenderVulkan*  m_vulkanRender  { nullptr };
    MythWindowVulkan*  m_vulkanWindow  { nullptr };
    VkDevice           m_device        { nullptr };
    QVulkanDeviceFunctions* m_devFuncs { nullptr };
    MythDebugVulkan*   m_debugMarker   { nullptr };
    vector<MythVideoTextureVulkan*> m_inputTextures;
};

#endif
