// MythTV
#include "libmythui/mythmainwindow.h"
#include "mythplayeruibase.h"

#define LOC QString("PlayerBase: ")

/*! \class MythPlayerUIBase
 *
 * This is the base class for all user facing MythPlayer classes. It contains
 * references to all of the major objects that will be needed for other UI player classes.
*/
MythPlayerUIBase::MythPlayerUIBase(MythMainWindow* MainWindow, TV* Tv, PlayerContext* Context, PlayerFlags Flags)
  : MythPlayer(Context, Flags),
    m_mainWindow(MainWindow),
    m_tv(Tv),
    m_render(MainWindow->GetRenderDevice()),
    m_painter(MainWindow->GetPainter()),
    m_display(MainWindow->GetDisplay())
{
}

void MythPlayerUIBase::InitialiseState()
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "Player state ready");
}

MythRender* MythPlayerUIBase::GetRender() const
{
    return m_render;
}
