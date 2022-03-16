// C++
#include <algorithm>

// Qt
#include <QGuiApplication>

// MythTV
#include "libmythbase/mythlogging.h"
#include "mythimage.h"
#include "vulkan/mythshadersvulkan.h"
#include "vulkan/mythshadervulkan.h"
#include "vulkan/mythtexturevulkan.h"
#include "vulkan/mythuniformbuffervulkan.h"
#include "vulkan/mythcombobuffervulkan.h"
#include "vulkan/mythdebugvulkan.h"
#include "vulkan/mythpaintervulkan.h"

#define LOC QString("VulkanPainter: ")

MythPainterVulkan::MythPainterVulkan(MythRenderVulkan* VulkanRender, MythMainWindow* Parent)
  : MythPainterGPU(Parent)
{
    m_transforms.push(QMatrix4x4());
    connect(VulkanRender, &MythRenderVulkan::DoFreeResources, this, &MythPainterVulkan::DoFreeResources);
}

MythPainterVulkan::~MythPainterVulkan()
{
    if (m_vulkan && m_vulkan->IsValidVulkan())
        m_vulkan->Funcs()->vkQueueWaitIdle(m_vulkan->Window()->graphicsQueue());

    Teardown();
    MythPainterVulkan::FreeResources();
    DoFreeResources();
}

void MythPainterVulkan::FreeResources()
{
    ClearCache();
    DeleteTextures();
    MythPainterGPU::FreeResources();
}

/// \brief Free resources before the render device is released.
void MythPainterVulkan::DoFreeResources()
{
    if (m_vulkan && m_vulkan->IsValidVulkan())
        m_vulkan->Funcs()->vkQueueWaitIdle(m_vulkan->Window()->graphicsQueue());

    LOG(VB_GENERAL, LOG_INFO, LOC + "Releasing Vulkan resources");

    delete m_projectionUniform;
    delete m_textureShader;

    if (m_vulkan && m_vulkan->IsValidVulkan())
    {
        m_vulkan->Funcs()->vkDestroyPipeline(m_vulkan->Device(), m_texturePipeline, nullptr);
        m_vulkan->Funcs()->vkDestroyDescriptorPool(m_vulkan->Device(), m_textureDescriptorPool, nullptr);
        m_vulkan->Funcs()->vkDestroyDescriptorPool(m_vulkan->Device(), m_projectionDescriptorPool, nullptr);
        m_vulkan->Funcs()->vkDestroySampler(m_vulkan->Device(), m_textureSampler, nullptr);
        if (m_textureUploadCmd)
        {
            m_vulkan->Funcs()->vkFreeCommandBuffers(m_vulkan->Device(),
                    m_vulkan->Window()->graphicsCommandPool(), 1, &m_textureUploadCmd);
        }
    }

    m_ready = false;
    delete m_vulkan;
    m_vulkan                   = nullptr;
    m_projectionDescriptorPool = MYTH_NULL_DISPATCH;
    m_projectionDescriptor     = MYTH_NULL_DISPATCH; // destroyed with pool
    m_projectionUniform        = nullptr;
    m_textureShader            = nullptr;
    m_textureUploadCmd         = nullptr;
    m_textureSampler           = MYTH_NULL_DISPATCH;
    m_texturePipeline          = MYTH_NULL_DISPATCH;
    m_textureDescriptorPool    = MYTH_NULL_DISPATCH;
    m_textureDescriptorsCreated = false;
    m_availableTextureDescriptors.clear(); // destroyed with pool
    m_frameStarted             = false;
    m_lastSize                 = { 0, 0 };

    LOG(VB_GENERAL, LOG_INFO, LOC + "Finished releasing resources");
}

QString MythPainterVulkan::GetName()
{
    return "Vulkan";
}

bool MythPainterVulkan::SupportsAlpha()
{
    return true;
}

bool MythPainterVulkan::SupportsAnimation()
{
    return true;
}

bool MythPainterVulkan::SupportsClipping()
{
    return false;
}

