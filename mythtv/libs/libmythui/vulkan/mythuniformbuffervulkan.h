#ifndef MYTHUNIFORMBUFFERVULKAN_H
#define MYTHUNIFORMBUFFERVULKAN_H

// MythTV
#include "libmythui/vulkan/mythrendervulkan.h"

class MUI_PUBLIC MythUniformBufferVulkan : protected MythVulkanObject
{
  public:
    static MythUniformBufferVulkan* Create(MythVulkanObject* Vulkan, VkDeviceSize Size);
   ~MythUniformBufferVulkan();

    VkDeviceSize           Size          (void) const;
    VkDescriptorBufferInfo GetBufferInfo (void) const;
    void                   Update        (void *Source);

  protected:
    MythUniformBufferVulkan(MythVulkanObject* Vulkan, VkDeviceSize Size);

  private:
    VkDeviceSize   m_bufferSize    { 0       };
    VkBuffer       m_buffer        { MYTH_NULL_DISPATCH };
    VkDeviceMemory m_bufferMemory  { MYTH_NULL_DISPATCH };
    void*          m_mappedMemory  { nullptr };
};

#endif
