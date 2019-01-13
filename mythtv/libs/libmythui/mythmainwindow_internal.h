#ifndef MYTHMAINWINDOW_INT
#define MYTHMAINWINDOW_INT

#include <QWidget>
class MythMainWindow;
class MythMainWindowPrivate;

#ifdef USE_OPENGL_PAINTER
#include <QOpenGLWidget>
#include "mythrender_opengl.h"

class MythPainterWindowGL : public QWidget
{
    Q_OBJECT

  public:
    MythPainterWindowGL(MythMainWindow *win, MythMainWindowPrivate *priv,
                        MythRenderOpenGL *rend);
    ~MythPainterWindowGL();
    QPaintEngine *paintEngine() const override;

    // QWidget
    void paintEvent(QPaintEvent *e) override;

    MythMainWindow *parent;
    MythMainWindowPrivate *d;
    MythRenderOpenGL *render;
};
#endif

#ifdef _WIN32
class MythPainterWindowD3D9 : public QWidget
{
    Q_OBJECT

  public:
    MythPainterWindowD3D9(MythMainWindow *win, MythMainWindowPrivate *priv);

    // QWidget
    void paintEvent(QPaintEvent *e) override;

    MythMainWindow *parent;
    MythMainWindowPrivate *d;
};
#endif

class MythPainterWindowQt : public QWidget
{
    Q_OBJECT

  public:
    MythPainterWindowQt(MythMainWindow *win, MythMainWindowPrivate *priv);

    // QWidget
    void paintEvent(QPaintEvent *e) override;

    MythMainWindow *parent;
    MythMainWindowPrivate *d;
};

#endif
