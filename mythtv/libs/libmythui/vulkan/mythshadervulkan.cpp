// MythTV
#include "libmythbase/mythlogging.h"
#include "vulkan/mythshadersvulkan.h"
#include "vulkan/mythshadervulkan.h"

// Std
#include <algorithm>

#define LOC QString("VulkanShader: ")

// libglslang
#ifdef USING_GLSLANG
#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>

static const TBuiltInResource s_TBuiltInResource = {
    /* .MaxLights = */ 32,
    /* .MaxClipPlanes = */ 6,
    /* .MaxTextureUnits = */ 32,
    /* .MaxTextureCoords = */ 32,
    /* .MaxVertexAttribs = */ 64,
    /* .MaxVertexUniformComponents = */ 4096,
    /* .MaxVaryingFloats = */ 64,
    /* .MaxVertexTextureImageUnits = */ 32,
    /* .MaxCombinedTextureImageUnits = */ 80,
    /* .MaxTextureImageUnits = */ 32,
    /* .MaxFragmentUniformComponents = */ 4096,
    /* .MaxDrawBuffers = */ 32,
    /* .MaxVertexUniformVectors = */ 128,
    /* .MaxVaryingVectors = */ 8,
    /* .MaxFragmentUniformVectors = */ 16,
    /* .MaxVertexOutputVectors = */ 16,
    /* .MaxFragmentInputVectors = */ 15,
    /* .MinProgramTexelOffset = */ -8,
    /* .MaxProgramTexelOffset = */ 7,
    /* .MaxClipDistances = */ 8,
    /* .MaxComputeWorkGroupCountX = */ 65535,
    /* .MaxComputeWorkGroupCountY = */ 65535,
    /* .MaxComputeWorkGroupCountZ = */ 65535,
    /* .MaxComputeWorkGroupSizeX = */ 1024,
    /* .MaxComputeWorkGroupSizeY = */ 1024,
    /* .MaxComputeWorkGroupSizeZ = */ 64,
    /* .MaxComputeUniformComponents = */ 1024,
    /* .MaxComputeTextureImageUnits = */ 16,
    /* .MaxComputeImageUniforms = */ 8,
    /* .MaxComputeAtomicCounters = */ 8,
    /* .MaxComputeAtomicCounterBuffers = */ 1,
    /* .MaxVaryingComponents = */ 60,
    /* .MaxVertexOutputComponents = */ 64,
    /* .MaxGeometryInputComponents = */ 64,
    /* .MaxGeometryOutputComponents = */ 128,
    /* .MaxFragmentInputComponents = */ 128,
    /* .MaxImageUnits = */ 8,
    /* .MaxCombinedImageUnitsAndFragmentOutputs = */ 8,
    /* .MaxCombinedShaderOutputResources = */ 8,
    /* .MaxImageSamples = */ 0,
    /* .MaxVertexImageUniforms = */ 0,
    /* .MaxTessControlImageUniforms = */ 0,
    /* .MaxTessEvaluationImageUniforms = */ 0,
    /* .MaxGeometryImageUniforms = */ 0,
    /* .MaxFragmentImageUniforms = */ 8,
    /* .MaxCombinedImageUniforms = */ 8,
    /* .MaxGeometryTextureImageUnits = */ 16,
    /* .MaxGeometryOutputVertices = */ 256,
    /* .MaxGeometryTotalOutputComponents = */ 1024,
    /* .MaxGeometryUniformComponents = */ 1024,
    /* .MaxGeometryVaryingComponents = */ 64,
    /* .MaxTessControlInputComponents = */ 128,
    /* .MaxTessControlOutputComponents = */ 128,
    /* .MaxTessControlTextureImageUnits = */ 16,
    /* .MaxTessControlUniformComponents = */ 1024,
    /* .MaxTessControlTotalOutputComponents = */ 4096,
    /* .MaxTessEvaluationInputComponents = */ 128,
    /* .MaxTessEvaluationOutputComponents = */ 128,
    /* .MaxTessEvaluationTextureImageUnits = */ 16,
    /* .MaxTessEvaluationUniformComponents = */ 1024,
    /* .MaxTessPatchComponents = */ 120,
    /* .MaxPatchVertices = */ 32,
    /* .MaxTessGenLevel = */ 64,
    /* .MaxViewports = */ 16,
    /* .MaxVertexAtomicCounters = */ 0,
    /* .MaxTessControlAtomicCounters = */ 0,
    /* .MaxTessEvaluationAtomicCounters = */ 0,
    /* .MaxGeometryAtomicCounters = */ 0,
    /* .MaxFragmentAtomicCounters = */ 8,
    /* .MaxCombinedAtomicCounters = */ 8,
    /* .MaxAtomicCounterBindings = */ 1,
    /* .MaxVertexAtomicCounterBuffers = */ 0,
    /* .MaxTessControlAtomicCounterBuffers = */ 0,
    /* .MaxTessEvaluationAtomicCounterBuffers = */ 0,
    /* .MaxGeometryAtomicCounterBuffers = */ 0,
    /* .MaxFragmentAtomicCounterBuffers = */ 1,
    /* .MaxCombinedAtomicCounterBuffers = */ 1,
    /* .MaxAtomicCounterBufferSize = */ 16384,
    /* .MaxTransformFeedbackBuffers = */ 4,
    /* .MaxTransformFeedbackInterleavedComponents = */ 64,
    /* .MaxCullDistances = */ 8,
    /* .MaxCombinedClipAndCullDistances = */ 8,
    /* .MaxSamples = */ 4,
    /* .maxMeshOutputVerticesNV = */ 256,
    /* .maxMeshOutputPrimitivesNV = */ 512,
    /* .maxMeshWorkGroupSizeX_NV = */ 32,
    /* .maxMeshWorkGroupSizeY_NV = */ 1,
    /* .maxMeshWorkGroupSizeZ_NV = */ 1,
    /* .maxTaskWorkGroupSizeX_NV = */ 32,
    /* .maxTaskWorkGroupSizeY_NV = */ 1,
    /* .maxTaskWorkGroupSizeZ_NV = */ 1,
    /* .maxMeshViewCountNV = */ 4,

    .limits = {
        /* .nonInductiveForLoops = */ true,
        /* .whileLoops = */ true,
        /* .doWhileLoops = */ true,
        /* .generalUniformIndexing = */ true,
        /* .generalAttributeMatrixVectorIndexing = */ true,
        /* .generalVaryingIndexing = */ true,
        /* .generalSamplerIndexing = */ true,
        /* .generalVariableIndexing = */ true,
        /* .generalConstantMatrixVectorIndexing = */ true,
    }
};

