#include <algorithm>

// MythTV
#include "libmythbase/mythchrono.h"
#include "videovisualmonoscope.h"

#ifdef USING_OPENGL
#include "visualisations/opengl/mythvisualmonoscopeopengl.h"
#endif
#ifdef USING_VULKAN
#include "visualisations/vulkan/mythvisualmonoscopevulkan.h"
#endif

VideoVisualMonoScope::VideoVisualMonoScope(AudioPlayer* Audio, MythRender* Render, bool Fade)
  : VideoVisual(Audio, Render),
    m_fade(Fade)
{
}

QString VideoVisualMonoScope::Name()
{
    return m_fade ? FADE_NAME : SIMPLE_NAME;
}

void VideoVisualMonoScope::InitCommon(QRect Area)
{
    m_hue = 0.0;
    m_area = Area;
    m_rate = 1.0;
    m_lastTime = nowAsDuration<std::chrono::milliseconds>();
    m_lineWidth = std::max(1.0F, m_area.height() * 0.004F);
}

bool VideoVisualMonoScope::UpdateVertices(float* Buffer)
{
    if (!Buffer)
        return false;

    QMutexLocker locker(mutex());
    auto * node = GetNode();

    if (m_area.isEmpty() || !node)
        return false;

    float y = (static_cast<float>(m_area.height()) / 2.0F) + m_area.top();
    float x = m_area.left();
    float xstep = static_cast<float>(m_area.width()) / (NUM_SAMPLES - 1);

    double index = 0;
    double const step = static_cast<double>(node->m_length) / NUM_SAMPLES;
    for (size_t i = 0; i < NUM_SAMPLES; i++)
    {
        auto indexTo = static_cast<long>(index + step);
        if (indexTo == static_cast<long>(index))
            indexTo = static_cast<long>(index + 1);

        double value = 0.0;
        for (auto s = static_cast<long>(index); s < indexTo && s < node->m_length; s++)
        {
            double temp = (static_cast<double>(node->m_left[s]) +
                          (node->m_right ? static_cast<double>(node->m_right[s]) : 0.0) *
                          (static_cast<double>(m_area.height())) ) / 65536.0;
            value = temp > 0.0 ? std::max(temp, value) : std::min(temp, value);
        }

        index += step;
        Buffer[i * 2] = x;
        Buffer[(i * 2) + 1] = y + static_cast<float>(value);
        x += xstep;
    }
    return true;
}

void VideoVisualMonoScope::UpdateTime()
{
    // try and give a similar rate of transitions for different playback speeds
    auto timenow = nowAsDuration<std::chrono::milliseconds>();
    m_rate = (timenow - m_lastTime).count();
    m_lastTime = timenow;
    m_hue += m_rate / 7200.0F;
    if (m_hue > 1.0F)
        m_hue -= static_cast<uint>(m_hue);
}

static class VideoVisualMonoScopeFactory : public VideoVisualFactory
{
  public:
    const QString& name() const override
    {
        static QString s_name(FADE_NAME);
        return s_name;
    }

    VideoVisual* Create(AudioPlayer* Audio, MythRender* Render) const override
    {
#ifdef USING_OPENGL
        auto * render1 = dynamic_cast<MythRenderOpenGL*>(Render);
        if (render1)
            return new MythVisualMonoScopeOpenGL(Audio, Render, true);
#endif
#ifdef USING_VULKAN
        auto * render2 = dynamic_cast<MythRenderVulkan*>(Render);
        if (render2)
            return new MythVisualMonoScopeVulkan(Audio, Render, true);
#endif
        return nullptr;
    }

    bool SupportedRenderer(RenderType Type) override;
} VideoVisualMonoScopeFactory;

bool VideoVisualMonoScopeFactory::SupportedRenderer(RenderType Type)
{
    return ((Type == kRenderOpenGL) || (Type == kRenderVulkan));
}

static class VideoVisualSimpleScopeFactory : public VideoVisualFactory
{
  public:
    const QString& name() const override
    {
        static QString s_name(SIMPLE_NAME);
        return s_name;
    }

    VideoVisual* Create(AudioPlayer* Audio, MythRender* Render) const override
    {
#ifdef USING_OPENGL
        auto * render1 = dynamic_cast<MythRenderOpenGL*>(Render);
        if (render1)
            return new MythVisualMonoScopeOpenGL(Audio, Render, false);
#endif
#ifdef USING_VULKAN
        auto * render2 = dynamic_cast<MythRenderVulkan*>(Render);
        if (render2)
            return new MythVisualMonoScopeVulkan(Audio, Render, false);
#endif
        return nullptr;
    }

    bool SupportedRenderer(RenderType Type) override;
} VideoVisualSimpleScopeFactory;

bool VideoVisualSimpleScopeFactory::SupportedRenderer(RenderType Type)
{
    return ((Type == kRenderOpenGL) || (Type == kRenderVulkan));
}
