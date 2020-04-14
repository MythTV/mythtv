// MythTV
#include "mythmainwindow.h"
#include "mythpainterwindowqt.h"

MythPainterWindowQt::MythPainterWindowQt(MythMainWindow *MainWin)
  : MythPainterWindow(MainWin),
    m_parent(MainWin)
{
    setAttribute(Qt::WA_NoSystemBackground);
}

void MythPainterWindowQt::paintEvent(QPaintEvent *Event)
{
    m_parent->drawScreen(Event);
}
