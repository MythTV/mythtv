// MythTV
#include "mythcorecontext.h"
#include "mythmainwindow.h"
#include "mythpainter_qt.h"
#include "mythpainterwindowqt.h"
#include "mythpainterwindow.h"

#ifdef USING_OPENGL
#include "opengl/mythpainterwindowopengl.h"
#include "opengl/mythpainteropengl.h"
#endif

#ifdef USING_VULKAN
#include "vulkan/mythpainterwindowvulkan.h"
#include "vulkan/mythpaintervulkan.h"
#endif

QString MythPainterWindow::CreatePainters(MythMainWindow *MainWindow,
                                          MythPainterWindow *&PaintWin,
                                          MythPainter *&Painter)
{
    bool warn = false;

#ifdef USING_VULKAN
    auto *vulkan = new MythPainterWindowVulkan(MainWindow);
    if (vulkan && vulkan->IsValid())
    {
        PaintWin = vulkan;
        auto *render = dynamic_cast<MythRenderVulkan*>(vulkan->GetRenderDevice());
        auto *window = dynamic_cast<MythWindowVulkan*>(vulkan->GetVulkanWindow());
        Painter = new MythPainterVulkan(render, window);
        return QString();
    }
    delete vulkan;
#endif

    // only OpenGL provides video playback
#ifdef USING_OPENGL
    auto* glwindow = new MythPainterWindowOpenGL(MainWindow);
    if (glwindow && glwindow->IsValid())
    {
        PaintWin = glwindow;
        auto *render = dynamic_cast<MythRenderOpenGL*>(glwindow->GetRenderDevice());
        Painter = new MythOpenGLPainter(render, MainWindow);
        return QString();
    }
    delete glwindow;
#endif

    // Fallback to Qt painter as the last resort.
    LOG(VB_GENERAL, LOG_INFO, "Using the Qt painter. Video playback will not work!");
    Painter = new MythQtPainter();
    PaintWin = new MythPainterWindowQt(MainWindow);
    warn = QCoreApplication::applicationName() == MYTH_APPNAME_MYTHFRONTEND;

    return warn ? tr("Warning: OpenGL is not available.") : QString();
}

void MythPainterWindow::DestroyPainters(MythPainterWindow *&PaintWin, MythPainter *&Painter)
{
    delete Painter;
    delete PaintWin;
    Painter = nullptr;
    PaintWin = nullptr;
}

MythPainterWindow::MythPainterWindow(MythMainWindow *MainWin)
  : QWidget(MainWin)
{
}

MythRender* MythPainterWindow::GetRenderDevice(void)
{
    return m_render;
}

bool MythPainterWindow::RenderIsShared(void)
{
    return m_render && m_render->IsShared();
}
