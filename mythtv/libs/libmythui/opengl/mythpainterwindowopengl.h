#ifndef MYTHPAINTERWINDOWOPENGL_H
#define MYTHPAINTERWINDOWOPENGL_H

// MythTV
#include "mythpainterwindow.h"
#include "mythrenderopengl.h"

class MythMainWindow;

class MythPainterWindowOpenGL : public MythPainterWindow
{
    Q_OBJECT

  public:
    MythPainterWindowOpenGL(MythMainWindow *MainWin);
   ~MythPainterWindowOpenGL() override;

    bool          IsValid    (void) const;
    QPaintEngine* paintEngine(void) const override;
    void          paintEvent (QPaintEvent *PaintEvent) override;

    MythMainWindow *m_parent { nullptr };
    bool m_valid             { false   };
};

#endif