void MythPainterVulkan::PushTransformation(const UIEffects &Fx, QPointF Center)
{
    QMatrix4x4 newtop = m_transforms.top();
    if (Fx.m_hzoom != 1.0F || Fx.m_vzoom != 1.0F || Fx.m_angle != 0.0F)
    {
        newtop.translate(static_cast<float>(Center.x()), static_cast<float>(Center.y()));
        newtop.scale(Fx.m_hzoom, Fx.m_vzoom);
        newtop.rotate(Fx.m_angle, 0, 0, 1);
        newtop.translate(static_cast<float>(-Center.x()), static_cast<float>(-Center.y()));
    }
    m_transforms.push(newtop);
}

void MythPainterVulkan::PopTransformation()
{
    m_transforms.pop();
}

bool MythPainterVulkan::Ready()
{
    if (m_ready)
        return true;

    if (!m_vulkan)
    {
        // device setup can be delayed by a few frames on startup - check status
        // before continuing
        auto * window = MythRenderVulkan::GetVulkanRender()->GetVulkanWindow();
        if (!window || !window->device())
            return false;

        m_vulkan = MythVulkanObject::Create(MythRenderVulkan::GetVulkanRender());
        if (!m_vulkan)
            return false;
    }

    if (!m_textureShader)
    {
        std::vector<int> stages = { DefaultVertex450, DefaultFragment450 };
        m_textureShader = MythShaderVulkan::Create(m_vulkan, stages);
        if (!m_textureShader)
            return false;
    }

    if (!m_texturePipeline)
    {
        QRect viewport(QPoint{0, 0}, m_vulkan->Window()->swapChainImageSize());
        m_texturePipeline = m_vulkan->Render()->CreatePipeline(m_textureShader, viewport);
        if (!m_texturePipeline)
            return false;
    }

    if (!m_projectionDescriptorPool)
    {
        const auto & sizes = m_textureShader->GetPoolSizes(0);
        VkDescriptorPoolCreateInfo pool { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, nullptr,
                                          0, 1, static_cast<uint32_t>(sizes.size()), sizes.data() };
        if (m_vulkan->Funcs()->vkCreateDescriptorPool(m_vulkan->Device(), &pool, nullptr, &m_projectionDescriptorPool) != VK_SUCCESS)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create descriptor pool for projection");
            return false;
        }
    }

    if (!m_projectionUniform)
    {
        m_projectionUniform = MythUniformBufferVulkan::Create(m_vulkan, sizeof(float) * 16);
        if (!m_projectionUniform)
            return false;

        m_projection.setToIdentity();
        QRect viewport(QPoint{0, 0}, m_vulkan->Window()->swapChainImageSize());
        m_projection.ortho(viewport);
        m_projection = m_vulkan->Window()->clipCorrectionMatrix() * m_projection;
        m_projectionUniform->Update(m_projection.data());
    }

    if (!m_projectionDescriptor)
    {
        // projection is set 0
        VkDescriptorSetLayout layout = m_textureShader->GetDescSetLayout(0);
        VkDescriptorSetAllocateInfo alloc { };
        alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc.descriptorPool = m_projectionDescriptorPool;
        alloc.descriptorSetCount = 1;
        alloc.pSetLayouts = &layout;

        if (m_vulkan->Funcs()->vkAllocateDescriptorSets(m_vulkan->Device(), &alloc, &m_projectionDescriptor) != VK_SUCCESS)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + "Failed to allocate projection descriptor set");
            return false;
        }

        VkDescriptorBufferInfo buffdesc = m_projectionUniform->GetBufferInfo();

        VkWriteDescriptorSet write { };
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = m_projectionDescriptor;
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &buffdesc;

        m_vulkan->Funcs()->vkUpdateDescriptorSets(m_vulkan->Device(), 1, &write, 0, nullptr);
    }

    if (!m_textureDescriptorPool)
    {
        const auto & sizes = m_textureShader->GetPoolSizes(1);
        // match total number of individual descriptors with pool size
        std::vector<VkDescriptorPoolSize> adjsizes;
        std::transform(sizes.cbegin(), sizes.cend(), std::back_inserter(adjsizes),
                       [](VkDescriptorPoolSize Size){ return VkDescriptorPoolSize { Size.type, MAX_TEXTURE_COUNT }; });
        VkDescriptorPoolCreateInfo pool { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, nullptr,
                                          0, MAX_TEXTURE_COUNT, static_cast<uint32_t>(adjsizes.size()), adjsizes.data() };
        if (m_vulkan->Funcs()->vkCreateDescriptorPool(m_vulkan->Device(), &pool, nullptr, &m_textureDescriptorPool) != VK_SUCCESS)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create descriptor pool");
            return false;
        }
    }

    if (!m_textureDescriptorsCreated)
    {
        // transform and sampler are set 1 (projection is set 0)
        VkDescriptorSetLayout layout = m_textureShader->GetDescSetLayout(1);
        m_availableTextureDescriptors.clear();
        for (int i = 0; i < MAX_TEXTURE_COUNT; ++i)
        {
            VkDescriptorSet descset = MYTH_NULL_DISPATCH;
            VkDescriptorSetAllocateInfo alloc { };
            alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            alloc.descriptorPool = m_textureDescriptorPool;
            alloc.descriptorSetCount = 1;
            alloc.pSetLayouts = &layout;
            VkResult res = m_vulkan->Funcs()->vkAllocateDescriptorSets(m_vulkan->Device(), &alloc, &descset);
            if (res != VK_SUCCESS)
                LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to allocate descriptor set");
            m_availableTextureDescriptors.push_back(descset);
        }

        m_textureDescriptorsCreated = true;
    }

    if (!m_textureSampler)
    {
        m_textureSampler = m_vulkan->Render()->CreateSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR);
        if (!m_textureSampler)
            return false;
    }

    m_ready = true;
    return true;
}

