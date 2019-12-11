#include "mythlogging.h"

#include "mythmainwindow.h"

#ifdef USING_OPENGL
#include "opengl/mythrenderopengl.h"
#endif

extern "C" {
#include "goom/goom_tools.h"
#include "goom/goom_core.h"
}

#include "videovisualgoom.h"

VideoVisualGoom::VideoVisualGoom(AudioPlayer *audio, MythRender *render, bool hd)
  : VideoVisual(audio, render),
    m_buffer(nullptr),
    m_glSurface(nullptr),
    m_hd(hd)
{
    int max_width  = m_hd ? 1200 : 600;
    int max_height = m_hd ? 800  : 400;
    MythMainWindow *mw = GetMythMainWindow();
    QSize sz = mw ? mw->GetUIScreenRect().size() : QSize(600, 400);
    int width  = (sz.width() > max_width)   ? max_width  : sz.width();
    int height = (sz.height() > max_height) ? max_height : sz.height();
    m_area = QRect(0, 0, width, height);
    goom_init(width, height, 0);
    LOG(VB_GENERAL, LOG_INFO, QString("Initialised Goom (%1x%2)")
            .arg(width).arg(height));
}

VideoVisualGoom::~VideoVisualGoom()
{
#ifdef USING_OPENGL
    if (m_glSurface && m_render && (m_render->Type() == kRenderOpenGL))
    {
        auto *glrender = static_cast<MythRenderOpenGL*>(m_render);
        if (glrender)
            glrender->DeleteTexture(m_glSurface);
        m_glSurface = nullptr;
    }
#endif

    goom_close();
}

void VideoVisualGoom::Draw(const QRect &area, MythPainter */*painter*/,
                           QPaintDevice */*device*/)
{
    if (m_disabled || !m_render || area.isEmpty())
        return;

    QMutexLocker lock(mutex());
    unsigned int* last = m_buffer;
    VisualNode *node = GetNode();
    if (node)
    {
        int numSamps = 512;
        if (node->m_length < 512)
            numSamps = node->m_length;

        signed short int data[2][512];
        int i= 0;
        for (; i < numSamps; i++)
        {
            data[0][i] = node->m_left[i];
            data[1][i] = node->m_right ? node->m_right[i] : data[0][i];
        }

        for (; i < 512; i++)
        {
            data[0][i] = 0;
            data[1][i] = 0;
        }

        m_buffer = goom_update(data, 0);
    }

#ifdef USING_OPENGL
    if ((m_render->Type() == kRenderOpenGL))
    {
        auto *glrender = dynamic_cast<MythRenderOpenGL*>(m_render);
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
                    m_glSurface->m_texture->setData(m_glSurface->m_pixelFormat, m_glSurface->m_pixelType, m_buffer);
                // goom doesn't render properly due to changes in video alpha blending
                // so turn blend off
                glrender->SetBlend(false);
                glrender->DrawBitmap(&m_glSurface, 1, nullptr, m_area, area, nullptr, 0);
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
    const QString &name(void) const override // VideoVisualFactory
    {
        static QString s_name("Goom");
        return s_name;
    }

    VideoVisual *Create(AudioPlayer *audio,
                        MythRender  *render) const override // VideoVisualFactory
    {
        return new VideoVisualGoom(audio, render, false);
    }

    bool SupportedRenderer(RenderType type) override // VideoVisualFactory
    {
        return (type == kRenderOpenGL);
    }
} VideoVisualGoomFactory;

static class VideoVisualGoomHDFactory : public VideoVisualFactory
{
  public:
    const QString &name(void) const override // VideoVisualFactory
    {
        static QString s_name("Goom HD");
        return s_name;
    }

    VideoVisual *Create(AudioPlayer *audio,
                        MythRender  *render) const override // VideoVisualFactory
    {
        return new VideoVisualGoom(audio, render, true);
    }

    bool SupportedRenderer(RenderType type) override // VideoVisualFactory
    {
        return (type == kRenderOpenGL);
    }
} VideoVisualGoomHDFactory;
