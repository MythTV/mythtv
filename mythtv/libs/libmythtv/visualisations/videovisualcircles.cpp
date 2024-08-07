// Qt
#include <QPen>

// MythTV
#include "videovisualcircles.h"

#ifdef USING_VULKAN
#include "visualisations/vulkan/mythvisualcirclesvulkan.h"
#endif

VideoVisualCircles::VideoVisualCircles(AudioPlayer* Audio, MythRender* Render)
  : VideoVisualSpectrum(Audio, Render)
{
    m_numSamples = 32;
}

void VideoVisualCircles::DrawPriv(MythPainter* Painter, QPaintDevice* Device)
{
    if (!Painter)
        return;

    static const QBrush kNobrush(Qt::NoBrush);
    int red = 0;
    int green = 200;
    QPen pen(QColor(red, green, 0, 255));
    int count = m_scale.range();
    int incr = 200 / count;
    int rad = static_cast<int>(m_range);
    QRect circ(m_area.x() + (m_area.width() / 2), m_area.y() + (m_area.height() / 2),
               rad, rad);
    Painter->Begin(Device);
    for (int i = 0; i < count; i++, rad += m_range, red += incr, green -= incr)
    {
        double mag = qAbs((m_magnitudes[i] + m_magnitudes[i + count]) / 2.0);
        if (mag > 1.0)
        {
            pen.setWidth(static_cast<int>(mag));
            Painter->DrawRoundRect(circ, rad, kNobrush, pen, 200);
        }
        circ.adjust(static_cast<int>(-m_range), static_cast<int>(-m_range),
                    static_cast<int>(m_range),  static_cast<int>(m_range));
        pen.setColor(QColor(red, green, 0, 255));
    }
    Painter->End();
}

bool VideoVisualCircles::InitialisePriv()
{
    m_range = (static_cast<double>(m_area.height()) / 2.0) / (m_scale.range() - 10);
    m_scaleFactor = 10.0;
    m_falloff = 1.0;

    LOG(VB_GENERAL, LOG_INFO, DESC +
        QString("Initialised Circles with %1 circles.") .arg(m_scale.range()));
    return true;
}

static class VideoVisualCirclesFactory : public VideoVisualFactory
{
  public:
    const QString &name() const override
    {
        static QString s_name(CIRCLES_NAME);
        return s_name;
    }

    VideoVisual* Create(AudioPlayer* Audio, MythRender* Render) const override
    {
#ifdef USING_VULKAN
        auto * vulkan = dynamic_cast<MythRenderVulkan*>(Render);
        if (vulkan)
            return new MythVisualCirclesVulkan(Audio, vulkan);
#endif
        return new VideoVisualCircles(Audio, Render);
    }

    bool SupportedRenderer(RenderType Type) override;

} VideoVisualCirclesFactory;

bool VideoVisualCirclesFactory::SupportedRenderer(RenderType Type)
{
    return ((Type == kRenderOpenGL) || (Type == kRenderVulkan));
}
