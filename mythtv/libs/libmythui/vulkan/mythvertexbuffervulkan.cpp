// MythTV
#include "mythlogging.h"
#include "vulkan/mythvertexbuffervulkan.h"

#define LOC QString("VulkanVBO: ")

MythVertexBufferVulkan* MythVertexBufferVulkan::Create(MythRenderVulkan *Render, VkDevice Device,
                                                       QVulkanDeviceFunctions *Functions, VkDeviceSize Size)
{
    MythVertexBufferVulkan* result = new MythVertexBufferVulkan(Render, Device, Functions, Size);
    if (result && !result->m_valid)
    {
        delete result;
        result = nullptr;
    }
    return result;
}

MythVertexBufferVulkan::MythVertexBufferVulkan(MythRenderVulkan *Render, VkDevice Device,
                                               QVulkanDeviceFunctions* Functions, VkDeviceSize Size)
  : MythVulkanObject(Render, Device, Functions),
    m_bufferSize(Size)
{
    if (!(Render && Device && Functions))
        return;

    if (m_render->CreateBuffer(Size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                               m_stagingBuffer, m_stagingMemory))
    {
        if (m_render->CreateBuffer(Size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                   m_buffer, m_bufferMemory))
        {
            m_valid = m_devFuncs->vkMapMemory(m_device, m_stagingMemory, 0, Size, 0, &m_mappedMemory) == VK_SUCCESS;
        }
    }
}

MythVertexBufferVulkan::~MythVertexBufferVulkan()
{
    if (m_device && m_devFuncs)
    {
        m_devFuncs->vkDestroyBuffer(m_device, m_stagingBuffer, nullptr);
        m_devFuncs->vkFreeMemory(m_device, m_stagingMemory, nullptr);
        m_devFuncs->vkDestroyBuffer(m_device, m_buffer, nullptr);
        m_devFuncs->vkFreeMemory(m_device, m_bufferMemory, nullptr);
    }
}

VkBuffer MythVertexBufferVulkan::GetBuffer(void) const
{
    return m_buffer;
}
void* MythVertexBufferVulkan::GetMappedMemory(void) const
{
    return m_mappedMemory;
}

void MythVertexBufferVulkan::Update(const QRect &Source, const QRect &Dest,
                                    int Alpha, int Rotation, VkCommandBuffer CommandBuffer)
{
    m_render->CopyBuffer(m_stagingBuffer, m_buffer, m_bufferSize, CommandBuffer);
    m_source   = Source;
    m_dest     = Dest;
    m_alpha    = Alpha;
    m_rotation = Rotation;
}

bool MythVertexBufferVulkan::NeedsUpdate(const QRect &Source, const QRect &Dest, int Alpha, int Rotation)
{
    return !((m_source == Source) && (m_dest == Dest) && (m_alpha == Alpha) && (m_rotation == Rotation));
}
