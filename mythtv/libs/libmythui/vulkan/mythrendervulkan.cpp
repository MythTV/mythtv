// Qt
#include <QGuiApplication>

// MythTV
#include "libmythbase/mythlogging.h"
#include "mythimage.h"
#include "mythmainwindow.h"
#include "vulkan/mythdebugvulkan.h"
#include "vulkan/mythwindowvulkan.h"
#include "vulkan/mythshadervulkan.h"
#include "vulkan/mythtexturevulkan.h"
#include "vulkan/mythrendervulkan.h"

#define LOC QString("VulkanRender: ")

MythVulkanObject* MythVulkanObject::Create(MythRenderVulkan* Render)
{
    MythVulkanObject* result = nullptr;
    result = new MythVulkanObject(Render);
    if (result && !result->IsValidVulkan())
    {
        delete result;
        result = nullptr;
    }
    return result;
}

MythVulkanObject::MythVulkanObject(MythRenderVulkan* Render)
  : m_vulkanRender(Render)
{
    if (m_vulkanRender)
    {
        m_vulkanWindow = m_vulkanRender->GetVulkanWindow();
        if (m_vulkanWindow)
        {
            m_vulkanDevice = m_vulkanWindow->device();
            if (m_vulkanDevice)
                m_vulkanFuncs = m_vulkanWindow->vulkanInstance()->deviceFunctions(m_vulkanDevice);
        }
    }

    CheckValid();
}

MythVulkanObject::MythVulkanObject(MythVulkanObject* Other)
  : m_vulkanRender(Other ? Other->Render() : nullptr),
    m_vulkanDevice(Other ? Other->Device() : nullptr),
    m_vulkanFuncs(Other  ? Other->Funcs()  : nullptr),
    m_vulkanWindow(Other ? Other->Window() : nullptr)
{
    CheckValid();
}

void MythVulkanObject::CheckValid()
{
    if (!(m_vulkanRender && m_vulkanDevice && m_vulkanFuncs && m_vulkanWindow))
    {
        m_vulkanValid = false;
        LOG(VB_GENERAL, LOG_ERR, "VulkanBase: Invalid Myth vulkan object");
    }
}

bool MythVulkanObject::IsValidVulkan() const
{
    return m_vulkanValid;
}

MythRenderVulkan* MythVulkanObject::Render()
{
    return m_vulkanRender;
}

VkDevice MythVulkanObject::Device()
{
    return m_vulkanDevice;
}

QVulkanDeviceFunctions* MythVulkanObject::Funcs()
{
    return m_vulkanFuncs;
}

MythWindowVulkan* MythVulkanObject::Window()
{
    return m_vulkanWindow;
}

MythRenderVulkan* MythRenderVulkan::GetVulkanRender(void)
{
    MythRenderVulkan* result = nullptr;

    // Don't try and create the window
    if (!HasMythMainWindow())
        return result;

    MythMainWindow* window = MythMainWindow::getMainWindow();
    if (window)
        result = dynamic_cast<MythRenderVulkan*>(window->GetRenderDevice());
    return result;
}

MythRenderVulkan::MythRenderVulkan()
  : MythRender(kRenderVulkan)
{
#ifdef USING_GLSLANG
    // take a top level 'reference' to libglslang to ensure it is persistent
    if (!MythShaderVulkan::InitGLSLang())
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to initialise GLSLang");
#endif
    LOG(VB_GENERAL, LOG_INFO, LOC + "Created");
}

MythRenderVulkan::~MythRenderVulkan()
{
#ifdef USING_GLSLANG
    MythShaderVulkan::InitGLSLang(true);
#endif
    LOG(VB_GENERAL, LOG_INFO, LOC + "Destroyed");
}

void MythRenderVulkan::SetVulkanWindow(MythWindowVulkan *VulkanWindow)
{
    m_window = VulkanWindow;
}

MythWindowVulkan* MythRenderVulkan::GetVulkanWindow(void)
{
    return m_window;
}

void MythRenderVulkan::preInitResources(void)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + __FUNCTION__);
}

