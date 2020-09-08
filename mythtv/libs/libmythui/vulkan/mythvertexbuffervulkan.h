#ifndef MYTHVBOVULKAN_H
#define MYTHVBOVULKAN_H

// MythTV
#include "vulkan/mythrendervulkan.h"

class MUI_PUBLIC MythBufferVulkan  : protected MythVulkanObject
{
  public:
    static MythBufferVulkan* Create(MythRenderVulkan* Render, VkDevice Device,
                                    QVulkanDeviceFunctions* Functions, VkDeviceSize Size);
    virtual ~MythBufferVulkan();

    VkBuffer GetBuffer       () const;
    void*    GetMappedMemory () const;
    void     Update          (VkCommandBuffer CommandBuffer = nullptr);

  protected:
    MythBufferVulkan(MythRenderVulkan* Render, VkDevice Device,
                     QVulkanDeviceFunctions* Functions, VkDeviceSize Size);

    VkDeviceSize   m_bufferSize    { 0       };
    VkBuffer       m_buffer        { nullptr };
    VkDeviceMemory m_bufferMemory  { nullptr };
    VkBuffer       m_stagingBuffer { nullptr };
    VkDeviceMemory m_stagingMemory { nullptr };
    void*          m_mappedMemory  { nullptr };

  private:
    Q_DISABLE_COPY(MythBufferVulkan)
};

class MUI_PUBLIC MythVertexBufferVulkan : public MythBufferVulkan
{
  public:
    static MythVertexBufferVulkan* Create(MythRenderVulkan* Render, VkDevice Device,
                                          QVulkanDeviceFunctions* Functions, VkDeviceSize Size);

    bool     NeedsUpdate (const QRect& Source, const QRect& Dest, int Alpha, int Rotation);
    void     Update      (const QRect& Source, const QRect& Dest, int Alpha, int Rotation,
                          VkCommandBuffer CommandBuffer = nullptr);

  protected:
    MythVertexBufferVulkan(MythRenderVulkan* Render, VkDevice Device,
                           QVulkanDeviceFunctions* Functions, VkDeviceSize Size);

  private:
    QRect          m_source;
    QRect          m_dest;
    int            m_rotation { 0   };
    int            m_alpha    { 255 };
};

#endif
