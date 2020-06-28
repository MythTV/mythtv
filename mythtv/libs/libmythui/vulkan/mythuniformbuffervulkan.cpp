// MythTV
#include "vulkan/mythuniformbuffervulkan.h"

MythUniformBufferVulkan* MythUniformBufferVulkan::Create(MythRenderVulkan *Render,
                                                         VkDevice Device,
                                                         QVulkanDeviceFunctions *Functions,
                                                         VkDeviceSize Size)
{
    auto* result = new MythUniformBufferVulkan(Render, Device, Functions, Size);
    if (result && !result->m_valid)
    {
        delete result;
        result = nullptr;
    }
    return result;
}

MythUniformBufferVulkan::MythUniformBufferVulkan(MythRenderVulkan* Render, VkDevice Device,
                                                 QVulkanDeviceFunctions* Functions, VkDeviceSize Size)
  : MythVulkanObject(Render, Device, Functions),
    m_bufferSize(Size)
{
    if (Render->CreateBuffer(Size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                             m_buffer, m_bufferMemory))
    {
        m_valid = m_devFuncs->vkMapMemory(m_device, m_bufferMemory, 0, Size, 0, &m_mappedMemory) == VK_SUCCESS;
    }
}

MythUniformBufferVulkan::~MythUniformBufferVulkan()
{
    if (m_device && m_devFuncs)
    {
        m_devFuncs->vkDestroyBuffer(m_device, m_buffer, nullptr);
        m_devFuncs->vkFreeMemory(m_device, m_bufferMemory, nullptr);
    }
}

VkDeviceSize MythUniformBufferVulkan::Size(void) const
{
    return m_bufferSize;
}

VkDescriptorBufferInfo MythUniformBufferVulkan::GetBufferInfo(void) const
{
    return { m_buffer, 0, VK_WHOLE_SIZE };
}

void MythUniformBufferVulkan::Update(void *Source)
{
    if (Source)
        memcpy(m_mappedMemory, Source, m_bufferSize);
}
