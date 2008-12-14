#ifndef MYTHMAINWINDOW_INT
#define MYTHMAINWINDOW_INT

#include <QWidget>

class MythMainWindow;
class MythMainWindowPrivate;

#ifdef USE_OPENGL_PAINTER
#include <QGLWidget>

class MythPainterWindowGL : public QGLWidget
{
    Q_OBJECT

  public:
    MythPainterWindowGL(MythMainWindow *win, MythMainWindowPrivate *priv);

    void paintEvent(QPaintEvent *e);

    MythMainWindow *parent;
    MythMainWindowPrivate *d;
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
