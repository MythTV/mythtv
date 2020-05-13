// Std
#include <algorithm>

// MythTV
#include "mythlogging.h"
#include "vulkan/mythvertexbuffervulkan.h"
#include "vulkan/mythuniformbuffervulkan.h"
#include "vulkan/mythtexturevulkan.h"

#define LOC QString("VulkanTex: ")

MythTextureVulkan* MythTextureVulkan::Create(MythRenderVulkan* Render, VkDevice Device,
                                             QVulkanDeviceFunctions* Functions, QImage *Image)
{
    MythTextureVulkan* result = new MythTextureVulkan(Render, Device, Functions, Image);
    if (result && !result->m_valid)
    {
        delete result;
        result = nullptr;
    }
    return result;
}

MythTextureVulkan::MythTextureVulkan(MythRenderVulkan* Render, VkDevice Device,
                                     QVulkanDeviceFunctions* Functions, QImage *Image)
  : MythVulkanObject(Render, Device, Functions)
{
    if (!(Render && Device && Functions && Image))
        return;

    // create vertex buffer object
    m_vertexBuffer = MythVertexBufferVulkan::Create(Render, Device, Functions, 4 * 8 * sizeof(float));
    if (!m_vertexBuffer)
        return;

    // retrieve and check Image data
    auto datasize = static_cast<VkDeviceSize>((Image->width() * Image->height() * Image->depth()) >> 3);
    auto datasize2 =
#if QT_VERSION >= QT_VERSION_CHECK(5,10,0)
                     Image->sizeInBytes();
#else
                     Image->byteCount();
#endif
    if (datasize != static_cast<VkDeviceSize>(datasize2))
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Inconsistent image data size");

    auto data = Image->constBits();

    // Create staging buffer
    VkBuffer stagingbuf;
    VkDeviceMemory stagingmem;
    if (!Render->CreateBuffer(datasize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                              stagingbuf, stagingmem))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create staging buffer");
        return;
    }

    // Map memory and copy data
    void* memory;
    Functions->vkMapMemory(Device, stagingmem, 0, datasize, 0, &memory);
    memcpy(memory, data, static_cast<size_t>(datasize));
    Functions->vkUnmapMemory(Device, stagingmem);

    // Create image
    if (Render->CreateImage(Image->size(), VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
                            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_image, m_deviceMemory))
    {
        // Transition
        Render->TransitionImageLayout(m_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        Render->CopyBufferToImage(stagingbuf, m_image, static_cast<uint32_t>(Image->width()),
                                  static_cast<uint32_t>(Image->height()));
        Render->TransitionImageLayout(m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        // Create sampler
        VkSamplerCreateInfo samplerinfo { };
        memset(&samplerinfo, 0, sizeof(samplerinfo));
        samplerinfo.sType           = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerinfo.magFilter        = VK_FILTER_LINEAR;
        samplerinfo.minFilter        = VK_FILTER_LINEAR;
        samplerinfo.addressModeU     = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerinfo.addressModeV     = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerinfo.addressModeW     = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerinfo.anisotropyEnable = VK_FALSE;
        samplerinfo.maxAnisotropy    = 1;
        samplerinfo.borderColor      = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
        samplerinfo.unnormalizedCoordinates = VK_FALSE;
        samplerinfo.compareEnable    = VK_FALSE;
        samplerinfo.compareOp        = VK_COMPARE_OP_ALWAYS;
        samplerinfo.mipmapMode       = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        if (Functions->vkCreateSampler(Device, &samplerinfo, nullptr, &m_sampler) != VK_SUCCESS)
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create image sampler");

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
        if (Functions->vkCreateImageView(Device, &viewinfo, nullptr, &m_view) != VK_SUCCESS)
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create image view");

        m_width    = static_cast<uint32_t>(Image->width());
        m_height   = static_cast<uint32_t>(Image->height());
        m_dataSize = datasize;

        m_valid = m_sampler && m_view;
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create texture image");
    }

    Functions->vkDestroyBuffer(Device, stagingbuf, nullptr);
    Functions->vkFreeMemory(Device, stagingmem, nullptr);

    m_crop = true;
}

MythTextureVulkan::~MythTextureVulkan()
{
    if (m_descriptor)
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Texture has not returned descriptor");

    delete m_vertexBuffer;
    delete m_uniform;
    if (m_device && m_devFuncs)
    {
        m_devFuncs->vkDestroySampler(m_device, m_sampler, nullptr);
        m_devFuncs->vkDestroyImageView(m_device, m_view, nullptr);
        m_devFuncs->vkDestroyImage(m_device, m_image, nullptr);
        m_devFuncs->vkFreeMemory(m_device, m_deviceMemory, nullptr);
    }
}

VkBuffer MythTextureVulkan::GetVertexBuffer(void) const
{
    return m_vertexBuffer->GetBuffer();
}

VkDescriptorImageInfo MythTextureVulkan::GetDescriptorImage(void) const
{
    return { m_sampler, m_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
}

void MythTextureVulkan::AddUniform(MythUniformBufferVulkan *Uniform)
{
    m_uniform = Uniform;
}

void MythTextureVulkan::AddDescriptor(VkDescriptorSet Descriptor)
{
    m_descriptor = Descriptor;
}

VkDescriptorSet MythTextureVulkan::TakeDescriptor(void)
{
    VkDescriptorSet result = m_descriptor;
    m_descriptor = nullptr;
    return result;
}

/*! \brief Update the textures vertex buffer object if necessary
 *
 * \todo Need to use a 'cached' vertex buffer when rendering the same image multiple times
 * \todo Handle flip and rotation
 * \todo No need for full colour support - only need alpha
*/
void MythTextureVulkan::UpdateVertices(const QRect &Source, const QRect &Destination,
                                       int Alpha, int Rotation, VkCommandBuffer CommandBuffer)
{
    if (!m_vertexBuffer->NeedsUpdate(Source, Destination, Alpha, Rotation))
        return;

    float* data = static_cast<float*>(m_vertexBuffer->GetMappedMemory());

    int width  = m_crop ? std::min(Source.width(),  static_cast<int>(m_width))  : Source.width();
    int height = m_crop ? std::min(Source.height(), static_cast<int>(m_height)) : Source.height();

    // X, Y  tX, Ty  R,G,B,A
    // BottomLeft, TopLeft, TopRight, BottomRight

    // Position
    data[ 0] = data[ 8] = Destination.left();
    data[ 9] = data[17] = Destination.top();
    data[16] = data[24] = Destination.left() + width;
    data[ 1] = data[25] = Destination.top()  + height;

    // Texcoord
    data[ 2] = data[10] = Source.left() / static_cast<float>(m_width);
    data[11] = data[19] = Source.top() / static_cast<float>(m_height);
    data[18] = data[26] = (Source.left() + width) / static_cast<float>(m_width);
    data[ 3] = data[27] = (Source.top() + height) / static_cast<float>(m_height);

    // Color
    data[ 7] = data[15] = data[23] = data[31] = Alpha / 255.0F;
    data[ 4] = data[ 5] = data[ 6] =
    data[12] = data[13] = data[14] =
    data[20] = data[21] = data[22] =
    data[28] = data[29] = data[30] = 1.0f;

    m_vertexBuffer->Update(Source, Destination, Alpha, Rotation, CommandBuffer);
}
