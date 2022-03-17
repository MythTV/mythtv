#ifndef MYTHVULKANVIDEO_H
#define MYTHVULKANVIDEO_H

// MythTV
#include "libmythui/vulkan/mythrendervulkan.h"

#include "mythframe.h"
#include "mythvideogpu.h"
#include "videoouttypes.h"

// Std
#include <vector>

class MythPainterVulkan;
class MythVideoVulkan;
class MythVideoTextureVulkan;

class MythVideoVulkan : public MythVideoGPU, public MythVulkanObject
{
    Q_OBJECT

  public:
    MythVideoVulkan(MythVulkanObject* Vulkan, MythVideoColourSpace* ColourSpace,
                    MythVideoBounds* Bounds, const MythVideoProfilePtr& VideoProfile, const QString &Profile);
   ~MythVideoVulkan() override;

    void    StartFrame       () override;
    void    PrepareFrame     (MythVideoFrame* Frame, FrameScanType /*Scan*/ = kScan_Progressive) override;
    void    RenderFrame      (MythVideoFrame* /*Frame*/, bool /*TopFieldFirst*/,
                              FrameScanType /*Scan*/, StereoscopicMode /*StereoOverride*/, bool /*DrawBorder*/ = false) override;
    void    EndFrame         () override;
    void    ResetFrameFormat () override;
    void    ResetTextures    () override {}

  protected:
    void    ColourSpaceUpdate(bool /*PrimariesChanged*/) override;

  private:
    bool    SetupFrameFormat (VideoFrameType InputType, VideoFrameType OutputType, QSize Size,
                              VkCommandBuffer CmdBuffer);

    std::vector<MythVideoTextureVulkan*> m_inputTextures;
};

#endif
