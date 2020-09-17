// MythTV
#include "config.h"
#include "mythlogging.h"
#include "mythdisplay.h"
#include "mythpaintergpu.h"

MythPainterGPU::MythPainterGPU(QWidget *Parent)
  : m_widget(Parent)
{
#ifdef Q_OS_MACOS
    m_display = MythDisplay::AcquireRelease();
    if (m_widget)
    {
        CurrentDPIChanged(m_widget->devicePixelRatioF());
        connect(m_display, &MythDisplay::CurrentDPIChanged, this, &MythPainterGPU::CurrentDPIChanged);
    }
#endif
}

MythPainterGPU::~MythPainterGPU()
{
#ifdef Q_OS_MACOS
    MythDisplay::AcquireRelease(false);
#endif
}

void MythPainterGPU::SetViewControl(ViewControls Control)
{
    m_viewControl = Control;
}

void MythPainterGPU::CurrentDPIChanged(qreal DPI)
{
    m_pixelRatio = DPI;
    m_usingHighDPI = !qFuzzyCompare(m_pixelRatio, 1.0);
    LOG(VB_GENERAL, LOG_INFO, QString("High DPI scaling %1").arg(m_usingHighDPI ? "enabled" : "disabled"));
}
