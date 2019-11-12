#ifndef MYTHMAINWINDOW_INT
#define MYTHMAINWINDOW_INT

#include <QWidget>
class MythMainWindow;
class MythMainWindowPrivate;

#ifdef USING_OPENGL
#include "mythrenderopengl.h"

class MythPainterWindowGL : public QWidget
{
    Q_OBJECT

  public:
    MythPainterWindowGL(MythMainWindow *win, MythMainWindowPrivate *priv,
                        MythRenderOpenGL *rend);
    ~MythPainterWindowGL() override;
    QPaintEngine *paintEngine() const override;

    // QWidget
    void paintEvent(QPaintEvent *e) override;

    MythMainWindow *m_parent;
    MythMainWindowPrivate *d;
    MythRenderOpenGL *m_render;
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

    MythMainWindow *m_parent;
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

    MythMainWindow *m_parent;
    MythMainWindowPrivate *d;
};

#endif