/*! \brief Begin painting
 *
 * \note MythRenderVulkan must render between calls to startNextFrame and
 * the call to QVulkanWindow::frameReady - this guarantees that the underlying
 * QVulkanWindowRenderer is ready. startNextFrame is triggered by a call to
 * MythWindowVulkan::requestUpdate.
*/
void MythPainterVulkan::Begin(QPaintDevice* /*Parent*/)
{
    if (!Ready())
        return;

    // check if we need to adjust cache sizes
    if (m_lastSize != m_vulkan->Window()->size())
    {
        // This will scale the cache depending on the resolution in use
        static const int s_onehd = 1920 * 1080;
        static const int s_basesize = 64;
        m_lastSize = m_vulkan->Window()->size();
        float hdscreens = (static_cast<float>(m_lastSize.width() + 1) * m_lastSize.height()) / s_onehd;
        int cpu = std::max(static_cast<int>(hdscreens * s_basesize), s_basesize);
        int gpu = cpu * 3 / 2;
        SetMaximumCacheSizes(gpu, cpu);
    }

    // Sometimes the UI engine will mark images as 'changed' when moving between
    // screens. These are then often released here whilst still in use for the
    // previous frame. To avoid validation errors, wait for the last frame to
    // complete before continuing - though this feels like a hack and surely
    // the last frame should already be complete at this point?
    if (!m_texturesToDelete.empty())
    {
        m_vulkan->Funcs()->vkQueueWaitIdle(m_vulkan->Window()->graphicsQueue());
        DeleteTextures();
    }
    m_frameStarted = true;
}

