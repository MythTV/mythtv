#ifndef MYTHTEXTUREVULKAN_H
#define MYTHTEXTUREVULKAN_H

// Qt
#include <QImage>

// MythTV
#include "vulkan/mythrendervulkan.h"
#include "vulkan/mythcombobuffervulkan.h"

class MythVertexBufferVulkan;
class MythUniformBufferVulkan;

class MUI_PUBLIC MythTextureVulkan : protected MythVulkanObject, public MythComboBufferVulkan
{
  public:
    static MythTextureVulkan* Create(MythVulkanObject* Vulkan, QImage *Image, VkSampler Sampler,
                                     VkCommandBuffer CommandBuffer = nullptr);
   ~MythTextureVulkan();

    void                  StagingFinished    (void);
    VkDescriptorImageInfo GetDescriptorImage (void) const;
    VkDescriptorSet       TakeDescriptor     (void);
    void                  AddDescriptor      (VkDescriptorSet Descriptor);

    VkDescriptorSet  m_descriptor   { MYTH_NULL_DISPATCH };
    uint64_t         m_dataSize     { 0 };

  protected:
    MythTextureVulkan(MythVulkanObject* Vulkan, QImage *Image, VkSampler Sampler,
                      VkCommandBuffer CommandBuffer = nullptr);

  private:
    Q_DISABLE_COPY(MythTextureVulkan)

    VkBuffer         m_stagingBuffer  { MYTH_NULL_DISPATCH };
    VkDeviceMemory   m_stagingMemory  { MYTH_NULL_DISPATCH };
    VkImage          m_image          { MYTH_NULL_DISPATCH };
    VkDeviceMemory   m_deviceMemory   { MYTH_NULL_DISPATCH };
    bool             m_createdSampler { false };
    VkSampler        m_sampler        { MYTH_NULL_DISPATCH };
    VkImageView      m_view           { MYTH_NULL_DISPATCH };
};

#endif
