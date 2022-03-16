// MythTV
#include "libmythbase/mythlogging.h"
#include "vulkan/mythrendervulkan.h"
#include "vulkan/mythwindowvulkan.h"

#define LOC QString("VulkanWindow: ")

MythWindowVulkan::MythWindowVulkan(MythRenderVulkan *Render)
  : m_render(Render)
{
    // Most drivers/devices only seem to support these formats. Prefer UNORM.
    QVector<VkFormat> formats = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_SRGB };
    setPreferredColorFormats(formats);
    setFlags(QVulkanWindow::PersistentResources);

    // Enable dubug markers as appropriate
    // Note: These will only work when run *from* a debugger (e.g. renderdoc)
    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        setDeviceExtensions(QByteArrayList() << VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
}

MythWindowVulkan::~MythWindowVulkan() = default;

QVulkanWindowRenderer* MythWindowVulkan::createRenderer(void)
{
    return m_render;
}

/*! \brief Override QVulkanWindow::event method
 *
 * We need to filter out spontaneous update requests, which for QVulkanWindow
 * trigger a call to startNextFrame, as Vulkan will spew errors if the frame is
 * not completed - but completing the frame will swap framebuffers (with nothing rendered).
*/
bool MythWindowVulkan::event(QEvent *Event)
{
    if (Event->type() == QEvent::UpdateRequest)
    {
        if (m_render && !m_render->GetFrameExpected())
        {
            // Log these for now - they may be important (e.g. QVulkanWindow::grab)
            LOG(VB_GENERAL, LOG_INFO, LOC + "Ignoring spontaneous update request");
            return QWindow::event(Event); // NOLINT(bugprone-parent-virtual-call)
        }
    }
    return QVulkanWindow::event(Event);
}