void MythRenderVulkan::initResources(void)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + __FUNCTION__);
    m_device = m_window->device();
    m_funcs = m_window->vulkanInstance()->functions();
    m_devFuncs = m_window->vulkanInstance()->deviceFunctions(m_device);

    static bool s_debugged = false;
    if (!s_debugged)
    {
        s_debugged = true;
        DebugVulkan();
    }

    // retrieve physical device features and limits
    m_window->vulkanInstance()->functions()
            ->vkGetPhysicalDeviceFeatures(m_window->physicalDevice(), &m_phyDevFeatures);
    m_phyDevLimits = m_window->physicalDeviceProperties()->limits;

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
    {
        MythVulkanObject temp(this);
        m_debugMarker = MythDebugVulkan::Create(&temp);
    }
}

QStringList MythRenderVulkan::GetDescription(void)
{
    QStringList result;
    result.append(tr("QPA platform") + "\t: " + QGuiApplication::platformName());
    result.append(tr("Vulkan details:"));
    result.append(tr("Driver name")  + "\t: " + m_debugInfo["drivername"]);
    result.append(tr("Driver info")  + "\t: " + m_debugInfo["driverinfo"]);
    result.append(tr("Device name")  + "\t: " + m_debugInfo["devicename"]);
    result.append(tr("Device type")  + "\t: " + m_debugInfo["devicetype"]);
    result.append(tr("API version")  + "\t: " + m_debugInfo["apiversion"]);
    return result;
}

void MythRenderVulkan::DebugVulkan(void)
{
    auto deviceType = [](VkPhysicalDeviceType Type)
    {
        switch (Type)
        {
            case VK_PHYSICAL_DEVICE_TYPE_CPU:            return "Software";
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "Integrated GPU";
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   return "Discrete GPU";
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    return "Virtual GPU";
            default: break;
        }
        return "Unknown";
    };

    auto version = [](uint32_t Version)
    {
        return QString("%1.%2.%3").arg(VK_VERSION_MAJOR(Version))
                                  .arg(VK_VERSION_MINOR(Version))
                                  .arg(VK_VERSION_PATCH(Version));
    };

    const auto * props = m_window->physicalDeviceProperties();
    if (!props)
        return;
    const auto & limits = props->limits;
    auto devextensions = m_window->supportedDeviceExtensions();
    auto instextensions = m_window->vulkanInstance()->supportedExtensions();

    if (instextensions.contains(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
    {
        auto raw  = m_window->vulkanInstance()->getInstanceProcAddr("vkGetPhysicalDeviceProperties2");
        auto proc = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(raw);
        if (proc)
        {
            VkPhysicalDeviceDriverPropertiesKHR driverprops { };
            driverprops.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES_KHR;
            driverprops.pNext = nullptr;

            VkPhysicalDeviceProperties2 devprops { };
            devprops.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            devprops.pNext = &driverprops;

            proc(m_window->physicalDevice(), &devprops);
            m_debugInfo.insert("drivername", driverprops.driverName);
            m_debugInfo.insert("driverinfo", driverprops.driverInfo);
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("Driver name     : %1").arg(driverprops.driverName));
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("Driver info     : %1").arg(driverprops.driverInfo));
        }
    }

    m_debugInfo.insert("devicename", props->deviceName);
    m_debugInfo.insert("devicetype", deviceType(props->deviceType));
    m_debugInfo.insert("apiversion", version(props->apiVersion));

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Qt platform     : %1").arg(QGuiApplication::platformName()));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Device name     : %1").arg(props->deviceName));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Device type     : %1").arg(deviceType(props->deviceType)));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("API version     : %1").arg(version(props->apiVersion)));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Device ID       : 0x%1").arg(props->deviceID, 0, 16));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Driver version  : %1").arg(version(props->driverVersion)));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Max image size  : %1x%1").arg(limits.maxImageDimension2D));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Max framebuffer : %1x%2")
        .arg(limits.maxFramebufferWidth).arg(limits.maxFramebufferHeight));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Max allocations : %1").arg(limits.maxMemoryAllocationCount));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Max samplers    : %1").arg(limits.maxSamplerAllocationCount));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Min alignment   : %1").arg(limits.minMemoryMapAlignment));

    if (VERBOSE_LEVEL_CHECK(VB_GENERAL, LOG_DEBUG))
    {
        LOG(VB_GENERAL, LOG_INFO, QString("%1 device extensions supported:").arg(devextensions.size()));
        for (const auto& extension : std::as_const(devextensions))
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("%1 Version: %2").arg(extension.name.constData()).arg(extension.version));

        LOG(VB_GENERAL, LOG_INFO, QString("%1 instance extensions supported:").arg(instextensions.size()));
        for (const auto& extension : std::as_const(instextensions))
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("%1 Version: %2").arg(extension.name.constData()).arg(extension.version));

        auto layers = m_window->vulkanInstance()->supportedLayers();
        LOG(VB_GENERAL, LOG_INFO, QString("%1 layer types supported:").arg(layers.size()));
        for (const auto& layer : std::as_const(layers))
            LOG(VB_GENERAL, LOG_INFO, QString("%1 Version: %2").arg(layer.name.constData()).arg(layer.version));
    }
}

