#ifndef MYTHPAINTERWINDOW_H
#define MYTHPAINTERWINDOW_H

// Qt
#include <QWidget>

// MythTV
#include "mythuiexp.h"
#include "mythrender_base.h"

#ifdef USING_WAYLANDEXTRAS
class MythWaylandDevice;
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
#ifdef USING_WAYLANDEXTRAS
    MythWaylandDevice* m_waylandDev { nullptr };
#endif
};

#endif
