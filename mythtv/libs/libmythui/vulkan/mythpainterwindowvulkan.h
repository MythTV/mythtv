#ifndef MYTHPAINTERWINDOWVULKAN_H
#define MYTHPAINTERWINDOWVULKAN_H

// MythTV
#include "mythpainterwindow.h"

class QVulkanInstance;
class MythWindowVulkan;

#define MYTH_PAINTER_VULKAN QString("Vulkan")

class MythPainterWindowVulkan : public MythPainterWindow
{
  public:
    explicit MythPainterWindowVulkan(MythMainWindow* MainWindow);
   ~MythPainterWindowVulkan() override;

    bool              IsValid         (void) const;
    MythWindowVulkan* GetVulkanWindow (void);
    void              paintEvent      (QPaintEvent* PaintEvent) override;
    void              resizeEvent     (QResizeEvent* ResizeEvent) override;

  private:
    Q_DISABLE_COPY(MythPainterWindowVulkan)

    bool              m_valid   { false   };
    MythMainWindow*   m_parent  { nullptr };
    QVulkanInstance*  m_vulkan  { nullptr };
    QWidget*          m_wrapper { nullptr };
    MythWindowVulkan* m_window  { nullptr };
};

#endif
