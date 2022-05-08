// MythTV
#include "libmythbase/mythlogging.h"
#include "libmythui/mythmainwindow.h"
#ifdef USING_OPENGL
#include "libmythui/opengl/mythrenderopengl.h"
#endif
#include "videovisualgoom.h"

// Goom
#include "goom/goom_tools.h"
#include "goom/goom_core.h"

VideoVisualGoom::VideoVisualGoom(AudioPlayer* Audio, MythRender* Render, bool HD)
  : VideoVisual(Audio, Render),
    m_hd(HD)
{
    int max_width  = m_hd ? 1200 : 600;
    int max_height = m_hd ? 800  : 400;
    MythMainWindow* mw = GetMythMainWindow();
    QSize sz = mw ? mw->GetUIScreenRect().size() : QSize(600, 400);
    int width  = (sz.width() > max_width)   ? max_width  : sz.width();
    int height = (sz.height() > max_height) ? max_height : sz.height();
    m_area = QRect(0, 0, width, height);
    goom_init(static_cast<guint32>(width), static_cast<guint32>(height), 0);
    LOG(VB_GENERAL, LOG_INFO, QString("Initialised Goom (%1x%2)").arg(width).arg(height));
}

VideoVisualGoom::~VideoVisualGoom()
{
#ifdef USING_OPENGL
    if (m_glSurface && m_render && (m_render->Type() == kRenderOpenGL))
    {
        auto * glrender = dynamic_cast<MythRenderOpenGL*>(m_render);
        if (glrender)
            glrender->DeleteTexture(m_glSurface);
        m_glSurface = nullptr;
    }
#endif

    goom_close();
}

void VideoVisualGoom::Draw(const QRect Area, MythPainter* /*Painter*/, QPaintDevice* /*Device*/)
{
    if (m_disabled || !m_render || Area.isEmpty())
        return;

    QMutexLocker lock(mutex());
    unsigned int* last = m_buffer;
    VisualNode* node = GetNode();
    if (node)
    {
        size_t numSamps = 512;
        if (node->m_length < 512)
            numSamps = static_cast<size_t>(node->m_length);

        GoomDualData data {};
        for (size_t i = 0; i < numSamps; i++)
        {
            data[0][i] = node->m_left[i];
            data[1][i] = node->m_right ? node->m_right[i] : data[0][i];
        }

        m_buffer = goom_update(data, 0);
    }

#ifdef USING_OPENGL
    if ((m_render->Type() == kRenderOpenGL))
    {
        auto * glrender = dynamic_cast<MythRenderOpenGL*>(m_render);
        if (glrender && m_buffer)
        {
            glrender->makeCurrent();

            if (!m_glSurface)
            {
                QImage image(m_area.size(), QImage::Format_ARGB32);
                m_glSurface = glrender->CreateTextureFromQImage(&image);
            }

            if (m_glSurface)
            {
                m_glSurface->m_crop = false;
                if (m_buffer != last)
                    m_glSurface->m_texture->setData(m_glSurface->m_pixelFormat, m_glSurface->m_pixelType,
						    reinterpret_cast<const uint8_t *>(m_buffer));
                // goom doesn't render properly due to changes in video alpha blending
                // so turn blend off
                glrender->SetBlend(false);
                std::vector<MythGLTexture*> surfaces {m_glSurface};
                glrender->DrawBitmap(surfaces, nullptr, m_area, Area, nullptr, 0);
                glrender->SetBlend(true);
            }
            glrender->doneCurrent();
        }
        return;
    }
#endif
}

static class VideoVisualGoomFactory : public VideoVisualFactory
{
  public:
    const QString& name(void) const override
    {
        static QString s_name(GOOM_NAME);
        return s_name;
    }

    VideoVisual* Create(AudioPlayer* Audio, MythRender* Render) const override
    {
        return new VideoVisualGoom(Audio, Render, false);
    }

    bool SupportedRenderer(RenderType Type) override;
} VideoVisualGoomFactory;

bool VideoVisualGoomFactory::SupportedRenderer(RenderType Type)
{
    return (Type == kRenderOpenGL);
}

static class VideoVisualGoomHDFactory : public VideoVisualFactory
{
  public:
    const QString& name(void) const override
    {
        static QString s_name(GOOMHD_NAME);
        return s_name;
    }

    VideoVisual* Create(AudioPlayer* Audio, MythRender *Render) const override
    {
        return new VideoVisualGoom(Audio, Render, true);
    }

    bool SupportedRenderer(RenderType Type) override;
} VideoVisualGoomHDFactory;

bool VideoVisualGoomHDFactory::SupportedRenderer(RenderType Type)
{
    return (Type == kRenderOpenGL);
}
