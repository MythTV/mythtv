// MythTV
#include "mythmainwindow.h"
#include "opengl/mythpainterwindowopengl.h"

#define LOC QString("GLPaintWin: ")

MythPainterWindowOpenGL::MythPainterWindowOpenGL(MythMainWindow *MainWin)
  : MythPainterWindow(MainWin),
    m_parent(MainWin)
{
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_DontCreateNativeAncestors);
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
