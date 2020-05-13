#ifndef MYTHTEXTUREVULKAN_H
#define MYTHTEXTUREVULKAN_H

// Qt
#include <QImage>

// MythTV
#include "vulkan/mythrendervulkan.h"

class MythVertexBufferVulkan;
class MythUniformBufferVulkan;

class MythTextureVulkan : protected MythVulkanObject
{
  public:
    static MythTextureVulkan* Create(MythRenderVulkan* Render, VkDevice Device,
                                     QVulkanDeviceFunctions* Functions, QImage *Image);
   ~MythTextureVulkan();

    void     UpdateVertices  (const QRect& Source, const QRect& Destination, int Alpha, int Rotation = 0,
                              VkCommandBuffer CommandBuffer = nullptr);
    VkDescriptorImageInfo GetDescriptorImage (void) const;
    VkDescriptorSet       TakeDescriptor     (void);
    VkBuffer GetVertexBuffer (void) const;
    void     AddUniform      (MythUniformBufferVulkan* Uniform);
    void     AddDescriptor   (VkDescriptorSet Descriptor);


    VkDescriptorSet  m_descriptor   { nullptr };
    uint64_t         m_dataSize     { 0       };
    MythVertexBufferVulkan* m_vertexBuffer { nullptr };
    MythUniformBufferVulkan* m_uniform { nullptr };

  protected:
    MythTextureVulkan(MythRenderVulkan* Render, VkDevice Device,
                      QVulkanDeviceFunctions* Functions, QImage *Image);

  private:
    Q_DISABLE_COPY(MythTextureVulkan)

    VkImage          m_image        { nullptr };
    VkDeviceMemory   m_deviceMemory { nullptr };
    VkSampler        m_sampler      { nullptr };
    VkImageView      m_view         { nullptr };
    uint32_t         m_width        { 0       };
    uint32_t         m_height       { 0       };
    bool             m_crop         { false   };
};

#endif
