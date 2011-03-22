#include <QPen>
#include "videovisualcircles.h"

VideoVisualCircles::VideoVisualCircles(AudioPlayer *audio, MythRender *render)
  : VideoVisualSpectrum(audio, render)
{
    m_numSamples = 32;
}

void VideoVisualCircles::DrawPriv(MythPainter *painter, QPaintDevice* device)
{
    if (!painter)
        return;

    static const QBrush nobrush(Qt::NoBrush);
    int red = 0, green = 200;
    QPen pen(QColor(red, green, 0, 255));
    int count = m_scale.range();
    int incr = 200 / count;
    int rad = m_range;
    QRect circ(m_area.x() + m_area.width() / 2, m_area.y() + m_area.height() / 2,
               rad, rad);
    painter->Begin(device);
    for (int i = 0; i < count; i++, rad += m_range, red += incr, green -= incr)
    {
        double mag = abs((m_magnitudes[i] + m_magnitudes[i + count]) / 2.0);
        if (mag > 1.0)
        {
            pen.setWidth((int)mag);
            painter->DrawRoundRect(circ, rad, nobrush, pen, 200);
        }
        circ.adjust(-m_range, -m_range, m_range, m_range);
        pen.setColor(QColor(red, green, 0, 255));
    }
    painter->End();
}

bool VideoVisualCircles::InitialisePriv(void)
{
    m_range = (m_area.height() / 2) / (m_scale.range() -10);
    m_scaleFactor = 10.0;
    m_falloff = 1.0;

    VERBOSE(VB_GENERAL, DESC + QString("Initialised Circles with %1 circles.")
        .arg(m_scale.range()));
    return true;
}
