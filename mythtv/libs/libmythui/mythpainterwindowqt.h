#ifndef MYTHPAINTERWINDOWQT_H
#define MYTHPAINTERWINDOWQT_H

// MythTV
#include "mythpainterwindow.h"

class MythPainterWindowQt : public MythPainterWindow
{
    Q_OBJECT

  public:
    explicit MythPainterWindowQt(MythMainWindow *MainWin);

    void paintEvent(QPaintEvent *Event) override;

  private:
    MythMainWindow *m_parent;
};
#endif // MYTHPAINTERWINDOWQT_H
