// MythTV
#include "libmythbase/mythlogging.h"
#include "libmythui/vulkan/mythuniformbuffervulkan.h"
#include "libmythui/vulkan/mythwindowvulkan.h"
#include "visualisations/vulkan/mythvisualvulkan.h"

#include <utility>

#define LOC QString("VulkanVis: ")

MythVisualVulkan::MythVisualVulkan(MythRenderVulkan *Render,
                                   std::vector<VkDynamicState> Dynamic,
                                   std::vector<int> Stages,
                                   const MythShaderMap *Sources,
                                   const MythBindingMap *Bindings)
  : MythVulkanObject(Render),
    m_dynamicState(std::move(Dynamic)),
    m_shaderStages(std::move(Stages)),
    m_shaderSources(Sources),
    m_shaderBindings(Bindings)
{
}

MythVisualVulkan::~MythVisualVulkan()
{
    MythVisualVulkan::TearDownVulkan();
}

MythRenderVulkan* MythVisualVulkan::InitialiseVulkan(const QRect /*Area*/)
{
    if (!IsValidVulkan())
        return nullptr;

    if (m_vulkanShader && m_pipeline && m_projectionUniform && m_descriptorPool && m_projectionDescriptor)
        return m_vulkanRender;

    MythVisualVulkan::TearDownVulkan();

    // Create shader
    if (!m_shaderStages.empty() && m_shaderSources && m_shaderBindings)
    {
        m_vulkanShader = MythShaderVulkan::Create(this, m_shaderStages, m_shaderSources, m_shaderBindings);
        if (!m_vulkanShader)
            return nullptr;
    }
    else
    {
        return nullptr;
    }

    // Create pipeline
    QRect viewport(QPoint{0, 0}, m_vulkanWindow->swapChainImageSize());
    m_pipeline = m_vulkanRender->CreatePipeline(m_vulkanShader, viewport, m_dynamicState);
    if (!m_pipeline)
        return nullptr;

    // Create projection uniform
    m_projectionUniform = MythUniformBufferVulkan::Create(this, sizeof(float) * 16);
    if (!m_projectionUniform)
        return nullptr;

    // Set projection
    QMatrix4x4 projection;
    projection.setToIdentity();
    projection.ortho(viewport);
    projection = m_vulkanWindow->clipCorrectionMatrix() * projection;
    m_projectionUniform->Update(projection.data());

    // Create descriptor pool
    const auto & sizes = m_vulkanShader->GetPoolSizes(0);
    VkDescriptorPoolCreateInfo pool { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, nullptr,
                                      0, 1, static_cast<uint32_t>(sizes.size()), sizes.data() };
    if (m_vulkanFuncs->vkCreateDescriptorPool(m_vulkanDevice, &pool, nullptr, &m_descriptorPool) != VK_SUCCESS)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create descriptor pool for projection");
        return nullptr;
    }

    // Create descriptor
    VkDescriptorSetLayout layout = m_vulkanShader->GetDescSetLayout(0);
    VkDescriptorSetAllocateInfo alloc { };
    alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc.descriptorPool = m_descriptorPool;
    alloc.descriptorSetCount = 1;
    alloc.pSetLayouts = &layout;

    if (m_vulkanFuncs->vkAllocateDescriptorSets(m_vulkanDevice, &alloc,
                                                    &m_projectionDescriptor) != VK_SUCCESS)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "Failed to allocate projection descriptor set");
        return nullptr;
    }

    // Update descriptor
    auto buffdesc = m_projectionUniform->GetBufferInfo();
    VkWriteDescriptorSet write { };
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_projectionDescriptor;
    write.dstBinding = 0;
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.descriptorCount = 1;
    write.pBufferInfo = &buffdesc;
    m_vulkanFuncs->vkUpdateDescriptorSets(m_vulkanDevice, 1, &write, 0, nullptr);

    return m_vulkanRender;
}

void MythVisualVulkan::TearDownVulkan()
{
    if (IsValidVulkan())
    {
        if (m_pipeline)
            m_vulkanFuncs->vkDestroyPipeline(m_vulkanDevice, m_pipeline, nullptr);
        if (m_descriptorPool)
            m_vulkanFuncs->vkDestroyDescriptorPool(m_vulkanDevice, m_descriptorPool, nullptr);
    }

    delete m_projectionUniform;
    delete m_vulkanShader;

    m_pipeline = MYTH_NULL_DISPATCH;
    m_projectionDescriptor = MYTH_NULL_DISPATCH; // destroyed with pool
    m_descriptorPool = MYTH_NULL_DISPATCH;
    m_projectionUniform = nullptr;
    m_vulkanShader = nullptr;
}