void MythPainterVulkan::End()
{
    if (!(Ready() && m_frameStarted))
        return;

    // Complete any texture updates first
    if (m_textureUploadCmd)
    {
        m_vulkan->Render()->FinishSingleUseCommandBuffer(m_textureUploadCmd);
        m_textureUploadCmd = nullptr;

        // release staging buffers which are no longer needed
        for (auto * texture : m_stagedTextures)
            texture->StagingFinished();
        m_stagedTextures.clear();
    }

    if (m_queuedTextures.empty())
        return;

    if (m_viewControl.testFlag(Framebuffer))
    {
        // Tell the renderer that we are requesting a frame start
        m_vulkan->Render()->SetFrameExpected();

        // Signal DIRECTLY to the window to start the frame - which ensures
        // the event is not delayed and we can start to render immediately.
        QEvent update(QEvent::UpdateRequest);
        QGuiApplication::sendEvent(m_vulkan->Window(), &update);
    }

    // Retrieve the command buffer
    VkCommandBuffer currentcmdbuf = m_vulkan->Window()->currentCommandBuffer();
    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_vulkan->Render()->BeginDebugRegion(currentcmdbuf, "PAINTER_RENDER", MythDebugVulkan::kDebugGreen);

    // Bind our pipeline and retrieve layout once
    m_vulkan->Funcs()->vkCmdBindPipeline(currentcmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_texturePipeline);
    VkPipelineLayout layout = m_textureShader->GetPipelineLayout();

    // Bind descriptor set 0 - which is the projection, which is 'constant' for all textures
    m_vulkan->Funcs()->vkCmdBindDescriptorSets(currentcmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        layout, 0, 1, &m_projectionDescriptor, 0, nullptr);

    for (auto * texture : m_queuedTextures)
    {
        // Bind descriptor set 1 for this texture - sampler
        m_vulkan->Funcs()->vkCmdBindDescriptorSets(currentcmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                            layout, 1, 1, &texture->m_descriptor, 0, nullptr);

        // Push constants - transform, vertex data and color (alpha)
        m_vulkan->Funcs()->vkCmdPushConstants(currentcmdbuf, layout, VK_SHADER_STAGE_VERTEX_BIT,
                                       0, MYTH_PUSHBUFFER_SIZE, texture->Data());
        texture->PopData();

        // Draw
        m_vulkan->Funcs()->vkCmdDraw(currentcmdbuf, 4, 1, 0, 0);
    }

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_vulkan->Render()->EndDebugRegion(currentcmdbuf);

    m_queuedTextures.clear();

    if (m_viewControl.testFlag(Framebuffer))
        m_vulkan->Render()->EndFrame();
}

void MythPainterVulkan::DrawImage(const QRect Dest, MythImage *Image, const QRect Source, int Alpha)
{
    if (!m_frameStarted)
        return;

    MythTextureVulkan* texture = GetTextureFromCache(Image);
    if (texture)
    {
        // Update push constant buffer
        texture->PushData(m_transforms.top(), Source, Dest, Alpha);
        // Queue
        m_queuedTextures.emplace_back(texture);
    }
}

MythImage* MythPainterVulkan::GetFormatImagePriv()
{
    return new MythImage(this);
}

void MythPainterVulkan::DeleteFormatImagePriv(MythImage *Image)
{
    if (m_imageToTextureMap.contains(Image))
    {
        m_texturesToDelete.push_back(m_imageToTextureMap[Image]);
        m_imageToTextureMap.remove(Image);
        m_imageExpire.remove(Image);
    }
}

void MythPainterVulkan::ClearCache()
{
    LOG(VB_GENERAL, LOG_INFO, "Clearing Vulkan painter cache.");

    QMapIterator<MythImage *, MythTextureVulkan*> it(m_imageToTextureMap);
    while (it.hasNext())
    {
        it.next();
        m_texturesToDelete.push_back(m_imageToTextureMap[it.key()]);
        m_imageExpire.remove(it.key());
    }
    m_imageToTextureMap.clear();
}