void MythRenderVulkan::initSwapChainResources(void)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + __FUNCTION__);
}

void MythRenderVulkan::releaseSwapChainResources(void)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + __FUNCTION__);
}

void MythRenderVulkan::releaseResources(void)
{
    delete m_debugMarker;

    LOG(VB_GENERAL, LOG_INFO, LOC + __FUNCTION__);
    emit DoFreeResources();
    m_devFuncs      = nullptr;
    m_funcs         = nullptr;
    m_device        = nullptr;
    m_debugMarker   = nullptr;
    m_frameStarted  = false;
    m_frameExpected = false;
    m_phyDevLimits  = { };
    m_phyDevFeatures = { };
}

void MythRenderVulkan::physicalDeviceLost(void)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + __FUNCTION__);
    emit PhysicalDeviceLost();
}

void MythRenderVulkan::logicalDeviceLost(void)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + __FUNCTION__);
    emit LogicalDeviceLost();
}

void MythRenderVulkan::SetFrameExpected(void)
{
    if (m_frameExpected)
        LOG(VB_GENERAL, LOG_ERR, LOC + "Starting frame rendering before last is finished");
    m_frameExpected = true;
}

bool MythRenderVulkan::GetFrameExpected(void) const
{
    return m_frameExpected;
}

bool MythRenderVulkan::GetFrameStarted(void) const
{
    return m_frameStarted;
}

void MythRenderVulkan::startNextFrame(void)
{
    // essentially ignore spontaneous requests
    if (!m_frameExpected)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "Spontaneous frame");
        m_window->frameReady();
        return;
    }

    m_frameExpected = false;
    BeginFrame();
}