static auto GLSLangCompile(EShLanguage Stage, const QString &Code)
{
    std::vector<uint32_t> result;
    auto *shader = new glslang::TShader(Stage);
    if (!shader)
        return result;

    QByteArray data = Code.toLocal8Bit();
    const char *tmp = data.constData();
    shader->setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_1);
    shader->setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_3);
    shader->setStrings(&tmp, 1);
    if (!shader->parse(&s_TBuiltInResource, glslang::EShTargetVulkan_1_1, true, EShMsgDefault))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Shader parse error:");
        LOG(VB_GENERAL, LOG_ERR, shader->getInfoLog());
        delete shader;
        return result;
    }

    auto *program = new glslang::TProgram();
    if (!program)
    {
        delete shader;
        return result;
    }

    program->addShader(shader);
    if (program->link(EShMsgDefault))
    {
        auto ByteCodeToString = [](std::vector<uint32_t> &OpCodes)
        {
            QString string;
            int count = 0;
            for (uint32_t opcode : OpCodes)
            {
                if (count++ == 0) string += "\n";
                if (count > 5) count = 0;
                string += "0x" + QString("%1, ").arg(opcode, 8, 16, QLatin1Char('0')).toUpper();
            }
            return string;
        };

        glslang::SpvOptions options { };
        options.generateDebugInfo = false;
        options.disassemble       = false;
        options.validate          = false;
        options.disableOptimizer  = false;
        options.optimizeSize      = true;
        GlslangToSpv(*program->getIntermediate(Stage), result, &options);
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Generated SPIR-V: %1bytes").arg(result.size() * sizeof (uint32_t)));
        LOG(VB_GENERAL, LOG_INFO, "Source:\n" + Code);
        LOG(VB_GENERAL, LOG_INFO, "ByteCode:\n" + ByteCodeToString(result));
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Shader link error:");
        LOG(VB_GENERAL, LOG_ERR, program->getInfoLog());
    }

    delete shader;
    delete program;
    return result;
}

