// MythTV
#include "libmythbase/mythlogging.h"
#include "vulkan/mythvertexbuffervulkan.h"

#define LOC QString("VulkanBuf: ")

MythBufferVulkan* MythBufferVulkan::Create(MythVulkanObject *Vulkan, VkDeviceSize Size)
{
    auto* result = new MythBufferVulkan(Vulkan, Size);
    if (result && !result->IsValidVulkan())
    {
        delete result;
        result = nullptr;
    }
    return result;
}

MythBufferVulkan::MythBufferVulkan(MythVulkanObject *Vulkan, VkDeviceSize Size)
  : MythVulkanObject(Vulkan),
    m_bufferSize(Size)
{
    if (m_vulkanValid && m_vulkanRender->CreateBuffer(Size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                          m_stagingBuffer, m_stagingMemory))
    {
        if (m_vulkanRender->CreateBuffer(Size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                   m_buffer, m_bufferMemory))
        {
            m_vulkanValid = m_vulkanFuncs->vkMapMemory(m_vulkanDevice, m_stagingMemory, 0, Size, 0, &m_mappedMemory) == VK_SUCCESS;
        }
    }
}

MythBufferVulkan::~MythBufferVulkan()
{
    if (m_vulkanValid)
    {
        m_vulkanFuncs->vkDestroyBuffer(m_vulkanDevice, m_stagingBuffer, nullptr);
        m_vulkanFuncs->vkFreeMemory(m_vulkanDevice, m_stagingMemory, nullptr);
        m_vulkanFuncs->vkDestroyBuffer(m_vulkanDevice, m_buffer, nullptr);
        m_vulkanFuncs->vkFreeMemory(m_vulkanDevice, m_bufferMemory, nullptr);
    }
}

VkBuffer MythBufferVulkan::GetBuffer(void) const
{
    return m_buffer;
}

void* MythBufferVulkan::GetMappedMemory(void) const
{
    return m_mappedMemory;
}

void MythBufferVulkan::Update(VkCommandBuffer CommandBuffer)
{
    m_vulkanRender->CopyBuffer(m_stagingBuffer, m_buffer, m_bufferSize, CommandBuffer);
}

MythVertexBufferVulkan* MythVertexBufferVulkan::Create(MythVulkanObject *Vulkan, VkDeviceSize Size)
{
    auto* result = new MythVertexBufferVulkan(Vulkan, Size);
    if (result && !result->IsValidVulkan())
    {
        delete result;
        result = nullptr;
    }
    return result;
}

MythVertexBufferVulkan::MythVertexBufferVulkan(MythVulkanObject *Vulkan, VkDeviceSize Size)
  : MythBufferVulkan(Vulkan, Size)
{
}

void MythVertexBufferVulkan::Update(const QRect Source, const QRect Dest,
                                    int Alpha, int Rotation, VkCommandBuffer CommandBuffer)
{
    m_vulkanRender->CopyBuffer(m_stagingBuffer, m_buffer, m_bufferSize, CommandBuffer);
    m_source   = Source;
    m_dest     = Dest;
    m_alpha    = Alpha;
    m_rotation = Rotation;
}

bool MythVertexBufferVulkan::NeedsUpdate(const QRect Source, const QRect Dest, int Alpha, int Rotation)
{
    return !(m_source == Source) || !(m_dest == Dest) || (m_alpha != Alpha) || (m_rotation != Rotation);
}
