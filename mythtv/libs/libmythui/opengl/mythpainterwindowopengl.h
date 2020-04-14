#ifndef MYTHPAINTERWINDOWOPENGL_H
#define MYTHPAINTERWINDOWOPENGL_H

// MythTV
#include "mythpainterwindow.h"
#include "mythrenderopengl.h"

class MythMainWindow;
class MythMainWindowPrivate;

class MythPainterWindowOpenGL : public MythPainterWindow
{
    Q_OBJECT

  public:
    MythPainterWindowOpenGL(MythMainWindow *MainWin, MythMainWindowPrivate *MainWinPriv);
   ~MythPainterWindowOpenGL() override;

    bool          IsValid    (void) const;
    QPaintEngine* paintEngine(void) const override;
    void          paintEvent (QPaintEvent *e) override;

    MythMainWindow *m_parent { nullptr };
    MythMainWindowPrivate *d { nullptr };
    bool m_valid             { false   };
};

#endif
