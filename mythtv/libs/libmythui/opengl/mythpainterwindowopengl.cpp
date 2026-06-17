#include "mythpainterwindowopengl.h"

#include <QGuiApplication>
#include <QWindow>

#include "libmythbase/mythlogging.h"
#include "mythmainwindow.h"

#define LOC QString("GLPaintWin: ")

MythPainterWindowOpenGL::MythPainterWindowOpenGL(MythMainWindow *MainWin)
  : MythPainterWindow(MainWin),
    m_parent(MainWin)
{
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_DontCreateNativeAncestors);
    // The eglfs QPA platform works without setting the surface type and
    // can only have one OpenGLSurface, which must be the top level widget
    // (which for us is currently the MythMainWindow).
    if (QGuiApplication::platformName() != "eglfs" && windowHandle() != nullptr)
    {
        windowHandle()->setSurfaceType(QWindow::OpenGLSurface);
        QWindow* window = MainWin->windowHandle();
        if (window != nullptr) {
            window->setSurfaceType(QWindow::OpenGLSurface);
        }
    }
    winId();
#ifdef Q_OS_MACOS
     // must be visible before OpenGL initialisation on OSX
    setVisible(true);
#endif
    MythRenderOpenGL *render = MythRenderOpenGL::Create(this);
    if (render)
    {
        m_render = render;
        if (render->Init() && render->IsRecommendedRenderer())
            m_valid = true;
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create MythRenderOpenGL");
    }
}

QPaintEngine *MythPainterWindowOpenGL::paintEngine(void) const
{
    return testAttribute(Qt::WA_PaintOnScreen) ? nullptr : m_parent->paintEngine();
}

MythPainterWindowOpenGL::~MythPainterWindowOpenGL()
{
    if (m_render)
        m_render->DecrRef();
}

bool MythPainterWindowOpenGL::IsValid(void) const
{
    return m_valid;
}

void MythPainterWindowOpenGL::paintEvent(QPaintEvent* /*PaintEvent*/)
{
    m_parent->drawScreen();
}
