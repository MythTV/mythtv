#ifndef MYTHVISUALMONOSCOPEVULKAN_H
#define MYTHVISUALMONOSCOPEVULKAN_H

// MythTV
#include "vulkan/mythrendervulkan.h"
#include "visualisations/videovisualmonoscope.h"

class MythShaderVulkan;
class MythUniformBufferVulkan;
class MythBufferVulkan;

// Vertex buffer + Hue,Alpha,Zoom
using VertexState  = std::pair<MythBufferVulkan*, std::array<float,3>>;
using VertexStates = QVector<VertexState>;

extern "C" {
struct alignas(16) PushBuffer
{
    float transform [16];
    float color     [4];
};
}

class MythVisualMonoScopeVulkan : public VideoVisualMonoScope, public MythVulkanObject
{
  public:
    MythVisualMonoScopeVulkan(AudioPlayer* Audio, MythRender* Render, bool Fade);
   ~MythVisualMonoScopeVulkan() override;

    void Prepare (const QRect& Area) override;
    void Draw    (const QRect& Area, MythPainter* /*Painter*/, QPaintDevice* /*Device*/) override;

  private:
    MythRenderVulkan* Initialise (const QRect& Area);
    void              TearDown   ();

    MythShaderVulkan*        m_vulkanShader         { nullptr };
    VkPipeline               m_pipeline             { nullptr };
    VkDescriptorPool         m_descriptorPool       { nullptr };
    VkDescriptorSet          m_projectionDescriptor { nullptr };
    MythUniformBufferVulkan* m_projectionUniform    { nullptr };
    VertexStates             m_vertexBuffers;
    PushBuffer               m_pushBuffer { };
};

#endif
