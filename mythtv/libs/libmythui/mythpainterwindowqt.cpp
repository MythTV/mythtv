// MythTV
#include "mythmainwindowprivate.h"
#include "mythpainterwindowqt.h"

MythPainterWindowQt::MythPainterWindowQt(MythMainWindow *MainWin,
                                         MythMainWindowPrivate *MainWinPriv)
  : MythPainterWindow(MainWin),
    m_parent(MainWin),
    d(MainWinPriv)
{
    setAttribute(Qt::WA_NoSystemBackground);
}

void MythPainterWindowQt::paintEvent(QPaintEvent *Event)
{
    d->m_repaintRegion = d->m_repaintRegion.united(Event->region());
    m_parent->drawScreen();
}
