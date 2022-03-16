#ifndef MYTHSHADERVULKAN_H
#define MYTHSHADERVULKAN_H

// MythTV
#include "libmythui/vulkan/mythrendervulkan.h"

using MythShaderMap   = std::map<int, std::pair<QString, std::vector<uint32_t>>>;
using MythGLSLStage   = std::pair<VkShaderStageFlags, QString>;
using MythSPIRVStage  = std::pair<VkShaderStageFlags, const std::vector<uint32_t>>;
using MythVertexAttrs = std::vector<VkVertexInputAttributeDescription>;
using MythSetLayout   = std::pair<int, VkDescriptorSetLayoutBinding>;
using MythStageLayout = std::vector<MythSetLayout>;
using MythBindingDesc = std::tuple<VkPrimitiveTopology,
                                   MythStageLayout,
                                   VkVertexInputBindingDescription,
                                   MythVertexAttrs,
                                   VkPushConstantRange>;
using MythBindingMap  = std::map<int, MythBindingDesc>;

class MUI_PUBLIC MythShaderVulkan : protected MythVulkanObject
{
  public:
    static MythShaderVulkan* Create(MythVulkanObject* Vulkan,
                                    const std::vector<int> &Stages,
                                    const MythShaderMap *Sources = nullptr,
                                    const MythBindingMap *Bindings = nullptr);
   ~MythShaderVulkan();

    VkPrimitiveTopology     GetTopology            () const;
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
    MythShaderVulkan(MythVulkanObject* Vulkan,
                     const std::vector<int> &Stages,
                     const MythShaderMap *Sources = nullptr,
                     const MythBindingMap *Bindings = nullptr);

  private:
    Q_DISABLE_COPY(MythShaderVulkan)
#ifdef USING_GLSLANG
    bool CreateShaderFromGLSL  (const std::vector<MythGLSLStage> &Stages);
#endif
    bool CreateShaderFromSPIRV (const std::vector<MythSPIRVStage> &Stages);

    VkPrimitiveTopology                          m_topology          { VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP };
    VkVertexInputBindingDescription              m_vertexBindingDesc { };
    MythVertexAttrs                              m_vertexAttributes;
    std::vector<VkPipelineShaderStageCreateInfo> m_stages;
    std::vector<uint32_t*>                       m_spirv;
    VkPipelineLayout                             m_pipelineLayout    { MYTH_NULL_DISPATCH };

    // these are per set
    std::vector<VkDescriptorSetLayout>           m_descriptorSetLayouts;
    std::vector<std::vector<VkDescriptorPoolSize>> m_descriptorPoolSizes;
};

#endif
