#ifndef MYTHDEBUGVULKAN_H
#define MYTHDEBUGVULKAN_H

// MythTV
#include "mythuiexp.h"
#include "vulkan/mythrendervulkan.h"

class MUI_PUBLIC MythDebugVulkan : protected MythVulkanObject
{
  public:
    static const MythVulkan4F s_DebugRed;
    static const MythVulkan4F s_DebugGreen;
    static const MythVulkan4F s_DebugBlue;
    static const MythVulkan4F s_DebugGray;
    static const MythVulkan4F s_DebugBlack;

    static MythDebugVulkan* Create(MythVulkanObject* Vulkan);

    void BeginRegion (VkCommandBuffer CmdBuffer, const char* Name, const MythVulkan4F& Color);
    void EndRegion   (VkCommandBuffer CmdBuffer);
    void NameObject  (uint64_t Object, VkDebugReportObjectTypeEXT Type, const char *Name);

  protected:
    MythDebugVulkan(MythVulkanObject *Vulkan);

  private:
    PFN_vkCmdDebugMarkerBeginEXT      m_beginRegion { nullptr };
    PFN_vkCmdDebugMarkerEndEXT        m_endRegion   { nullptr };
    PFN_vkDebugMarkerSetObjectNameEXT m_nameObject  { nullptr };
};

#endif
