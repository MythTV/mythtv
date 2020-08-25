#ifndef MYTHDEBUGVULKAN_H
#define MYTHDEBUGVULKAN_H

// MythTV
#include "mythuiexp.h"
#include "vulkan/mythrendervulkan.h"

using VulkanDebugColor = std::array<float,4>;

class MUI_PUBLIC MythDebugVulkan : protected MythVulkanObject
{
  public:
    static const VulkanDebugColor s_DebugRed;
    static const VulkanDebugColor s_DebugGreen;
    static const VulkanDebugColor s_DebugBlue;
    static const VulkanDebugColor s_DebugGray;
    static const VulkanDebugColor s_DebugBlack;

    static MythDebugVulkan* Create(MythRenderVulkan* Render, VkDevice Device,
                                   QVulkanDeviceFunctions* Functions,
                                   MythWindowVulkan* Window);

    void BeginRegion (VkCommandBuffer CmdBuffer, const char* Name, const VulkanDebugColor& Color);
    void EndRegion   (VkCommandBuffer CmdBuffer);
    void NameObject  (uint64_t Object, VkDebugReportObjectTypeEXT Type, const char *Name);

  protected:
    MythDebugVulkan(MythRenderVulkan* Render, VkDevice Device,
                    QVulkanDeviceFunctions* Functions,
                    MythWindowVulkan* Window);

  private:
    MythWindowVulkan*                 m_window      { nullptr };
    PFN_vkCmdDebugMarkerBeginEXT      m_beginRegion { nullptr };
    PFN_vkCmdDebugMarkerEndEXT        m_endRegion   { nullptr };
    PFN_vkDebugMarkerSetObjectNameEXT m_nameObject  { nullptr };
};

#endif
