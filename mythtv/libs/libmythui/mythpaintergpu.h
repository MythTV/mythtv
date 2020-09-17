#ifndef MYTHPAINTERGPU_H
#define MYTHPAINTERGPU_H

// MythTV
#include "mythuiexp.h"
#include "mythpainter.h"

class MythDisplay;

class MUI_PUBLIC MythPainterGPU : public MythPainter
{
    Q_OBJECT

  public:
    enum ViewControl
    {
        None        = 0x00,
        Viewport    = 0x01,
        Framebuffer = 0x02
    };
    Q_DECLARE_FLAGS(ViewControls, ViewControl)

    MythPainterGPU(QWidget *Parent);
   ~MythPainterGPU() override;

    void SetViewControl    (ViewControls Control);

  public slots:
    void CurrentDPIChanged (qreal DPI);

  protected:
    QWidget*       m_widget       { nullptr };
    ViewControls   m_viewControl  { Viewport | Framebuffer };
    qreal          m_pixelRatio   { 1.0     };
    MythDisplay*   m_display      { nullptr };
    bool           m_usingHighDPI { false   };
    QSize          m_lastSize;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(MythPainterGPU::ViewControls)

#endif