bool MythShaderVulkan::InitGLSLang(bool Release /* = false */)
{
    static QMutex s_glslangLock;
    static int    s_glslangRefcount = 0;
    static bool   s_initSuccess = false;
    QMutexLocker locker(&s_glslangLock);

    if (Release)
    {
        s_glslangRefcount--;
        if (s_glslangRefcount < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "GLSLang ref count error");
            return false;
        }

        if (s_glslangRefcount < 1)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + "GLSLang released");
            glslang::FinalizeProcess();
        }
        return true;
    }

    if (s_glslangRefcount < 1)
    {
        s_initSuccess = glslang::InitializeProcess();
        if (s_initSuccess)
            LOG(VB_GENERAL, LOG_INFO, LOC + "GLSLang initialised");
    }
    s_glslangRefcount++;
    return s_initSuccess;
}

bool MythShaderVulkan::CreateShaderFromGLSL(const std::vector<MythGLSLStage> &Stages)
{
    if (Stages.empty())
        return false;

    if (!MythShaderVulkan::InitGLSLang())
        return false;

    std::vector<MythSPIRVStage> spirvstages;

    auto glslangtype = [](VkShaderStageFlags Type)
    {
        switch (Type)
        {
            case VK_SHADER_STAGE_VERTEX_BIT:   return EShLangVertex;
            case VK_SHADER_STAGE_FRAGMENT_BIT: return EShLangFragment;
            case VK_SHADER_STAGE_COMPUTE_BIT:  return EShLangCompute;
            default: break;
        }
        return EShLangCount;
    };

    for (const auto& stage : Stages)
        if (glslangtype(stage.first) != EShLangCount)
            spirvstages.emplace_back(stage.first, GLSLangCompile(glslangtype(stage.first), stage.second));
    MythShaderVulkan::InitGLSLang(true);
    return CreateShaderFromSPIRV(spirvstages);
}
#endif

/*! \class MythShaderVulkan
 * \brief Creates shader objects suitable for use with the Vulkan API
 *
 * By default, shaders are created from SPIR-V bytecode. If libglslang support
 * is available and the environment variable MYTHTV_GLSLANG is set, shaders will
 * be compiled from the GLSL source code. libglslang support is currently intended
 * for development use only - i.e. to test and compile shaders.
*/
MythShaderVulkan* MythShaderVulkan::Create(MythVulkanObject *Vulkan,
                                           const std::vector<int> &Stages,
                                           const MythShaderMap *Sources,
                                           const MythBindingMap *Bindings)
{
    auto * result = new MythShaderVulkan(Vulkan, Stages, Sources, Bindings);
    if (result && !result->IsValidVulkan())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create shader");
        delete result;
        result = nullptr;
    }
    return result;
}

