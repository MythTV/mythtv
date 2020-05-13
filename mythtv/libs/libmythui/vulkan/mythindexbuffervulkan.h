#ifndef MYTHINDEXBUFFERVULKAN_H
#define MYTHINDEXBUFFERVULKAN_H

// MythTV
#include "vulkan/mythrendervulkan.h"

class MythIndexBufferVulkan : protected MythVulkanObject
{
  public:
    static MythIndexBufferVulkan* Create(MythRenderVulkan* Render, VkDevice Device,
                                         QVulkanDeviceFunctions* Functions,
                                         const std::vector<uint16_t>& Indices);
   ~MythIndexBufferVulkan();

    VkBuffer GetBuffer (void) const;
    uint32_t GetSize   (void) const;

  protected:
    MythIndexBufferVulkan(MythRenderVulkan* Render, VkDevice Device,
                          QVulkanDeviceFunctions* Functions,
                          const std::vector<uint16_t>& Indices);

  private:
    uint32_t       m_size   { 0       };
    VkBuffer       m_buffer { nullptr };
    VkDeviceMemory m_memory { nullptr };
};

#endif // MYTHINDEXBUFFERVULKAN_H
