// MythTV
#include "mythmainwindow.h"
#include "mythpainterwindow.h"

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