MythShaderVulkan::MythShaderVulkan(MythVulkanObject *Vulkan,
                                   const std::vector<int> &Stages,
                                   const MythShaderMap  *Sources,
                                   const MythBindingMap *Bindings)
  : MythVulkanObject(Vulkan)
{
    if (!m_vulkanValid || Stages.empty())
        return;
    m_vulkanValid = false;

    if (!Sources)
        Sources = &k450DefaultShaders;

    if (!Bindings)
        Bindings = &k450ShaderBindings;

    // ensure we have sources and bindings
    for (const auto & stage : Stages)
        if (Sources->find(stage) == Sources->end() || Bindings->find(stage) == Bindings->end())
            return;

    // build the descriptor set layouts from the shader descriptions
    bool foundvertices = false;
    bool pushconstants = false;
    VkPushConstantRange ranges = { };

    std::map<int, std::vector<VkDescriptorSetLayoutBinding>> layoutbindings;
    std::map<int, std::vector<VkDescriptorPoolSize>> poolsizes;
    for (const auto & stage : Stages)
    {
        bool isvertex = false;
        MythBindingDesc desc = Bindings->at(stage);
        m_topology = std::get<0>(desc);
        MythStageLayout binding = std::get<1>(desc);

        for (auto & stagelayout : binding)
        {
            if (stagelayout.second.stageFlags == VK_SHADER_STAGE_VERTEX_BIT)
                isvertex = true;

            if (layoutbindings.find(stagelayout.first) != layoutbindings.end())
                layoutbindings.at(stagelayout.first).emplace_back(stagelayout.second);
            else
                layoutbindings.insert( { stagelayout.first, { stagelayout.second } } );

            VkDescriptorPoolSize poolsize = {stagelayout.second.descriptorType, stagelayout.second.descriptorCount};
            if (poolsizes.find(stagelayout.first) != poolsizes.end())
                poolsizes.at(stagelayout.first).emplace_back(poolsize);
            else
                poolsizes.insert( { stagelayout.first, { poolsize } } );
        }

        if (isvertex && !foundvertices)
        {
            foundvertices = true;
            m_vertexBindingDesc = std::get<2>(desc);
            m_vertexAttributes  = std::get<3>(desc);
        }

        if (!pushconstants)
        {
            VkPushConstantRange range = std::get<4>(desc);
            if (range.stageFlags)
            {
                pushconstants = true;
                ranges = range;
            }
        }
    }

    // convert poolsizes to vector
    for (const auto& poolsize : poolsizes)
        m_descriptorPoolSizes.emplace_back(poolsize.second);

    // create the desriptor layouts
    for (auto & layoutbinding : layoutbindings)
    {
        VkDescriptorSetLayout layout = MYTH_NULL_DISPATCH;
        VkDescriptorSetLayoutCreateInfo layoutinfo { };
        layoutinfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutinfo.bindingCount = static_cast<uint32_t>(layoutbinding.second.size());
        layoutinfo.pBindings    = layoutbinding.second.data();
        if (m_vulkanFuncs->vkCreateDescriptorSetLayout(m_vulkanDevice, &layoutinfo, nullptr, &layout) != VK_SUCCESS)
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create DescriptorSetLayout");
        else
            m_descriptorSetLayouts.push_back(layout);
    }

    if (m_descriptorSetLayouts.size() != layoutbindings.size())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create all layouts");
        return;
    }

    VkPipelineLayoutCreateInfo pipelinelayout { };
    pipelinelayout.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelinelayout.setLayoutCount = static_cast<uint32_t>(m_descriptorSetLayouts.size());
    pipelinelayout.pSetLayouts    = m_descriptorSetLayouts.data();

    if (pushconstants)
    {
        pipelinelayout.pushConstantRangeCount = 1;
        pipelinelayout.pPushConstantRanges    = &ranges;
    }

    if (m_vulkanFuncs->vkCreatePipelineLayout(m_vulkanDevice, &pipelinelayout, nullptr, &m_pipelineLayout) != VK_SUCCESS)
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create pipeline layout");

    if (!m_pipelineLayout)
        return;

