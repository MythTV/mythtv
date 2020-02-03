#ifndef MYTHMAINWINDOW_INT
#define MYTHMAINWINDOW_INT

// Qt
#include <QWidget>

// MythTV
#include "mythrender_base.h"

class MythMainWindow;
class MythMainWindowPrivate;

class MythPainterWindow : public QWidget
{
  public:
    MythPainterWindow(MythMainWindow *MainWin);
    MythRender* GetRenderDevice(void) { return m_render; }
    bool        RenderIsShared (void) { return m_render && m_render->IsShared(); }

  protected:
    MythRender* m_render { nullptr };
};

#ifdef USING_OPENGL
#include "mythrenderopengl.h"
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