MythTextureVulkan* MythPainterVulkan::GetTextureFromCache(MythImage *Image)
{
    if (!(Ready() && Image))
        return nullptr;

    if (m_imageToTextureMap.contains(Image))
    {
        if (!Image->IsChanged())
        {
            m_imageExpire.remove(Image);
            m_imageExpire.push_back(Image);
            return m_imageToTextureMap[Image];
        }
        DeleteFormatImagePriv(Image);
    }

    Image->SetChanged(false);

    int count = 0; // guard against unexpected texture creation failure
    MythTextureVulkan* texture = nullptr;
    while (texture == nullptr)
    {
        if (!m_textureUploadCmd)
            m_textureUploadCmd = m_vulkan->Render()->CreateSingleUseCommandBuffer();
        texture = MythTextureVulkan::Create(m_vulkan, Image, m_textureSampler, m_textureUploadCmd);
        if (texture != nullptr)
            break;

        // This can happen if the cached textures are too big for GPU memory
        if ((count++ > 1000) || m_hardwareCacheSize < (8 * 1024 * 1024))
        {
            LOG(VB_GENERAL, LOG_ERR, "Failed to create Vulkan texture.");
            return nullptr;
        }

        // Shrink the cache size
        m_maxHardwareCacheSize = (3 * m_hardwareCacheSize) / 4;
        LOG(VB_GENERAL, LOG_NOTICE, LOC + QString("Shrinking HW cache size to %1KB")
            .arg(m_maxHardwareCacheSize / 1024));

        while (m_hardwareCacheSize > m_maxHardwareCacheSize)
        {
            MythImage *expired = m_imageExpire.front();
            m_imageExpire.pop_front();
            DeleteFormatImagePriv(expired);
            DeleteTextures();
        }
    }

    if (!m_textureDescriptorPool)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "No descriptor pool?");
        delete texture;
        return nullptr;
    }

    // With a reasonable hardware cache size and many small thumbnails and
    // text images, we can easily hit the max texture count before the cache
    // size. So ensure we always have a descriptor available.

    if (m_availableTextureDescriptors.empty())
    {
        MythImage *expired = m_imageExpire.front();
        m_imageExpire.pop_front();
        DeleteFormatImagePriv(expired);
        DeleteTextures();

        if (m_availableTextureDescriptors.empty())
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC + "No texture descriptor available??");
            delete texture;
            return nullptr;
        }
    }

    VkDescriptorSet descset = m_availableTextureDescriptors.back();
    m_availableTextureDescriptors.pop_back();

    auto imagedesc = texture->GetDescriptorImage();
    VkWriteDescriptorSet write { };
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = descset;
    write.dstBinding = 0;
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &imagedesc;
    m_vulkan->Funcs()->vkUpdateDescriptorSets(m_vulkan->Device(), 1, &write, 0, nullptr);
    texture->AddDescriptor(descset);
    m_stagedTextures.emplace_back(texture);

    CheckFormatImage(Image);
    m_hardwareCacheSize += texture->m_dataSize;
    m_imageToTextureMap[Image] = texture;
    m_imageExpire.push_back(Image);

    while ((m_hardwareCacheSize > m_maxHardwareCacheSize) && !m_imageExpire.empty())
    {
        MythImage *expired = m_imageExpire.front();
        m_imageExpire.pop_front();
        DeleteFormatImagePriv(expired);
        DeleteTextures();
    }

    //LOG(VB_GENERAL, LOG_INFO, LOC + QString("Images: %1 Sets: %2 Recycled: %3")
    //    .arg(m_imageToTextureMap.size()).arg(m_allocatedTextureDescriptors).arg(m_availableTextureDescriptors.size()));
    //LOG(VB_GENERAL, LOG_INFO, LOC + QString("Hardware cache: %1 (Max: %2)")
    //    .arg(m_hardwareCacheSize / (1024 * 1024)).arg(m_maxHardwareCacheSize / (1024 * 1024)));

    return texture;
}

void MythPainterVulkan::DeleteTextures()
{
    if (!Ready() || m_texturesToDelete.empty())
        return;

    while (!m_texturesToDelete.empty())
    {
        MythTextureVulkan *texture = m_texturesToDelete.front();
        m_hardwareCacheSize -= texture->m_dataSize;
        VkDescriptorSet descriptor = texture->TakeDescriptor();
        if (descriptor)
            m_availableTextureDescriptors.emplace_back(descriptor);
        delete texture;
        m_texturesToDelete.pop_front();
    }
}
