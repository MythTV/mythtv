#ifndef MYTHVISUALVULKAN_H
#define MYTHVISUALVULKAN_H

// MythTV
#include "libmythui/vulkan/mythshadervulkan.h"

class MythUniformBufferVulkan;

class MythVisualVulkan : public MythVulkanObject
{
  public:
    MythVisualVulkan(MythRenderVulkan* Render,
                     std::vector<VkDynamicState> Dynamic,
                     std::vector<int> Stages,
                     const MythShaderMap* Sources,
                     const MythBindingMap* Bindings);
    virtual ~MythVisualVulkan();

    virtual MythRenderVulkan* InitialiseVulkan(QRect /*Area*/);
    virtual void              TearDownVulkan  ();

  protected:
    MythShaderVulkan*        m_vulkanShader         { nullptr };
    VkPipeline               m_pipeline             { MYTH_NULL_DISPATCH };
    VkDescriptorPool         m_descriptorPool       { MYTH_NULL_DISPATCH };
    VkDescriptorSet          m_projectionDescriptor { MYTH_NULL_DISPATCH };
    MythUniformBufferVulkan* m_projectionUniform    { nullptr };
    std::vector<VkDynamicState> m_dynamicState      { };
    std::vector<int>         m_shaderStages         { };
    const MythShaderMap*     m_shaderSources        { nullptr };
    const MythBindingMap*    m_shaderBindings       { nullptr };
};
#endif
