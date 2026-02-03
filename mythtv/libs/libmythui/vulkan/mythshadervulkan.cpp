// MythTV
#include "libmythbase/mythconfig.h"
#include "libmythbase/mythlogging.h"
#include "vulkan/mythshadersvulkan.h"
#include "vulkan/mythshadervulkan.h"

// Std
#include <algorithm>

#define LOC QString("VulkanShader: ")

// libglslang
#if CONFIG_LIBGLSLANG
#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>

static const TBuiltInResource s_TBuiltInResource = {
    .maxLights = 32,
    .maxClipPlanes = 6,
    .maxTextureUnits = 32,
    .maxTextureCoords = 32,
    .maxVertexAttribs = 64,
    .maxVertexUniformComponents = 4096,
    .maxVaryingFloats = 64,
    .maxVertexTextureImageUnits = 32,
    .maxCombinedTextureImageUnits = 80,
    .maxTextureImageUnits = 32,
    .maxFragmentUniformComponents = 4096,
    .maxDrawBuffers = 32,
    .maxVertexUniformVectors = 128,
    .maxVaryingVectors = 8,
    .maxFragmentUniformVectors = 16,
    .maxVertexOutputVectors = 16,
    .maxFragmentInputVectors = 15,
    .minProgramTexelOffset = -8,
    .maxProgramTexelOffset = 7,
    .maxClipDistances = 8,
    .maxComputeWorkGroupCountX = 65535,
    .maxComputeWorkGroupCountY = 65535,
    .maxComputeWorkGroupCountZ = 65535,
    .maxComputeWorkGroupSizeX = 1024,
    .maxComputeWorkGroupSizeY = 1024,
    .maxComputeWorkGroupSizeZ = 64,
    .maxComputeUniformComponents = 1024,
    .maxComputeTextureImageUnits = 16,
    .maxComputeImageUniforms = 8,
    .maxComputeAtomicCounters = 8,
    .maxComputeAtomicCounterBuffers = 1,
    .maxVaryingComponents = 60,
    .maxVertexOutputComponents = 64,
    .maxGeometryInputComponents = 64,
    .maxGeometryOutputComponents = 128,
    .maxFragmentInputComponents = 128,
    .maxImageUnits = 8,
    .maxCombinedImageUnitsAndFragmentOutputs = 8,
    .maxCombinedShaderOutputResources = 8,
    .maxImageSamples = 0,
    .maxVertexImageUniforms = 0,
    .maxTessControlImageUniforms = 0,
    .maxTessEvaluationImageUniforms = 0,
    .maxGeometryImageUniforms = 0,
    .maxFragmentImageUniforms = 8,
    .maxCombinedImageUniforms = 8,
    .maxGeometryTextureImageUnits = 16,
    .maxGeometryOutputVertices = 256,
    .maxGeometryTotalOutputComponents = 1024,
    .maxGeometryUniformComponents = 1024,
    .maxGeometryVaryingComponents = 64,
    .maxTessControlInputComponents = 128,
    .maxTessControlOutputComponents = 128,
    .maxTessControlTextureImageUnits = 16,
    .maxTessControlUniformComponents = 1024,
    .maxTessControlTotalOutputComponents = 4096,
    .maxTessEvaluationInputComponents = 128,
    .maxTessEvaluationOutputComponents = 128,
    .maxTessEvaluationTextureImageUnits = 16,
    .maxTessEvaluationUniformComponents = 1024,
    .maxTessPatchComponents = 120,
    .maxPatchVertices = 32,
    .maxTessGenLevel = 64,
    .maxViewports = 16,
    .maxVertexAtomicCounters = 0,
    .maxTessControlAtomicCounters = 0,
    .maxTessEvaluationAtomicCounters = 0,
    .maxGeometryAtomicCounters = 0,
    .maxFragmentAtomicCounters = 8,
    .maxCombinedAtomicCounters = 8,
    .maxAtomicCounterBindings = 1,
    .maxVertexAtomicCounterBuffers = 0,
    .maxTessControlAtomicCounterBuffers = 0,
    .maxTessEvaluationAtomicCounterBuffers = 0,
    .maxGeometryAtomicCounterBuffers = 0,
    .maxFragmentAtomicCounterBuffers = 1,
    .maxCombinedAtomicCounterBuffers = 1,
    .maxAtomicCounterBufferSize = 16384,
    .maxTransformFeedbackBuffers = 4,
    .maxTransformFeedbackInterleavedComponents = 64,
    .maxCullDistances = 8,
    .maxCombinedClipAndCullDistances = 8,
    .maxSamples = 4,
    .maxMeshOutputVerticesNV = 256,
    .maxMeshOutputPrimitivesNV = 512,
    .maxMeshWorkGroupSizeX_NV = 32,
    .maxMeshWorkGroupSizeY_NV = 1,
    .maxMeshWorkGroupSizeZ_NV = 1,
    .maxTaskWorkGroupSizeX_NV = 32,
    .maxTaskWorkGroupSizeY_NV = 1,
    .maxTaskWorkGroupSizeZ_NV = 1,
    .maxMeshViewCountNV = 4,
#if HAVE_TBUILTINRESOURCE_EXT_FIELDS
    .maxMeshOutputVerticesEXT = 0,
    .maxMeshOutputPrimitivesEXT = 0,
    .maxMeshWorkGroupSizeX_EXT = 0,
    .maxMeshWorkGroupSizeY_EXT = 0,
    .maxMeshWorkGroupSizeZ_EXT = 0,
    .maxTaskWorkGroupSizeX_EXT = 0,
    .maxTaskWorkGroupSizeY_EXT = 0,
    .maxTaskWorkGroupSizeZ_EXT = 0,
    .maxMeshViewCountEXT = 0,
    .maxDualSourceDrawBuffersEXT = 0,
#endif

    .limits = {
        .nonInductiveForLoops = true,
        .whileLoops = true,
        .doWhileLoops = true,
        .generalUniformIndexing = true,
        .generalAttributeMatrixVectorIndexing = true,
        .generalVaryingIndexing = true,
        .generalSamplerIndexing = true,
        .generalVariableIndexing = true,
        .generalConstantMatrixVectorIndexing = true,
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
        if (!Sources->contains(stage) || !Bindings->contains(stage))
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

            if (layoutbindings.contains(stagelayout.first))
                layoutbindings.at(stagelayout.first).emplace_back(stagelayout.second);
            else
                layoutbindings.insert( { stagelayout.first, { stagelayout.second } } );

            VkDescriptorPoolSize poolsize = {stagelayout.second.descriptorType, stagelayout.second.descriptorCount};
            if (poolsizes.contains(stagelayout.first))
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

#if CONFIG_LIBGLSLANG
    static bool useglsl = qEnvironmentVariableIsSet("MYTHTV_GLSLANG");
    if (useglsl)
    {
        std::vector<MythGLSLStage> glslstages;
        std::ranges::transform(Stages, std::back_inserter(glslstages),
                       [&](int Stage) { return MythGLSLStage{Stage & VK_SHADER_STAGE_ALL_GRAPHICS, Sources->at(Stage).first }; });
        m_vulkanValid = MythShaderVulkan::CreateShaderFromGLSL(glslstages);
        return;
    }
#endif

    std::vector<MythSPIRVStage> stages;
    std::ranges::transform(Stages, std::back_inserter(stages),
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

    if (std::ranges::any_of(Stages,
                            [](const MythSPIRVStage& Stage) { return Stage.second.empty(); }))
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
