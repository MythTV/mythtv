// Qt
#include <QLoggingCategory>
#include <QResizeEvent>

// MythTV
#include "libmythbase/mythlogging.h"
#include "mythmainwindow.h"
#include "vulkan/mythrendervulkan.h"
#include "vulkan/mythwindowvulkan.h"
#include "vulkan/mythpainterwindowvulkan.h"

#define LOC QString("VulkanPaintWin: ")

MythPainterWindowVulkan::MythPainterWindowVulkan(MythMainWindow *MainWindow)
  : MythPainterWindow(MainWindow),
    m_parent(MainWindow),
    m_vulkan(new QVulkanInstance())
{
    // Create the Vulkan instance. This must outlive the Vulkan window
    m_vulkan->setApiVersion(QVersionNumber(1, 1));
    m_vulkan->setExtensions(QByteArrayList() << VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME <<
                                                VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
    // As for OpenGL - VB_GPU enables debug output - but this also requires the
    // validation layers to be installed
    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
    {
#ifndef Q_OS_ANDROID
        m_vulkan->setLayers(QByteArrayList() << "VK_LAYER_LUNARG_standard_validation");
#else
        m_vulkan->setLayers(QByteArrayList() << "VK_LAYER_GOOGLE_threading" << "VK_LAYER_LUNARG_parameter_validation"
                                             << "VK_LAYER_LUNARG_object_tracker" << "VK_LAYER_LUNARG_core_validation"
                                             << "VK_LAYER_LUNARG_image" << "VK_LAYER_LUNARG_swapchain"
                                             << "VK_LAYER_GOOGLE_unique_objects");
#endif
        // Add some Qt internal Vulkan logging
        QLoggingCategory::setFilterRules(QStringLiteral("qt.vulkan=true"));
    }

    if (!m_vulkan->create())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to create Vulkan instance (%1)")
            .arg(m_vulkan->errorCode()));
        return;
    }

    // Create the Vulkan renderer
    auto* render = new MythRenderVulkan();
    m_render = render;

    // Create the the Vulkan window
    m_window = new MythWindowVulkan(render);
    m_window->setVulkanInstance(m_vulkan);
    render->SetVulkanWindow(m_window);
    m_window->resize(this->size());

    // Wrap the window in a widget
    m_wrapper = QWidget::createWindowContainer(m_window, this);
    m_wrapper->show();

    m_valid = true;
}

MythPainterWindowVulkan::~MythPainterWindowVulkan()
{
    // N.B. m_render is owned by QVulkanWindow - do not delete/decref
    delete m_window;
    delete m_wrapper;
    if (m_vulkan)
        m_vulkan->destroy();
    delete m_vulkan;
}

bool MythPainterWindowVulkan::IsValid(void) const
{
    return m_valid;
}

MythWindowVulkan* MythPainterWindowVulkan::GetVulkanWindow(void)
{
    return m_window;
}

void MythPainterWindowVulkan::paintEvent(QPaintEvent* /*PaintEvent*/)
{
    m_parent->drawScreen();
}

void MythPainterWindowVulkan::resizeEvent(QResizeEvent* ResizeEvent)
{
    if (m_wrapper)
        m_wrapper->resize(ResizeEvent->size());
    MythPainterWindow::resizeEvent(ResizeEvent);
}
