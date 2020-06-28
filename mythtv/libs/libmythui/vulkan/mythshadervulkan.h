#ifndef MYTHSHADERVULKAN_H
#define MYTHSHADERVULKAN_H

// MythTV
#include "vulkan/mythrendervulkan.h"

using MythShaderMap   = std::map<int, std::pair<QString, std::vector<uint32_t>>>;
using MythGLSLStage   = std::pair<VkShaderStageFlags, QString>;
using MythSPIRVStage  = std::pair<VkShaderStageFlags, const std::vector<uint32_t>>;
using MythVertexAttrs = std::vector<VkVertexInputAttributeDescription>;
using MythSetLayout   = std::pair<int, VkDescriptorSetLayoutBinding>;
using MythStageLayout = std::vector<MythSetLayout>;
using MythBindingDesc = std::tuple<MythStageLayout,
                                   VkVertexInputBindingDescription,
                                   MythVertexAttrs,
                                   VkPushConstantRange>;
using MythBindingMap  = std::map<int, MythBindingDesc>;

class MythShaderVulkan : protected MythVulkanObject
{
  public:
    static MythShaderVulkan* Create(MythRenderVulkan* Render, VkDevice Device,
                                    QVulkanDeviceFunctions* Functions,
                                    const std::vector<int> &Stages,
                                    const MythShaderMap *Sources = nullptr,
                                    const MythBindingMap *Bindings = nullptr);
   ~MythShaderVulkan();

    VkPipelineLayout        GetPipelineLayout      (void) const;
    const MythVertexAttrs&  GetVertexAttributes    (void) const;
    const VkVertexInputBindingDescription& GetVertexBindingDesc(void) const;
    const std::vector<VkPipelineShaderStageCreateInfo>& Stages(void) const;
    const std::vector<VkDescriptorPoolSize>& GetPoolSizes(size_t Set) const;
    VkDescriptorSetLayout GetDescSetLayout(size_t Set) const;

#ifdef USING_GLSLANG
    static bool  InitGLSLang(bool Release = false);
#endif

  protected:
    MythShaderVulkan(MythRenderVulkan* Render, VkDevice Device,
                     QVulkanDeviceFunctions* Functions,
                     const std::vector<int> &Stages,
                     const MythShaderMap *Sources = nullptr,
                     const MythBindingMap *Bindings = nullptr);

  private:
    Q_DISABLE_COPY(MythShaderVulkan)
#ifdef USING_GLSLANG
    bool CreateShaderFromGLSL  (const std::vector<MythGLSLStage> &Stages);
#endif
    bool CreateShaderFromSPIRV (const std::vector<MythSPIRVStage> &Stages);

    VkVertexInputBindingDescription              m_vertexBindingDesc{};
    MythVertexAttrs                              m_vertexAttributes;
    std::vector<VkPipelineShaderStageCreateInfo> m_stages;
    std::vector<uint32_t*>                       m_spirv;
    VkPipelineLayout                             m_pipelineLayout   { nullptr };

    // these are per set
    std::vector<VkDescriptorSetLayout>           m_descriptorSetLayouts;
    std::vector<std::vector<VkDescriptorPoolSize>> m_descriptorPoolSizes;
};

#endif
