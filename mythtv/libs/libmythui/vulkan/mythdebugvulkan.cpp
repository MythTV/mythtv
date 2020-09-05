// MythTV
#include "mythlogging.h"
#include "vulkan/mythwindowvulkan.h"
#include "vulkan/mythdebugvulkan.h"

#define LOC QString("VulkanMarker: ")

VulkanDebugColor const MythDebugVulkan::s_DebugRed   { 1.0, 0.0, 0.0, 1.0 };
VulkanDebugColor const MythDebugVulkan::s_DebugGreen { 0.0, 1.0, 0.0, 1.0 };
VulkanDebugColor const MythDebugVulkan::s_DebugBlue  { 0.0, 0.0, 1.0, 1.0 };
VulkanDebugColor const MythDebugVulkan::s_DebugGray  { 0.5, 0.5, 0.5, 1.0 };
VulkanDebugColor const MythDebugVulkan::s_DebugBlack { 0.0, 0.0, 0.0, 1.0 };

MythDebugVulkan* MythDebugVulkan::Create(MythRenderVulkan *Render, VkDevice Device,
                                         QVulkanDeviceFunctions *Functions, MythWindowVulkan *Window)
{
    auto* result = new MythDebugVulkan(Render, Device, Functions, Window);
    if (result && !result->IsValid())
    {
        delete result;
        result = nullptr;
    }
    return result;
}

MythDebugVulkan::MythDebugVulkan(MythRenderVulkan *Render, VkDevice Device,
                                 QVulkanDeviceFunctions *Functions,
                                 MythWindowVulkan* Window)
  : MythVulkanObject(Render, Device, Functions, Window)
{
    if (!m_valid)
        return;

    auto extensions = m_window->supportedDeviceExtensions();
    if (!extensions.contains(VK_EXT_DEBUG_MARKER_EXTENSION_NAME))
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "Extension not found");
        return;
    }

    m_beginRegion = reinterpret_cast<PFN_vkCmdDebugMarkerBeginEXT>(
                m_window->vulkanInstance()->getInstanceProcAddr("vkCmdDebugMarkerBeginEXT"));
    m_endRegion = reinterpret_cast<PFN_vkCmdDebugMarkerEndEXT>(
                m_window->vulkanInstance()->getInstanceProcAddr("vkCmdDebugMarkerEndEXT"));
    m_nameObject = reinterpret_cast<PFN_vkDebugMarkerSetObjectNameEXT>(
                m_window->vulkanInstance()->getInstanceProcAddr("vkDebugMarkerSetObjectNameEXT"));

    m_valid = m_beginRegion && m_endRegion && m_nameObject;
    if (!m_valid)
        LOG(VB_GENERAL, LOG_INFO, LOC + "Failed to load procs");
}

void MythDebugVulkan::BeginRegion(VkCommandBuffer CmdBuffer, const char *Name, const VulkanDebugColor& Color)
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
    m_nameObject(m_device, &info);
}
