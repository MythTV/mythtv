#include <QtGlobal>

// MythTV
#include "libmythbase/mythlogging.h"
#include "mythdisplay.h"
#include "mythmainwindow.h"
#include "mythpaintergpu.h"

MythPainterGPU::MythPainterGPU(MythMainWindow* Parent)
  : m_parent(Parent)
{
#ifdef Q_OS_MACOS
    MythDisplay* display = m_parent->GetDisplay();
    CurrentDPIChanged(m_parent->devicePixelRatioF());
    connect(display, &MythDisplay::CurrentDPIChanged, this, &MythPainterGPU::CurrentDPIChanged);
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
