#ifndef MYTHPAINTERWINDOW_H
#define MYTHPAINTERWINDOW_H

// Qt
#include <QWidget>

// MythTV
#include "mythrender_base.h"

class MythMainWindow;
class MythPainter;

class MythPainterWindow : public QWidget
{
  public:
    static QString CreatePainters(MythMainWindow* MainWindow,
                                  MythPainterWindow*& PaintWin,
                                  MythPainter*& Painter);
    static void    DestroyPainters(MythPainterWindow*& PaintWin,
                                   MythPainter*& Painter);

    MythRender* GetRenderDevice(void);
    bool        RenderIsShared (void);

  protected:
    MythPainterWindow(MythMainWindow *MainWin);
   ~MythPainterWindow() override = default;

    MythRender* m_render { nullptr };

  private:
    Q_DISABLE_COPY(MythPainterWindow)
};

#endif // MYTHPAINTERWINDOW_H
