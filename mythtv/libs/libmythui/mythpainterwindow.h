#ifndef MYTHPAINTERWINDOW_H
#define MYTHPAINTERWINDOW_H

// Qt
#include <QTimer>
#include <QWidget>

// MythTV
#include "mythuiexp.h"
#include "mythrender_base.h"

#ifdef USING_WAYLANDEXTRAS
class MythWaylandDevice;
#endif

// Not entirely sure when this started and was fixed (if it is fixed?)
// no issue with 5.11 - broken in 5.14.2, I'm guessing introduced with:-
// https://github.com/qt/qtwayland/commit/88a0246a46c30e08e9730d16cf8739773447d058
// but that was meant to be fixed in:-
// https://github.com/qt/qtwayland/commit/7451faab740ec6294159be60d8a713f5e8070c09
// Ringfence here to ensure the workaround does not live on forever.
//
// Note: There appears to be an additional, possibly related issue running Ubuntu under
// Wayland (20.10, Qt 5.14.2) and using XCB (not Wayland directly). On startup
// the UI is unresponsive and paint events are received roughly once per second.
// Forcing an expose event (al-tab away and back) fixes it - otherwise it resolves
// itself after 10-15seconds. The following define can be enabled to debug events
// received by the window
//#define DEBUG_PAINTERWIN_EVENTS

class MythMainWindow;
class MythPainter;

class MythPainterWindow : public QWidget
{
    Q_OBJECT

  public:
    static MUI_PUBLIC QString GetDefaultPainter();
    static MUI_PUBLIC QStringList GetPainters();
    static QString CreatePainters(MythMainWindow* MainWin,
                                  MythPainterWindow*& PaintWin,
                                  MythPainter*& Paint);
    static void    DestroyPainters(MythPainterWindow*& PaintWin,
                                   MythPainter*& Painter);

    MythRender* GetRenderDevice();
    bool        RenderIsShared ();
    void        resizeEvent    (QResizeEvent* /*ResizeEvent*/) override;
#if defined(DEBUG_PAINTERWIN_EVENTS)
    bool        event(QEvent* Event) override;
#endif

  protected:
    explicit MythPainterWindow(MythMainWindow* MainWin);
   ~MythPainterWindow() override;

    MythRender* m_render { nullptr };

  private:
    Q_DISABLE_COPY(MythPainterWindow)


#ifdef USING_WAYLANDEXTRAS
    MythWaylandDevice* m_waylandDev { nullptr };
#endif
};

#endif
