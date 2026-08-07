#ifndef MYTHWINDOWVULKAN_H
#define MYTHWINDOWVULKAN_H

// Qt
#include <QVulkanWindow>

class MythRenderVulkan;
class MythPainterWindowVulkan;

class MythWindowVulkan : public QVulkanWindow
{
  public:
    MythWindowVulkan(MythRenderVulkan *Render);
   ~MythWindowVulkan() override;

    QVulkanWindowRenderer* createRenderer(void) override;

  protected:
    bool event(QEvent *Event) override; // QWindow

  private:
    Q_DISABLE_COPY(MythWindowVulkan)
    MythRenderVulkan* m_render { nullptr };
};

#endif
