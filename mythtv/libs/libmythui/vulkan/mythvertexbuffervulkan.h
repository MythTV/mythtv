#ifndef MYTHVBOVULKAN_H
#define MYTHVBOVULKAN_H

// MythTV
#include "libmythui/vulkan/mythrendervulkan.h"

class MUI_PUBLIC MythBufferVulkan  : protected MythVulkanObject
{
  public:
    static MythBufferVulkan* Create(MythVulkanObject* Vulkan, VkDeviceSize Size);
    virtual ~MythBufferVulkan();

    VkBuffer GetBuffer       () const;
    void*    GetMappedMemory () const;
    void     Update          (VkCommandBuffer CommandBuffer = nullptr);

  protected:
    MythBufferVulkan(MythVulkanObject* Vulkan, VkDeviceSize Size);

    VkDeviceSize   m_bufferSize    { 0       };
    VkBuffer       m_buffer        { MYTH_NULL_DISPATCH };
    VkDeviceMemory m_bufferMemory  { MYTH_NULL_DISPATCH };
    VkBuffer       m_stagingBuffer { MYTH_NULL_DISPATCH };
    VkDeviceMemory m_stagingMemory { MYTH_NULL_DISPATCH };
    void*          m_mappedMemory  { nullptr };

  private:
    Q_DISABLE_COPY(MythBufferVulkan)
};

class MUI_PUBLIC MythVertexBufferVulkan : public MythBufferVulkan
{
  public:
    static MythVertexBufferVulkan* Create(MythVulkanObject* Vulkan, VkDeviceSize Size);

    bool     NeedsUpdate (QRect Source, QRect Dest, int Alpha, int Rotation);
    void     Update      (QRect Source, QRect Dest, int Alpha, int Rotation,
                          VkCommandBuffer CommandBuffer = nullptr);

  protected:
    MythVertexBufferVulkan(MythVulkanObject* Vulkan, VkDeviceSize Size);

  private:
    QRect          m_source;
    QRect          m_dest;
    int            m_rotation { 0   };
    int            m_alpha    { 255 };
};

#endif
