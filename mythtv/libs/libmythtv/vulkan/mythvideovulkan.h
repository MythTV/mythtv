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
class MythVideoTextureVulkan;

class MythVideoVulkan : public MythVideoGPU, public MythVulkanObject
{
    Q_OBJECT

  public:
    MythVideoVulkan(MythVulkanObject* Vulkan, MythVideoColourSpace* ColourSpace, MythVideoBounds* Bounds,
                    bool ViewportControl, const QString &Profile);
    MythVideoVulkan(MythVulkanObject* Vulkan, MythVideoColourSpace* ColourSpace,
                    QSize VideoDim, QSize VideoDispDim, QRect DisplayVisibleRect,
                    QRect DisplayVideoRect, QRect VideoRect,
                    bool ViewportControl, const QString& Profile);
   ~MythVideoVulkan() override;

    void    StartFrame       () override;
    void    PrepareFrame     (VideoFrame* Frame, FrameScanType /*Scan*/ = kScan_Progressive) override;
    void    RenderFrame      (VideoFrame* /*Frame*/, bool /*TopFieldFirst*/, FrameScanType /*Scan*/, StereoscopicMode /*Stereo*/, bool /*DrawBorder*/ = false) override;
    void    EndFrame         () override;
    void    ResetFrameFormat () override;
    void    ResetTextures    () override {}

  protected:
    void    ColourSpaceUpdate(bool /*PrimariesChanged*/) override;

  private:
    void    Init             () override;
    bool    SetupFrameFormat (VideoFrameType InputType, VideoFrameType OutputType, QSize Size,
                              VkCommandBuffer CmdBuffer);

    vector<MythVideoTextureVulkan*> m_inputTextures;
};

#endif
