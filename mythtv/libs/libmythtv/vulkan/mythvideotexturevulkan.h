#ifndef MYTHVIDEOTEXTUREVULKAN_H
#define MYTHVIDEOTEXTUREVULKAN_H

// MythTV
#include "libmythui/vulkan/mythrendervulkan.h"
#include "mythframe.h"

// FFmpeg
extern "C" {
#include "libavutil/pixfmt.h"
}

// Std
#include <vector>

class MythVideoTextureVulkan
{
  public:
    static std::vector<MythVideoTextureVulkan*>
    CreateTextures(MythVulkanObject* Vulkan,
                   VkCommandBuffer   CommandBuffer,
                   VideoFrameType    Type,
                   VideoFrameType    Format,
                   QSize             Size);
    static void DeleteTextures(MythVulkanObject* Vulkan,
                               VkCommandBuffer   CommandBuffer,
                               std::vector<MythVideoTextureVulkan*>& Textures);
  protected:
    MythVideoTextureVulkan(VideoFrameType Type, VideoFrameType Format);
    MythVideoTextureVulkan() = default;

  private:
    Q_DISABLE_COPY(MythVideoTextureVulkan)
    static std::vector<MythVideoTextureVulkan*>
    CreateSoftwareTextures(MythVulkanObject* Vulkan,
                           VkCommandBuffer   CommandBuffer,
                           VideoFrameType    Type,
                           VideoFrameType    Format,
                           QSize             Size);
    static void DeleteTexture (MythVulkanObject* Vulkan,
                               VkCommandBuffer   CommandBuffer,
                               MythVideoTextureVulkan* Texture);

#if 0
    bool           m_valid       { false };
    VideoFrameType m_frameType   { FMT_NONE };
    VideoFrameType m_frameFormat { FMT_NONE };
    uint           m_plane       { 0 };
    uint           m_planeCount  { 0 };
#endif
};

#endif
