#ifndef MYTHTEXTUREVULKAN_H
#define MYTHTEXTUREVULKAN_H

// Qt
#include <QImage>

// MythTV
#include "vulkan/mythrendervulkan.h"
#include "vulkan/mythcombobuffervulkan.h"

class MythVertexBufferVulkan;
class MythUniformBufferVulkan;

class MythTextureVulkan : protected MythVulkanObject, public MythComboBufferVulkan
{
  public:
    static MythTextureVulkan* Create(MythRenderVulkan* Render, VkDevice Device,
                                     QVulkanDeviceFunctions* Functions,
                                     QImage *Image, VkSampler Sampler,
                                     VkCommandBuffer CommandBuffer = nullptr);
   ~MythTextureVulkan();

    void                  StagingFinished    (void);
    VkDescriptorImageInfo GetDescriptorImage (void) const;
    VkDescriptorSet       TakeDescriptor     (void);
    void                  AddDescriptor      (VkDescriptorSet Descriptor);

    VkDescriptorSet  m_descriptor   { nullptr };
    uint64_t         m_dataSize     { 0       };

  protected:
    MythTextureVulkan(MythRenderVulkan* Render, VkDevice Device,
                      QVulkanDeviceFunctions* Functions,
                      QImage *Image, VkSampler Sampler,
                      VkCommandBuffer CommandBuffer = nullptr);

  private:
    Q_DISABLE_COPY(MythTextureVulkan)

    VkBuffer         m_stagingBuffer  { nullptr };
    VkDeviceMemory   m_stagingMemory  { nullptr };
    VkImage          m_image          { nullptr };
    VkDeviceMemory   m_deviceMemory   { nullptr };
    bool             m_createdSampler { false   };
    VkSampler        m_sampler        { nullptr };
    VkImageView      m_view           { nullptr };
};

#endif
