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

    MythPainterWindow(MythMainWindow *MainWin);
    MythRender* GetRenderDevice(void);
    bool        RenderIsShared (void);

  protected:
    MythRender* m_render { nullptr };
};

#endif // MYTHPAINTERWINDOW_H
