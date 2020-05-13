#ifndef MYTHVBOVULKAN_H
#define MYTHVBOVULKAN_H

// MythTV
#include "vulkan/mythrendervulkan.h"

class MythVertexBufferVulkan : protected MythVulkanObject
{
  public:
    static MythVertexBufferVulkan* Create(MythRenderVulkan* Render, VkDevice Device,
                                          QVulkanDeviceFunctions* Functions, VkDeviceSize Size);
   ~MythVertexBufferVulkan();

    VkBuffer GetBuffer       (void) const;
    bool     NeedsUpdate     (const QRect& Source, const QRect& Dest, int Alpha, int Rotation);
    void*    GetMappedMemory (void) const;
    void     Update          (const QRect& Source, const QRect& Dest, int Alpha, int Rotation,
                              VkCommandBuffer CommandBuffer = nullptr);

  protected:
    MythVertexBufferVulkan(MythRenderVulkan* Render, VkDevice Device,
                           QVulkanDeviceFunctions* Functions, VkDeviceSize Size);
  private:
    Q_DISABLE_COPY(MythVertexBufferVulkan)

    VkDeviceSize   m_bufferSize    { 0       };
    VkBuffer       m_buffer        { nullptr };
    VkDeviceMemory m_bufferMemory  { nullptr };
    VkBuffer       m_stagingBuffer { nullptr };
    VkDeviceMemory m_stagingMemory { nullptr };
    void*          m_mappedMemory  { nullptr };
    QRect          m_source;
    QRect          m_dest;
    int            m_rotation      { 0       };
    int            m_alpha         { 255     };
};

#endif // MYTHVBOVULKAN_H