void MythRenderVulkan::BeginFrame(void)
{
    m_frameStarted = true;

    // clear the framebuffer
    VkClearColorValue clearColor = {{ 0.0F, 0.0F, 0.0F, 1.0F }};
    VkClearDepthStencilValue clearDS = { 1.0F, 0 };
    std::array<VkClearValue,2> clearValues {};
    clearValues[0].color        = clearColor;
    clearValues[1].depthStencil = clearDS;

    // begin pass
    VkCommandBuffer commandbuffer = m_window->currentCommandBuffer();
    QSize size = m_window->swapChainImageSize();
    VkRenderPassBeginInfo rpBeginInfo;
    memset(&rpBeginInfo, 0, sizeof(rpBeginInfo));
    rpBeginInfo.sType            = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBeginInfo.renderPass       = m_window->defaultRenderPass();
    rpBeginInfo.framebuffer      = m_window->currentFramebuffer();
    rpBeginInfo.clearValueCount  = 2;
    rpBeginInfo.pClearValues     = clearValues.data();
    rpBeginInfo.renderArea.extent.width = static_cast<uint32_t>(size.width());
    rpBeginInfo.renderArea.extent.height = static_cast<uint32_t>(size.height());
    m_devFuncs->vkCmdBeginRenderPass(commandbuffer, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void MythRenderVulkan::EndFrame(void)
{
    if (m_frameStarted)
    {
        m_devFuncs->vkCmdEndRenderPass(m_window->currentCommandBuffer());
        m_window->frameReady();
    }
    m_frameStarted = false;
}

void MythRenderVulkan::TransitionImageLayout(VkImage &Image,
                                             VkImageLayout OldLayout,
                                             VkImageLayout NewLayout,
                                             VkCommandBuffer CommandBuffer)
{

    VkCommandBuffer commandbuffer = CommandBuffer ? CommandBuffer : CreateSingleUseCommandBuffer();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = OldLayout;
    barrier.newLayout = NewLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = Image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage = 0;
    VkPipelineStageFlags destinationStage = 0;
    if (OldLayout == VK_IMAGE_LAYOUT_UNDEFINED && NewLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (OldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && NewLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Unsupported layout transition");
    }

    m_devFuncs->vkCmdPipelineBarrier(commandbuffer, sourceStage, destinationStage,
                                     0, 0, nullptr, 0, nullptr, 1, &barrier);
    if (!CommandBuffer)
        FinishSingleUseCommandBuffer(commandbuffer);
}

void MythRenderVulkan::CopyBufferToImage(VkBuffer Buffer, VkImage Image,
                                         uint32_t Width, uint32_t Height,
                                         VkCommandBuffer CommandBuffer)
{
    VkCommandBuffer commandbuffer = CommandBuffer ? CommandBuffer : CreateSingleUseCommandBuffer();
    VkBufferImageCopy region { };
    region.bufferOffset      = 0;
    region.bufferRowLength   = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { Width, Height, 1 };
    m_devFuncs->vkCmdCopyBufferToImage(commandbuffer, Buffer, Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    if (!CommandBuffer)
        FinishSingleUseCommandBuffer(commandbuffer);
}

void MythRenderVulkan::CopyBuffer(VkBuffer Src, VkBuffer Dst, VkDeviceSize Size, VkCommandBuffer CommandBuffer)
{
    VkBufferCopy region { };
    region.size = Size;

    if (CommandBuffer)
    {
        m_devFuncs->vkCmdCopyBuffer(CommandBuffer, Src, Dst, 1, &region);
    }
    else
    {
        VkCommandBuffer cmdbuf = CreateSingleUseCommandBuffer();
        m_devFuncs->vkCmdCopyBuffer(cmdbuf, Src, Dst, 1, &region);
        FinishSingleUseCommandBuffer(cmdbuf);
    }
}

bool MythRenderVulkan::CreateBuffer(VkDeviceSize          Size,
                                    VkBufferUsageFlags    Usage,
                                    VkMemoryPropertyFlags Properties,
                                    VkBuffer             &Buffer,
                                    VkDeviceMemory       &Memory)
{
    VkBufferCreateInfo bufferInfo { };
    bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size        = Size;
    bufferInfo.usage       = Usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (m_devFuncs->vkCreateBuffer(m_device, &bufferInfo, nullptr, &Buffer) != VK_SUCCESS)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create buffer");
        return false;
    }

    VkMemoryRequirements requirements;
    m_devFuncs->vkGetBufferMemoryRequirements(m_device, Buffer, &requirements);

    VkMemoryAllocateInfo allocInfo { };
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = requirements.size;
    if (auto type = FindMemoryType(requirements.memoryTypeBits, Properties))
    {
        allocInfo.memoryTypeIndex = type.value();
        if (m_devFuncs->vkAllocateMemory(m_device, &allocInfo, nullptr, &Memory) == VK_SUCCESS)
        {
            m_devFuncs->vkBindBufferMemory(m_device, Buffer, Memory, 0);
            return true;
        }
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to allocate buffer memory");
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to deduce buffer memory type");
    }

    m_devFuncs->vkDestroyBuffer(m_device, Buffer, nullptr);
    return false;
}

VkSampler MythRenderVulkan::CreateSampler(VkFilter Min, VkFilter Mag)
{
    VkSampler result = MYTH_NULL_DISPATCH;
    VkSamplerCreateInfo samplerinfo { };
    memset(&samplerinfo, 0, sizeof(samplerinfo));
    samplerinfo.sType            = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerinfo.minFilter        = Min;
    samplerinfo.magFilter        = Mag;
    samplerinfo.addressModeU     = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerinfo.addressModeV     = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerinfo.addressModeW     = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerinfo.anisotropyEnable = VK_FALSE;
    samplerinfo.maxAnisotropy    = 1;
    samplerinfo.borderColor      = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
    samplerinfo.unnormalizedCoordinates = VK_FALSE;
    samplerinfo.compareEnable    = VK_FALSE;
    samplerinfo.compareOp        = VK_COMPARE_OP_ALWAYS;
    samplerinfo.mipmapMode       = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    if (m_devFuncs->vkCreateSampler(m_device, &samplerinfo, nullptr, &result) != VK_SUCCESS)
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create image sampler");
    return result;
}

VkPhysicalDeviceFeatures MythRenderVulkan::GetPhysicalDeviceFeatures() const
{
    return m_phyDevFeatures;
}

VkPhysicalDeviceLimits MythRenderVulkan::GetPhysicalDeviceLimits() const
{
    return m_phyDevLimits;
}

void MythRenderVulkan::BeginDebugRegion(VkCommandBuffer CommandBuffer, const char *Name,
                                        const MythVulkan4F Color)
{
    if (m_debugMarker && CommandBuffer)
        m_debugMarker->BeginRegion(CommandBuffer, Name, Color);
}

void MythRenderVulkan::EndDebugRegion(VkCommandBuffer CommandBuffer)
{
    if (m_debugMarker && CommandBuffer)
        m_debugMarker->EndRegion(CommandBuffer);
}

bool MythRenderVulkan::CreateImage(QSize             Size,
                                   VkFormat          Format,
                                   VkImageTiling     Tiling,
                                   VkImageUsageFlags Usage,
                                   VkMemoryPropertyFlags Properties,
                                   VkImage          &Image,
                                   VkDeviceMemory   &ImageMemory)
{
    VkImageCreateInfo imageinfo { };
    imageinfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageinfo.imageType     = VK_IMAGE_TYPE_2D;
    imageinfo.extent.width  = static_cast<uint32_t>(Size.width());
    imageinfo.extent.height = static_cast<uint32_t>(Size.height());
    imageinfo.extent.depth  = 1;
    imageinfo.mipLevels     = 1;
    imageinfo.arrayLayers   = 1;
    imageinfo.format        = Format;
    imageinfo.tiling        = Tiling;
    imageinfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageinfo.usage         = Usage;
    imageinfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageinfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

    if (m_devFuncs->vkCreateImage(m_device, &imageinfo, nullptr, &Image) != VK_SUCCESS)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create Vulkan image");
        return false;
    }

    VkMemoryRequirements requirements;
    m_devFuncs->vkGetImageMemoryRequirements(m_device, Image, &requirements);

    VkMemoryAllocateInfo allocinfo { };
    allocinfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocinfo.allocationSize = requirements.size;
    if (auto type = FindMemoryType(requirements.memoryTypeBits, Properties))
    {
        allocinfo.memoryTypeIndex = type.value();
        if (m_devFuncs->vkAllocateMemory(m_device, &allocinfo, nullptr, &ImageMemory) == VK_SUCCESS)
        {
            m_devFuncs->vkBindImageMemory(m_device, Image, ImageMemory, 0U);
            return true;
        }
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to allocate image memory");
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to deduce image memory type");
    }

    m_devFuncs->vkDestroyImage(m_device, Image, nullptr);
    return false;
}

std::optional<uint32_t> MythRenderVulkan::FindMemoryType(uint32_t Filter, VkMemoryPropertyFlags Properties)
{
    std::optional<uint32_t> result;
    VkPhysicalDeviceMemoryProperties memoryprops;
    m_funcs->vkGetPhysicalDeviceMemoryProperties(m_window->physicalDevice(), &memoryprops);

    for (uint32_t i = 0; i < memoryprops.memoryTypeCount; i++)
    {
        if ((Filter & (1 << i)) && (memoryprops.memoryTypes[i].propertyFlags & Properties) == Properties)
        {
            result = i;
            break;
        }
    }

   return result;
}

VkCommandBuffer MythRenderVulkan::CreateSingleUseCommandBuffer(void)
{
    VkCommandBufferAllocateInfo allocinfo { };
    allocinfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocinfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocinfo.commandPool = m_window->graphicsCommandPool();
    allocinfo.commandBufferCount = 1;
    VkCommandBuffer commandbuffer = nullptr;
    m_devFuncs->vkAllocateCommandBuffers(m_device, &allocinfo, &commandbuffer);
    VkCommandBufferBeginInfo begininfo { };
    begininfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begininfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    m_devFuncs->vkBeginCommandBuffer(commandbuffer, &begininfo);
    return commandbuffer;
}

void MythRenderVulkan::FinishSingleUseCommandBuffer(VkCommandBuffer &Buffer)
{
    m_devFuncs->vkEndCommandBuffer(Buffer);
    VkSubmitInfo submitinfo{};
    submitinfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitinfo.commandBufferCount = 1;
    submitinfo.pCommandBuffers = &Buffer;
    m_devFuncs->vkQueueSubmit(m_window->graphicsQueue(), 1, &submitinfo, MYTH_NULL_DISPATCH);
    m_devFuncs->vkQueueWaitIdle(m_window->graphicsQueue());
    m_devFuncs->vkFreeCommandBuffers(m_device, m_window->graphicsCommandPool(), 1, &Buffer);
}

VkPipeline MythRenderVulkan::CreatePipeline(MythShaderVulkan* Shader,
                                            const QRect Viewport,
                                            std::vector<VkDynamicState> Dynamic)
{
    if (!(Shader && Viewport.isValid()))
        return MYTH_NULL_DISPATCH;

    // shaders
    const auto & shaderstages = Shader->Stages();

    // primitives
    VkPipelineInputAssemblyStateCreateInfo inputassembly { };
    inputassembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputassembly.topology               = Shader->GetTopology();
    inputassembly.primitiveRestartEnable = VK_FALSE;

    // viewport - N.B. static
    VkViewport viewport { };
    viewport.x        = static_cast<float>(Viewport.left());
    viewport.y        = static_cast<float>(Viewport.left());
    viewport.width    = static_cast<float>(Viewport.width());
    viewport.height   = static_cast<float>(Viewport.height());
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;

    VkRect2D scissor { };
    scissor.offset = { Viewport.left(), Viewport.top() };
    scissor.extent = { static_cast<uint32_t>(Viewport.width()), static_cast<uint32_t>(Viewport.height()) };

    VkPipelineViewportStateCreateInfo viewportstate { };
    viewportstate.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportstate.viewportCount = 1;
    viewportstate.pViewports    = &viewport;
    viewportstate.scissorCount  = 1;
    viewportstate.pScissors     = &scissor;

    // Vertex input - from the shader
    VkPipelineVertexInputStateCreateInfo vertexinput { };
    const auto & vertexattribs = Shader->GetVertexAttributes();
    if (vertexattribs.empty())
    {
        vertexinput.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexinput.vertexBindingDescriptionCount   = 0;
        vertexinput.pVertexBindingDescriptions      = nullptr;
        vertexinput.vertexAttributeDescriptionCount = 0;
        vertexinput.pVertexAttributeDescriptions    = nullptr;
    }
    else
    {
        vertexinput.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexinput.vertexBindingDescriptionCount   = 1;
        vertexinput.pVertexBindingDescriptions      = &Shader->GetVertexBindingDesc();
        vertexinput.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexattribs.size());
        vertexinput.pVertexAttributeDescriptions    = vertexattribs.data();
    }

    // multisampling - no thanks
    VkPipelineMultisampleStateCreateInfo multisampling { };
    multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable  = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // blending - regular alpha blend
    VkPipelineColorBlendAttachmentState colorblendattachment { };
    colorblendattachment.blendEnable         = VK_TRUE;
    colorblendattachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorblendattachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorblendattachment.colorBlendOp        = VK_BLEND_OP_ADD;
    colorblendattachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorblendattachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorblendattachment.alphaBlendOp        = VK_BLEND_OP_ADD;
    colorblendattachment.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                               VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable     = VK_FALSE;
    colorBlending.logicOp           = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount   = 1;
    colorBlending.pAttachments      = &colorblendattachment;
    colorBlending.blendConstants[0] = 0.0F;
    colorBlending.blendConstants[1] = 0.0F;
    colorBlending.blendConstants[2] = 0.0F;
    colorBlending.blendConstants[3] = 0.0F;

    // rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer { };
    rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable        = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode             = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth               = 1.0F;
    rasterizer.cullMode                = VK_CULL_MODE_NONE;
    rasterizer.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable         = VK_FALSE;

    // depth/stencil test (required as Qt creates a depth/stencil attachment)
    VkPipelineDepthStencilStateCreateInfo depthstencil { };
    depthstencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthstencil.pNext                 = nullptr;
    depthstencil.flags                 = 0;
    depthstencil.depthTestEnable       = VK_FALSE;
    depthstencil.depthWriteEnable      = VK_FALSE;
    depthstencil.depthCompareOp        = VK_COMPARE_OP_NEVER;
    depthstencil.depthBoundsTestEnable = VK_FALSE;
    depthstencil.stencilTestEnable     = VK_FALSE;
    depthstencil.front                 = { };
    depthstencil.back                  = { };
    depthstencil.minDepthBounds        = 0.0F;
    depthstencil.maxDepthBounds        = 1.0F;

    // setup dynamic state
    VkPipelineDynamicStateCreateInfo dynamic { };
    dynamic.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = Dynamic.empty() ? 0 : static_cast<uint32_t>(Dynamic.size());
    dynamic.pDynamicStates    = Dynamic.empty() ? nullptr : Dynamic.data();

    // and breathe
    VkGraphicsPipelineCreateInfo pipelinecreate { };
    pipelinecreate.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelinecreate.pNext               = nullptr;
    pipelinecreate.flags               = 0;
    pipelinecreate.stageCount          = static_cast<uint32_t>(shaderstages.size());
    pipelinecreate.pStages             = shaderstages.data();
    pipelinecreate.pVertexInputState   = &vertexinput;
    pipelinecreate.pInputAssemblyState = &inputassembly;
    pipelinecreate.pTessellationState  = nullptr;
    pipelinecreate.pViewportState      = &viewportstate;
    pipelinecreate.pRasterizationState = &rasterizer;
    pipelinecreate.pMultisampleState   = &multisampling;
    pipelinecreate.pDepthStencilState  = &depthstencil;
    pipelinecreate.pColorBlendState    = &colorBlending;
    pipelinecreate.pDynamicState       = &dynamic;
    pipelinecreate.layout              = Shader->GetPipelineLayout();
    pipelinecreate.renderPass          = m_window->defaultRenderPass();
    pipelinecreate.subpass             = 0;
    pipelinecreate.basePipelineHandle  = MYTH_NULL_DISPATCH;
    pipelinecreate.basePipelineIndex   = 0;

    VkPipeline result = MYTH_NULL_DISPATCH;
    if (m_devFuncs->vkCreateGraphicsPipelines(m_device, MYTH_NULL_DISPATCH, 1, &pipelinecreate, nullptr, &result) == VK_SUCCESS)
        return result;

    LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create graphics pipeline");
    return MYTH_NULL_DISPATCH;
}
