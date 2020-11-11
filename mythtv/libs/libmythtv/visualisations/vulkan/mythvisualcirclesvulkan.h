#ifndef MYTHVISUALCIRCLESVULKAN_H
#define MYTHVISUALCIRCLESVULKAN_H

// MythTV
#include "visualisations/vulkan/mythvisualvulkan.h"
#include "visualisations/videovisualcircles.h"

extern "C" {
struct alignas(16) CirclesBuffer
{
    float transform [16];
    float positions [4];
    float params    [4]; // vec2 centre, float radius (outer), float width
    float color     [4];
};
}

class MythVisualCirclesVulkan : public VideoVisualCircles, public MythVisualVulkan
{
  public:
    MythVisualCirclesVulkan(AudioPlayer* Audio, MythRenderVulkan* Render);
   ~MythVisualCirclesVulkan() override;

  protected:
    void DrawPriv(MythPainter* /*Painter*/, QPaintDevice* /*Device*/) override;

  private:
    MythRenderVulkan* InitialiseVulkan (QRect Area) override;
    void              TearDownVulkan   () override;

    CirclesBuffer m_pushBuffer { };
    QRect         m_vulkanArea;
};

#endif
