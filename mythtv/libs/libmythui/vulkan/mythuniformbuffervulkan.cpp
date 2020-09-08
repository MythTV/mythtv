// MythTV
#include "vulkan/mythuniformbuffervulkan.h"

MythUniformBufferVulkan* MythUniformBufferVulkan::Create(MythVulkanObject* Vulkan, VkDeviceSize Size)
{
    auto* result = new MythUniformBufferVulkan(Vulkan, Size);
    if (result && !result->IsValidVulkan())
    {
        delete result;
        result = nullptr;
    }
    return result;
}

MythUniformBufferVulkan::MythUniformBufferVulkan(MythVulkanObject* Vulkan, VkDeviceSize Size)
  : MythVulkanObject(Vulkan),
    m_bufferSize(Size)
{
    if (m_vulkanValid && m_vulkanRender->CreateBuffer(Size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                          m_buffer, m_bufferMemory))
    {
        m_vulkanValid = m_vulkanFuncs->vkMapMemory(m_vulkanDevice, m_bufferMemory, 0, Size, 0, &m_mappedMemory) == VK_SUCCESS;
    }
}

MythUniformBufferVulkan::~MythUniformBufferVulkan()
{
    if (m_vulkanValid)
    {
        m_vulkanFuncs->vkDestroyBuffer(m_vulkanDevice, m_buffer, nullptr);
        m_vulkanFuncs->vkFreeMemory(m_vulkanDevice, m_bufferMemory, nullptr);
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
