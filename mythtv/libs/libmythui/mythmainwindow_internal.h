#ifndef MYTHMAINWINDOW_INT
#define MYTHMAINWINDOW_INT

#include <QWidget>
class MythRender;
class MythMainWindow;
class MythMainWindowPrivate;

#ifdef USING_OPENGL
#include "mythrenderopengl.h"

class MythPainterWindow : public QWidget
{
  public:
    MythPainterWindow(MythMainWindow *MainWin);
    MythRender* GetRenderDevice(void) { return m_render; }
    bool        RenderIsShared (void) { return m_render && m_render->IsShared(); }

  protected:
    MythRender* m_render { nullptr };
};

class MythPainterWindowGL : public MythPainterWindow
{
    Q_OBJECT

  public:
    MythPainterWindowGL(MythMainWindow *MainWin, MythMainWindowPrivate *MainWinPriv);
    ~MythPainterWindowGL() override;
    bool IsValid(void);
    QPaintEngine *paintEngine() const override;

    // QWidget
    void paintEvent(QPaintEvent *e) override;

    MythMainWindow *m_parent { nullptr };
    MythMainWindowPrivate *d { nullptr };
    bool m_valid { false };
};
#endif

#ifdef _WIN32
class MythPainterWindowD3D9 : public MythPainterWindow
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

class MythPainterWindowQt : public MythPainterWindow
{
    Q_OBJECT

  public:
    MythPainterWindowQt(MythMainWindow *MainWin, MythMainWindowPrivate *MainWinPriv);

    // QWidget
    void paintEvent(QPaintEvent *e) override;

    MythMainWindow *m_parent;
    MythMainWindowPrivate *d;
};

#endif
