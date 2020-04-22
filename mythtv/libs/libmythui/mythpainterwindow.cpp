// MythTV
#include "mythcorecontext.h"
#include "mythmainwindow.h"
#ifdef USING_OPENGL
#include "opengl/mythpainterwindowopengl.h"
#include "opengl/mythpainteropengl.h"
#endif
#include "mythpainter_qt.h"
#include "mythpainterwindowqt.h"
#include "mythpainterwindow.h"

QString MythPainterWindow::CreatePainters(MythMainWindow *MainWindow,
                                          MythPainterWindow *&PaintWin,
                                          MythPainter *&Painter)
{
    bool warn = false;

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