#ifdef USING_GLSLANG
    static bool useglsl = qEnvironmentVariableIsSet("MYTHTV_GLSLANG");
    if (useglsl)
    {
        std::vector<MythGLSLStage> glslstages;
        std::transform(Stages.cbegin(), Stages.cend(), std::back_inserter(glslstages),
                       [&](int Stage) { return MythGLSLStage{Stage & VK_SHADER_STAGE_ALL_GRAPHICS, Sources->at(Stage).first }; });
        m_vulkanValid = MythShaderVulkan::CreateShaderFromGLSL(glslstages);
        return;
    }
#endif

    std::vector<MythSPIRVStage> stages;
    std::transform(Stages.cbegin(), Stages.cend(), std::back_inserter(stages),
                   [&](int Stage) { return MythSPIRVStage{Stage & VK_SHADER_STAGE_ALL_GRAPHICS, Sources->at(Stage).second }; });
    m_vulkanValid = MythShaderVulkan::CreateShaderFromSPIRV(stages);
}

MythShaderVulkan::~MythShaderVulkan()
{
    if (m_vulkanValid)
    {
        m_vulkanFuncs->vkDestroyPipelineLayout(m_vulkanDevice, m_pipelineLayout, nullptr);
        for (auto & layout : m_descriptorSetLayouts)
            m_vulkanFuncs->vkDestroyDescriptorSetLayout(m_vulkanDevice, layout, nullptr);
        for (auto & stage : m_stages)
            m_vulkanFuncs->vkDestroyShaderModule(m_vulkanDevice, stage.module, nullptr);
    }
    for (auto * spirv : m_spirv)
        delete [] spirv;
}

bool MythShaderVulkan::CreateShaderFromSPIRV(const std::vector<MythSPIRVStage> &Stages)
{
    if (Stages.empty())
        return false;

    if (std::any_of(Stages.cbegin(), Stages.cend(), [](const MythSPIRVStage& Stage) { return Stage.second.empty(); }))
        return false;

    bool success = true;
    for (const auto & stage : Stages)
    {
        auto size = stage.second.size() * sizeof (uint32_t);
        auto *code = reinterpret_cast<uint32_t*>(new uint8_t [size]);
        memcpy(code, stage.second.data(), size);
        VkShaderModule module = MYTH_NULL_DISPATCH;
        VkShaderModuleCreateInfo create { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, nullptr, 0, size, code };
        success &= (m_vulkanFuncs->vkCreateShaderModule(m_vulkanDevice, &create, nullptr, &module) == VK_SUCCESS);
        m_stages.push_back( { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr,
                              0, static_cast<VkShaderStageFlagBits>(stage.first), module, "main", nullptr } );
        m_spirv.push_back(code);
    }
    return success;
}

const std::vector<VkPipelineShaderStageCreateInfo>& MythShaderVulkan::Stages(void) const
{
    return m_stages;
}


const MythVertexAttrs& MythShaderVulkan::GetVertexAttributes(void) const
{
    return m_vertexAttributes;
}

const VkVertexInputBindingDescription& MythShaderVulkan::GetVertexBindingDesc(void) const
{
    return m_vertexBindingDesc;
}

VkPipelineLayout MythShaderVulkan::GetPipelineLayout(void) const
{
    return m_pipelineLayout;
}

const std::vector<VkDescriptorPoolSize>& MythShaderVulkan::GetPoolSizes(size_t Set) const
{
    if (Set < m_descriptorPoolSizes.size())
        return m_descriptorPoolSizes.at(Set);
    static const std::vector<VkDescriptorPoolSize> broken;
    return broken;
}

VkDescriptorSetLayout MythShaderVulkan::GetDescSetLayout(size_t Set) const
{
    if (Set < m_descriptorSetLayouts.size())
        return m_descriptorSetLayouts.at(Set);
    return MYTH_NULL_DISPATCH;
}

VkPrimitiveTopology MythShaderVulkan::GetTopology() const
{
    return m_topology;
}
