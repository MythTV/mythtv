#ifndef MYTHMAINWINDOW_INT
#define MYTHMAINWINDOW_INT

#include <QWidget>
#include <QObject>
#include <QEvent>

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
