#ifndef MYTHDEBUGVULKAN_H
#define MYTHDEBUGVULKAN_H

// MythTV
#include "mythuiexp.h"
#include "vulkan/mythrendervulkan.h"

class MUI_PUBLIC MythDebugVulkan : protected MythVulkanObject
{
  public:
    static float s_DebugRed[4];
    static float s_DebugGreen[4];
    static float s_DebugBlue[4];
    static float s_DebugGray[4];
    static float s_DebugBlack[4];

    static MythDebugVulkan* Create(MythRenderVulkan* Render, VkDevice Device,
                                   QVulkanDeviceFunctions* Functions,
                                   MythWindowVulkan* Window);

    void BeginRegion (VkCommandBuffer CmdBuffer, const char* Name, const float *Color);
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
