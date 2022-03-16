// MythTV
#include "libmythbase/mythlogging.h"
#include "vulkan/mythwindowvulkan.h"
#include "vulkan/mythdebugvulkan.h"

#define LOC QString("VulkanMarker: ")

MythVulkan4F const MythDebugVulkan::kDebugRed   { 1.0, 0.0, 0.0, 1.0 };
MythVulkan4F const MythDebugVulkan::kDebugGreen { 0.0, 1.0, 0.0, 1.0 };
MythVulkan4F const MythDebugVulkan::kDebugBlue  { 0.0, 0.0, 1.0, 1.0 };
MythVulkan4F const MythDebugVulkan::kDebugGray  { 0.5, 0.5, 0.5, 1.0 };
MythVulkan4F const MythDebugVulkan::kDebugBlack { 0.0, 0.0, 0.0, 1.0 };

MythDebugVulkan* MythDebugVulkan::Create(MythVulkanObject *Vulkan)
{
    auto * result = new MythDebugVulkan(Vulkan);
    if (result && !result->IsValidVulkan())
    {
        delete result;
        result = nullptr;
    }
    return result;
}

MythDebugVulkan::MythDebugVulkan(MythVulkanObject* Vulkan)
  : MythVulkanObject(Vulkan)
{
    if (!m_vulkanValid)
        return;

    m_vulkanValid = false;
    if (m_vulkanWindow->supportedDeviceExtensions().contains(VK_EXT_DEBUG_MARKER_EXTENSION_NAME))
    {
        m_beginRegion = reinterpret_cast<PFN_vkCmdDebugMarkerBeginEXT>(
                    m_vulkanWindow->vulkanInstance()->getInstanceProcAddr("vkCmdDebugMarkerBeginEXT"));
        m_endRegion = reinterpret_cast<PFN_vkCmdDebugMarkerEndEXT>(
                    m_vulkanWindow->vulkanInstance()->getInstanceProcAddr("vkCmdDebugMarkerEndEXT"));
        m_nameObject = reinterpret_cast<PFN_vkDebugMarkerSetObjectNameEXT>(
                    m_vulkanWindow->vulkanInstance()->getInstanceProcAddr("vkDebugMarkerSetObjectNameEXT"));
        if (m_beginRegion && m_endRegion && m_nameObject)
            m_vulkanValid = true;
        else
            LOG(VB_GENERAL, LOG_INFO, LOC + "Failed to load procs");
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "Extension not found");
    }
}

void MythDebugVulkan::BeginRegion(VkCommandBuffer CmdBuffer, const char *Name, const MythVulkan4F Color)
{
    VkDebugMarkerMarkerInfoEXT begin =
        { VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT, nullptr, Name,
          { Color[0], Color[1], Color[2], Color[3] } };
    m_beginRegion(CmdBuffer, &begin);
}

void MythDebugVulkan::EndRegion(VkCommandBuffer CmdBuffer)
{
    m_endRegion(CmdBuffer);
}

void MythDebugVulkan::NameObject(uint64_t Object, VkDebugReportObjectTypeEXT Type, const char *Name)
{
    VkDebugMarkerObjectNameInfoEXT info { };
    info.sType       = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
    info.objectType  = Type;
    info.object      = Object;
    info.pObjectName = Name;
    m_nameObject(m_vulkanDevice, &info);
}
