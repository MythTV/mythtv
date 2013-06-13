#include "mythlogging.h"

#include "mythmainwindow.h"

#ifdef USING_OPENGL
#include "mythrender_opengl.h"
#endif

#ifdef USING_VDPAU
#include "mythrender_vdpau.h"
#endif

extern "C" {
#include "goom/goom_tools.h"
#include "goom/goom_core.h"
}

#include "videovisualgoom.h"

VideoVisualGoom::VideoVisualGoom(AudioPlayer *audio, MythRender *render, bool hd)
  : VideoVisual(audio, render), m_buffer(NULL), m_surface(0), m_hd(hd)
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
    if (m_surface && m_render &&
       (m_render->Type() == kRenderOpenGL1 ||
        m_render->Type() == kRenderOpenGL2 ||
        m_render->Type() == kRenderOpenGL2ES))
    {
        MythRenderOpenGL *glrender =
                    static_cast<MythRenderOpenGL*>(m_render);
        if (glrender)
            glrender->DeleteTexture(m_surface);
        m_surface = 0;
    }
#endif

#ifdef USING_VDPAU
    if (m_surface && m_render &&
       (m_render->Type() == kRenderVDPAU))
    {
        MythRenderVDPAU *render =
                    static_cast<MythRenderVDPAU*>(m_render);
        if (render)
            render->DestroyBitmapSurface(m_surface);
        m_surface = 0;
    }
#endif

    goom_close();
}

void VideoVisualGoom::Draw(const QRect &area, MythPainter *painter,
                           QPaintDevice *device)
{
    if (m_disabled || !m_render || area.isEmpty())
        return;

    QMutexLocker lock(mutex());
    unsigned int* last = m_buffer;
    VisualNode *node = GetNode();
    if (node)
    {
        int numSamps = 512;
        if (node->length < 512)
            numSamps = node->length;

        signed short int data[2][512];
        int i= 0;
        for (; i < numSamps; i++)
        {
            data[0][i] = node->left[i];
            data[1][i] = node->right ? node->right[i] : data[0][i];
        }

        for (; i < 512; i++)
        {
            data[0][i] = 0;
            data[1][i] = 0;
        }

        m_buffer = goom_update(data, 0);
    }

#ifdef USING_OPENGL
    if ((m_render->Type() == kRenderOpenGL1) ||
        (m_render->Type() == kRenderOpenGL2) ||
        (m_render->Type() == kRenderOpenGL2ES))
    {
        MythRenderOpenGL *glrender =
                    static_cast<MythRenderOpenGL*>(m_render);
        if (!m_surface && glrender && m_buffer)
        {
            m_surface = glrender->CreateTexture(m_area.size(),
                                  glrender->GetFeatures() & kGLExtPBufObj, 0,
                                  GL_UNSIGNED_BYTE, GL_RGBA, GL_RGBA8,
                                  GL_LINEAR_MIPMAP_LINEAR);
        }

        if (m_surface && glrender && m_buffer)
        {
            if (m_buffer != last)
            {
                bool copy = glrender->GetFeatures() & kGLExtPBufObj;
                void* buf = glrender->GetTextureBuffer(m_surface, copy);
                if (copy)
                    memcpy(buf, m_buffer, m_area.width() * m_area.height() * 4);
                glrender->UpdateTexture(m_surface, (void*)m_buffer);
            }
            QRectF src(m_area);
            QRectF dst(area);
            glrender->DrawBitmap(&m_surface, 1, 0, &src, &dst, 0);
        }
        return;
    }
#endif

#ifdef USING_VDPAU
    if (m_render->Type() == kRenderVDPAU)
    {
        MythRenderVDPAU *render =
                    static_cast<MythRenderVDPAU*>(m_render);

        if (!m_surface && render)
            m_surface = render->CreateBitmapSurface(m_area.size());

        if (m_surface && render && m_buffer)
        {
            if (m_buffer != last)
            {
                void    *plane[1] = { m_buffer };
                uint32_t pitch[1] = { static_cast<uint32_t>(m_area.width() * 4) };
                render->UploadBitmap(m_surface, plane, pitch);
            }
            render->DrawBitmap(m_surface, 0, NULL, NULL, kVDPBlendNull, 255, 255, 255, 255);
        }
        return;
    }
#endif
}

static class VideoVisualGoomFactory : public VideoVisualFactory
{
  public:
    const QString &name(void) const
    {
        static QString name("Goom");
        return name;
    }

    VideoVisual *Create(AudioPlayer *audio,
                        MythRender  *render) const
    {
        return new VideoVisualGoom(audio, render, false);
    }

    virtual bool SupportedRenderer(RenderType type)
    {
        return (type == kRenderVDPAU   ||
                type == kRenderOpenGL1 ||
                type == kRenderOpenGL2 ||
                type == kRenderOpenGL2ES);
    }
} VideoVisualGoomFactory;

static class VideoVisualGoomHDFactory : public VideoVisualFactory
{
  public:
    const QString &name(void) const
    {
        static QString name("Goom HD");
        return name;
    }

    VideoVisual *Create(AudioPlayer *audio,
                        MythRender  *render) const
    {
        return new VideoVisualGoom(audio, render, true);
    }

    virtual bool SupportedRenderer(RenderType type)
    {
        return (type == kRenderVDPAU   ||
                type == kRenderOpenGL1 ||
                type == kRenderOpenGL2 ||
                type == kRenderOpenGL2ES);
    }
} VideoVisualGoomHDFactory;
