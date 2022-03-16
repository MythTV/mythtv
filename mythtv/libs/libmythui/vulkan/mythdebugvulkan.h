#ifndef MYTHDEBUGVULKAN_H
#define MYTHDEBUGVULKAN_H

// MythTV
#include "libmythui/mythuiexp.h"
#include "libmythui/vulkan/mythrendervulkan.h"

class MUI_PUBLIC MythDebugVulkan : protected MythVulkanObject
{
  public:
    static const MythVulkan4F kDebugRed;
    static const MythVulkan4F kDebugGreen;
    static const MythVulkan4F kDebugBlue;
    static const MythVulkan4F kDebugGray;
    static const MythVulkan4F kDebugBlack;

    static MythDebugVulkan* Create(MythVulkanObject* Vulkan);

    void BeginRegion (VkCommandBuffer CmdBuffer, const char* Name, MythVulkan4F Color);
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
