#ifndef MYTHUNIFORMBUFFERVULKAN_H
#define MYTHUNIFORMBUFFERVULKAN_H

// MythTV
#include "vulkan/mythrendervulkan.h"

class MythUniformBufferVulkan : protected MythVulkanObject
{
  public:
    static MythUniformBufferVulkan* Create(MythRenderVulkan* Render, VkDevice Device,
                                           QVulkanDeviceFunctions* Functions, VkDeviceSize Size);
   ~MythUniformBufferVulkan();

    VkDeviceSize           Size          (void) const;
    VkDescriptorBufferInfo GetBufferInfo (void) const;
    void                   Update        (void *Source);

  protected:
    MythUniformBufferVulkan(MythRenderVulkan* Render, VkDevice Device,
                            QVulkanDeviceFunctions* Functions, VkDeviceSize Size);

  private:
    VkDeviceSize   m_bufferSize    { 0       };
    VkBuffer       m_buffer        { nullptr };
    VkDeviceMemory m_bufferMemory  { nullptr };
    void*          m_mappedMemory  { nullptr };
};

#endif // MYTHUNIFORMBUFFERVULKAN_H
