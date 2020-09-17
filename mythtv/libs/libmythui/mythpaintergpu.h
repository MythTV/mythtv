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
    MythPainterGPU(QWidget *Parent);
   ~MythPainterGPU() override;

    void SetMaster(bool Master);

  public slots:
    void CurrentDPIChanged(qreal DPI);

  protected:
    QWidget*     m_widget       { nullptr };
    bool         m_master       { true    };
    qreal        m_pixelRatio   { 1.0     };
    MythDisplay* m_display      { nullptr };
    bool         m_usingHighDPI { false   };
    QSize        m_lastSize;
};

#endif
