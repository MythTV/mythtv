#ifndef MYTHVISUALMONOSCOPEVULKAN_H
#define MYTHVISUALMONOSCOPEVULKAN_H

// MythTV
#include "libmythui/vulkan/mythrendervulkan.h"
#include "visualisations/vulkan/mythvisualvulkan.h"
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

class MythVisualMonoScopeVulkan : public VideoVisualMonoScope, public MythVisualVulkan
{
  public:
    MythVisualMonoScopeVulkan(AudioPlayer* Audio, MythRender* Render, bool Fade);
   ~MythVisualMonoScopeVulkan() override;

    void Prepare (QRect Area) override;
    void Draw    (QRect Area, MythPainter* /*Painter*/, QPaintDevice* /*Device*/) override;

  private:
    MythRenderVulkan* InitialiseVulkan (QRect Area) override;
    void              TearDownVulkan   () override;

    VertexStates m_vertexBuffers;
    PushBuffer   m_pushBuffer { };
};

#endif
