#ifndef MYTHVIDEOTEXTUREVULKAN_H
#define MYTHVIDEOTEXTUREVULKAN_H

// MythTV
#include "vulkan/mythrendervulkan.h"
#include "mythframe.h"

// FFmpeg
extern "C" {
#include "libavutil/pixfmt.h"
}

// Std
#include <vector>
using std::vector;

class MythVideoTextureVulkan
{
  public:
    static vector<MythVideoTextureVulkan*> CreateTextures(MythRenderVulkan* Render,
                                                          VkDevice          Device,
                                                          QVulkanDeviceFunctions* Functions,
                                                          VkCommandBuffer   CommandBuffer,
                                                          VideoFrameType    Type,
                                                          VideoFrameType    Format,
                                                          QSize             Size);
    static void                            DeleteTextures(MythRenderVulkan* Render,
                                                          VkDevice          Device,
                                                          QVulkanDeviceFunctions* Functions,
                                                          VkCommandBuffer   CommandBuffer,
                                                          vector<MythVideoTextureVulkan*>& Textures);
  protected:
    MythVideoTextureVulkan();

  private:
    static vector<MythVideoTextureVulkan*> CreateSoftwareTextures(MythRenderVulkan* Render,
                                                                  VkDevice          Device,
                                                                  QVulkanDeviceFunctions* Functions,
                                                                  VkCommandBuffer   CommandBuffer,
                                                                  VideoFrameType    Type,
                                                                  VideoFrameType    Format,
                                                                  QSize             Size);
    static void                            DeleteTexture (MythRenderVulkan* Render,
                                                          VkDevice          Device,
                                                          QVulkanDeviceFunctions* Functions,
                                                          VkCommandBuffer   CommandBuffer,
                                                          MythVideoTextureVulkan* Texture);

    bool           m_valid       { false };
    VideoFrameType m_frameType   { FMT_NONE };
    VideoFrameType m_frameFormat { FMT_NONE };
    uint           m_plane       { 0 };
    uint           m_planeCount  { 0 };
};

#endif
