#ifndef MYTHMAINWINDOW_INT
#define MYTHMAINWINDOW_INT

#include <QWidget>

#if defined( USE_OPENGL_PAINTER ) || defined( _WIN32 )
#  include <QGLWidget>
#endif

class MythMainWindow;
class MythMainWindowPrivate;

#ifdef USE_OPENGL_PAINTER
#include "mythrender_opengl.h"

#ifdef USE_OPENGL_QT5
#include <QWidget>
typedef QWidget MythPainterWindowWidget;
#else
#include <QGLWidget>
typedef QGLWidget MythPainterWindowWidget;
#endif
#ifdef USING_MINGW
#include <QGLWidget>
#endif

class MythPainterWindowGL : public MythPainterWindowWidget
{
    Q_OBJECT

  public:
    MythPainterWindowGL(MythMainWindow *win, MythMainWindowPrivate *priv,
                        MythRenderOpenGL *rend);
#ifdef USE_OPENGL_QT5
    ~MythPainterWindowGL();
    QPaintEngine *paintEngine() const;
#endif

    void paintEvent(QPaintEvent *e) override; // MythPainterWindowWidget aka QWidget

    MythMainWindow *m_parent;
    MythMainWindowPrivate *d;
    MythRenderOpenGL *m_render;
};
#endif

#ifdef _WIN32
// FIXME - this only really needs a QWidget but the background overpaints the
//         main window (setAutoFillBackground(false) does not seem to help)
class MythPainterWindowD3D9 : public QGLWidget
{
    Q_OBJECT

  public:
    MythPainterWindowD3D9(MythMainWindow *win, MythMainWindowPrivate *priv);

    void paintEvent(QPaintEvent *e) override; // QGLWidget

    MythMainWindow *m_parent;
    MythMainWindowPrivate *d;
};
#endif

class MythPainterWindowQt : public QWidget
{
    Q_OBJECT

  public:
    MythPainterWindowQt(MythMainWindow *win, MythMainWindowPrivate *priv);

    void paintEvent(QPaintEvent *e) override; // QWidget

    MythMainWindow *m_parent;
    MythMainWindowPrivate *d;
};

#endif
