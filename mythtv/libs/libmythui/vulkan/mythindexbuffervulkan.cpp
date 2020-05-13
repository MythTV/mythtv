// MythTV
#include "vulkan/mythindexbuffervulkan.h"

MythIndexBufferVulkan* MythIndexBufferVulkan::Create(MythRenderVulkan *Render, VkDevice Device,
                                                     QVulkanDeviceFunctions *Functions,
                                                     const std::vector<uint16_t> &Indices)
{
    MythIndexBufferVulkan* result = new MythIndexBufferVulkan(Render, Device, Functions, Indices);
    if (result && !result->m_valid)
    {
        delete result;
        result = nullptr;
    }
    return result;
}

MythIndexBufferVulkan::MythIndexBufferVulkan(MythRenderVulkan *Render, VkDevice Device,
                                             QVulkanDeviceFunctions *Functions,
                                             const std::vector<uint16_t> &Indices)
  : MythVulkanObject(Render, Device, Functions)
{
    m_size = static_cast<uint32_t>(Indices.size());
    VkDeviceSize size = sizeof(Indices[0]) * Indices.size();
    VkBuffer       stagingbuf = nullptr;
    VkDeviceMemory stagingmem = nullptr;
    if (Render->CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                             stagingbuf, stagingmem))
    {
        void* tmp;
        m_devFuncs->vkMapMemory(m_device, stagingmem, 0, size, 0, &tmp);
        memcpy(tmp, Indices.data(), static_cast<size_t>(size));
        m_devFuncs->vkUnmapMemory(m_device, stagingmem);

        if (Render->CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                 m_buffer, m_memory))
        {
            Render->CopyBuffer(stagingbuf, m_buffer, size);
            m_valid = true;
        }

        m_devFuncs->vkDestroyBuffer(m_device, stagingbuf, nullptr);
        m_devFuncs->vkFreeMemory(m_device, stagingmem, nullptr);
    }
}

MythIndexBufferVulkan::~MythIndexBufferVulkan()
{
    if (m_device && m_devFuncs)
    {
        m_devFuncs->vkDestroyBuffer(m_device, m_buffer, nullptr);
        m_devFuncs->vkFreeMemory(m_device, m_memory, nullptr);
    }
}

VkBuffer MythIndexBufferVulkan::GetBuffer(void) const
{
    return m_buffer;
}

uint32_t MythIndexBufferVulkan::GetSize(void) const
{
    return m_size;
}
