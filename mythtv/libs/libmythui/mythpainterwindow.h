#ifndef MYTHPAINTERWINDOW_H
#define MYTHPAINTERWINDOW_H

// Qt
#include <QWidget>

// MythTV
#include "mythuiexp.h"
#include "mythrender_base.h"

class MythMainWindow;
class MythPainter;

class MythPainterWindow : public QWidget
{
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

  protected:
    explicit MythPainterWindow(MythMainWindow* MainWin);
   ~MythPainterWindow() override = default;

    MythRender* m_render { nullptr };

  private:
    Q_DISABLE_COPY(MythPainterWindow)
};

#endif // MYTHPAINTERWINDOW_H
