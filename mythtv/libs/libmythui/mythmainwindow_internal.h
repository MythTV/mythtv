#ifndef MYTHMAINWINDOW_INT
#define MYTHMAINWINDOW_INT

#include <QWidget>

class MythMainWindow;
class MythMainWindowPrivate;

#ifdef USE_OPENGL_PAINTER
#include <QGLWidget>
#include "mythrender_opengl.h"

class MythPainterWindowGL : public QGLWidget
{
    Q_OBJECT

  public:
    MythPainterWindowGL(MythMainWindow *win, MythMainWindowPrivate *priv,
                        MythRenderOpenGL *rend);

    void paintEvent(QPaintEvent *e);

    MythMainWindow *parent;
    MythMainWindowPrivate *d;
    MythRenderOpenGL *render;
};
#endif

#ifdef USING_VDPAU

class MythPainterWindowVDPAU : public QGLWidget
{
    Q_OBJECT

  public:
    MythPainterWindowVDPAU(MythMainWindow *win, MythMainWindowPrivate *priv);

    void paintEvent(QPaintEvent *e);

    MythMainWindow *parent;
    MythMainWindowPrivate *d;
};
#endif

#ifdef USING_MINGW
// FIXME - this only really needs a QWidget but the background overpaints the
//         main window (setAutoFillBackground(false) does not seem to help)
class MythPainterWindowD3D9 : public QGLWidget
{
    Q_OBJECT

  public:
    MythPainterWindowD3D9(MythMainWindow *win, MythMainWindowPrivate *priv);

    void paintEvent(QPaintEvent *e);

    MythMainWindow *parent;
    MythMainWindowPrivate *d;
};
#endif

class MythPainterWindowQt : public QWidget
{
    Q_OBJECT

  public:
    MythPainterWindowQt(MythMainWindow *win, MythMainWindowPrivate *priv);

    void paintEvent(QPaintEvent *e);

    MythMainWindow *parent;
    MythMainWindowPrivate *d;
};

#endif
