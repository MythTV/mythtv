// MythTV
#include "libmythbase/mythlogging.h"
#include "vulkan/mythtexturevulkan.h"

#define LOC QString("VulkanTex: ")

MythTextureVulkan* MythTextureVulkan::Create(MythVulkanObject* Vulkan, QImage *Image,
                                             VkSampler Sampler, VkCommandBuffer CommandBuffer)
{
    MythTextureVulkan* result = nullptr;
    if (Image)
        result = new MythTextureVulkan(Vulkan, Image, Sampler, CommandBuffer);

    if (result && !result->IsValidVulkan())
    {
        delete result;
        result = nullptr;
    }
    return result;
}

MythTextureVulkan::MythTextureVulkan(MythVulkanObject *Vulkan,
                                     QImage *Image, VkSampler Sampler,
                                     VkCommandBuffer CommandBuffer)
  : MythVulkanObject(Vulkan),
    MythComboBufferVulkan(Image->width(), Image->height())
{
    if (!m_vulkanValid)
        return;

    // retrieve and check Image data
    auto datasize = static_cast<VkDeviceSize>((Image->width() * Image->height() * Image->depth()) >> 3);
    auto datasize2 = Image->sizeInBytes();
    if (datasize != static_cast<VkDeviceSize>(datasize2))
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Inconsistent image data size");

    const auto *data = Image->constBits();

    // Create staging buffer
    if (!m_vulkanRender->CreateBuffer(datasize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                m_stagingBuffer, m_stagingMemory))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create staging buffer");
        return;
    }

    // Map memory and copy data
    void* memory = nullptr;
    m_vulkanFuncs->vkMapMemory(m_vulkanDevice, m_stagingMemory, 0, datasize, 0, &memory);
    memcpy(memory, data, static_cast<size_t>(datasize));
    m_vulkanFuncs->vkUnmapMemory(m_vulkanDevice, m_stagingMemory);

    // Create image
    if (m_vulkanRender->CreateImage(Image->size(), VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
                              VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_image, m_deviceMemory))
    {
        VkCommandBuffer commandbuffer = CommandBuffer ? CommandBuffer : m_vulkanRender->CreateSingleUseCommandBuffer();

        // Transition
        m_vulkanRender->TransitionImageLayout(m_image, VK_IMAGE_LAYOUT_UNDEFINED,
                                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, commandbuffer);
        m_vulkanRender->CopyBufferToImage(m_stagingBuffer, m_image, static_cast<uint32_t>(Image->width()),
                                    static_cast<uint32_t>(Image->height()), commandbuffer);
        m_vulkanRender->TransitionImageLayout(m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandbuffer);

        // Create sampler if needed
        if (!Sampler)
        {
            m_sampler = m_vulkanRender->CreateSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR);
            m_createdSampler = true;
        }
        else
        {
            m_sampler = Sampler;
        }

        // Create image view
        VkImageViewCreateInfo viewinfo { };
        viewinfo.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewinfo.image    = m_image;
        viewinfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewinfo.format   = VK_FORMAT_B8G8R8A8_UNORM;
        viewinfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewinfo.subresourceRange.baseMipLevel = 0;
        viewinfo.subresourceRange.levelCount = 1;
        viewinfo.subresourceRange.baseArrayLayer = 0;
        viewinfo.subresourceRange.layerCount = 1;
        if (m_vulkanFuncs->vkCreateImageView(m_vulkanDevice, &viewinfo, nullptr, &m_view) != VK_SUCCESS)
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create image view");

        m_dataSize = datasize;
        m_vulkanValid = m_sampler && m_view;

        if (!CommandBuffer)
            m_vulkanRender->FinishSingleUseCommandBuffer(commandbuffer);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create texture image");
    }

    // If this is a single use command buffer, staging is complete and we can
    // release the staging resources
    if (!CommandBuffer)
        StagingFinished();
}

MythTextureVulkan::~MythTextureVulkan()
{
    if (m_descriptor)
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Texture has not returned descriptor");

    if (m_vulkanValid)
    {
        StagingFinished();
        if (m_createdSampler)
            m_vulkanFuncs->vkDestroySampler(m_vulkanDevice, m_sampler, nullptr);
        m_vulkanFuncs->vkDestroyImageView(m_vulkanDevice, m_view, nullptr);
        m_vulkanFuncs->vkDestroyImage(m_vulkanDevice, m_image, nullptr);
        m_vulkanFuncs->vkFreeMemory(m_vulkanDevice, m_deviceMemory, nullptr);
    }
}

void MythTextureVulkan::StagingFinished(void)
{
    m_vulkanFuncs->vkDestroyBuffer(m_vulkanDevice, m_stagingBuffer, nullptr);
    m_vulkanFuncs->vkFreeMemory(m_vulkanDevice, m_stagingMemory, nullptr);
    m_stagingBuffer = MYTH_NULL_DISPATCH;
    m_stagingMemory = MYTH_NULL_DISPATCH;
}

VkDescriptorImageInfo MythTextureVulkan::GetDescriptorImage(void) const
{
    return { m_sampler, m_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
}

void MythTextureVulkan::AddDescriptor(VkDescriptorSet Descriptor)
{
    m_descriptor = Descriptor;
}

VkDescriptorSet MythTextureVulkan::TakeDescriptor(void)
{
    VkDescriptorSet result = m_descriptor;
    m_descriptor = MYTH_NULL_DISPATCH;
    return result;
}
