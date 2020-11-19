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
#if (QT_VERSION >= QT_VERSION_CHECK(5,12,0)) && (QT_VERSION < QT_VERSION_CHECK(5,15,0))
#define USING_WAYLAND_EXPOSE_HACK 1
#endif

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

  protected:
    explicit MythPainterWindow(MythMainWindow* MainWin);
   ~MythPainterWindow() override;

    MythRender* m_render { nullptr };

  private:
    Q_DISABLE_COPY(MythPainterWindow)


#ifdef USING_WAYLAND_EXPOSE_HACK
  protected slots:
    void CheckWindowIsExposed();

  private:
    QTimer* m_exposureCheckTimer { nullptr };
#endif
#ifdef USING_WAYLANDEXTRAS
    MythWaylandDevice* m_waylandDev { nullptr };
#endif
};

#endif
