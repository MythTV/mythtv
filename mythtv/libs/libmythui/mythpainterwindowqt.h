#ifndef MYTHPAINTERWINDOWQT_H
#define MYTHPAINTERWINDOWQT_H

// MythTV
#include "mythpainterwindow.h"

class MythMainWindowPrivate;

class MythPainterWindowQt : public MythPainterWindow
{
    Q_OBJECT

  public:
    MythPainterWindowQt(MythMainWindow *MainWin, MythMainWindowPrivate *MainWinPriv);

    void paintEvent(QPaintEvent *Event) override;

    MythMainWindow *m_parent;
    MythMainWindowPrivate *d;
};
#endif // MYTHPAINTERWINDOWQT_H
