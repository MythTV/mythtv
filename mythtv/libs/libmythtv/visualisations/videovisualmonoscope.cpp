// MythTV
#include "opengl/mythrenderopenglshaders.h"
#include "opengl/mythrenderopengl.h"
#include "videovisualmonoscope.h"

VideoVisualMonoScope::VideoVisualMonoScope(AudioPlayer *Audio, MythRender *Render, bool Fade)
  : VideoVisual(Audio, Render),
    m_fade(Fade)
{
}

VideoVisualMonoScope::~VideoVisualMonoScope()
{
    MythRenderOpenGL *render = dynamic_cast<MythRenderOpenGL*>(m_render);
    if (!render)
        return;

    OpenGLLocker locker(render);
    render->DeleteShaderProgram(m_shader);
    delete m_vbo;
    delete m_fbo[0];
    delete m_fbo[1];
    render->DeleteTexture(m_texture[0]);
    render->DeleteTexture(m_texture[1]);
}

QString VideoVisualMonoScope::Name(void)
{
    return m_fade ? "FadeScope" : "SimpleScope";
}

MythRenderOpenGL *VideoVisualMonoScope::Initialise(const QRect &Area)
{
    MythRenderOpenGL *render = dynamic_cast<MythRenderOpenGL*>(m_render);
    if (!render)
        return nullptr;

    if (Area == m_area)
    {
        if (!m_fade && m_shader && m_vbo)
            return render;
        if (m_fade && m_shader && m_vbo && m_fbo[0] && m_fbo[1] && m_texture[0] && m_texture[1])
            return render;
    }

    OpenGLLocker locker(render);

    if (!m_shader)
        m_shader = render->CreateShaderProgram(kSimpleVertexShader, kSimpleFragmentShader);
    if (!m_vbo)
        m_vbo = render->CreateVBO(NUM_SAMPLES * 2 * sizeof(GLfloat), false);

    m_hue = 0.0;
    m_area = Area;
    m_lastTime = QDateTime::currentMSecsSinceEpoch();

    delete m_fbo[0];
    delete m_fbo[1];
    render->DeleteTexture(m_texture[0]);
    render->DeleteTexture(m_texture[1]);
    m_fbo[0] = nullptr;
    m_fbo[1] = nullptr;
    m_texture[0] = nullptr;
    m_texture[1] = nullptr;
    m_currentFBO = false;

    if (m_fade)
    {

        QSize size(m_area.size());
        m_fbo[0] = render->CreateFramebuffer(size);
        m_fbo[1] = render->CreateFramebuffer(size);
        render->SetBackground(0, 0, 0, 255);
        render->SetViewPort(m_area);
        if (m_fbo[0])
        {
            m_texture[0] = render->CreateFramebufferTexture(m_fbo[0]);
            render->BindFramebuffer(m_fbo[0]);
            render->ClearFramebuffer();
        }
        if (m_fbo[1])
        {
            m_texture[1] = render->CreateFramebufferTexture(m_fbo[1]);
            render->BindFramebuffer(m_fbo[1]);
            render->ClearFramebuffer();
        }
        if (m_texture[0])
            render->SetTextureFilters(m_texture[0],  QOpenGLTexture::Linear);
        if (m_texture[1])
            render->SetTextureFilters(m_texture[1], QOpenGLTexture::Linear);
        return (m_shader && m_vbo && m_fbo[0] && m_fbo[1] &&
                m_texture[0] && m_texture[1]) ? render : nullptr;
    }

    if (m_shader && m_vbo)
        return render;
    return nullptr;
}

void VideoVisualMonoScope::Draw(const QRect &Area, MythPainter*, QPaintDevice*)
{
    if (m_disabled)
        return;

    VisualNode *node = nullptr;
    MythRenderOpenGL *render = nullptr;

    {
        QMutexLocker locker(mutex());
        node = GetNode();

        if (Area.isEmpty() || !node)
            return;
        if (!(render = Initialise(Area)))
            return;

        int y = m_area.height() / 2;
        int x = 0;
        int xstep = m_area.width() / NUM_SAMPLES + 1;

        double index = 0;
        double const step = node->m_length / NUM_SAMPLES;
        for ( int i = 0; i < NUM_SAMPLES; i++)
        {
            auto indexTo = static_cast<long>(index + step);
            if (indexTo == static_cast<long>(index))
                indexTo = static_cast<long>(index + 1);

            double value  = 0.0;
            for (auto s = static_cast<long>(index); s < indexTo && s < node->m_length; s++)
            {
                double temp = (static_cast<double>(node->m_left[s]) +
                              (node->m_right ? static_cast<double>(node->m_right[s]) : 0.0) *
                              (static_cast<double>(m_area.height())) ) / 65536.0;
                value = temp > 0.0 ? qMax(temp, value) : qMin(temp, value);
            }

            index += step;
            m_vertices[i * 2] = x;
            m_vertices[i * 2 + 1] = y + static_cast<GLfloat>(value);
            x += xstep;
        }
    }

    // try and give a similar rate of transitions for different playback speeds
    qint64 timenow = QDateTime::currentMSecsSinceEpoch();
    qreal rate = (timenow - m_lastTime);
    m_lastTime = timenow;

    render->makeCurrent();

    int lastfbo = static_cast<int>(m_currentFBO);
    int nextfbo = static_cast<int>(!m_currentFBO);

    if (m_fade)
    {
        // bind the next framebuffer
        render->BindFramebuffer(m_fbo[nextfbo]);
        // Clear
        render->SetBackground(0, 0, 0, 255);
        render->SetViewPort(m_area);
        render->ClearFramebuffer();
        // Zoom out a little
        qreal zoom = 1.0 - (rate / 4000.0);
        int width  = static_cast<int>(m_area.width() * zoom);
        int height = static_cast<int>(m_area.height() * zoom);
        QRect dest = QRect((m_area.width() - width) / 2, (m_area.height() - height) / 2,
                           width, height);
        // render last framebuffer into next with a little alpha fade
        // N.B. this alpha fade, in conjunction with the clear alpha above, causes
        // us to grey out the underlying video (if rendered over video).
        render->DrawBitmap(m_texture[lastfbo], m_fbo[nextfbo], m_area, dest,
                           nullptr, static_cast<int>(255.0 - rate / 2));
    }

    // Draw the scope - either to screen or to our framebuffer object
    QColor color = QColor::fromHsvF(m_hue, 1.0, 1.0);
    m_hue += rate / 7200.0;
    if (m_hue > 1.0)
        m_hue -= 1.0;
    render->glEnableVertexAttribArray(0);
    m_vbo->bind();
    m_vbo->write(0, m_vertices, NUM_SAMPLES * 2 * sizeof(GLfloat));
    render->glVertexAttrib4f(1, static_cast<GLfloat>(color.redF()),
                                static_cast<GLfloat>(color.greenF()),
                                static_cast<GLfloat>(color.blueF()), 1.0F);
    render->SetShaderProjection(m_shader);
    render->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    render->glLineWidth(static_cast<int>(m_area.height() * 0.004f));
    render->glDrawArrays(GL_LINE_STRIP, 0, NUM_SAMPLES);
    render->glLineWidth(1);
    QOpenGLBuffer::release(QOpenGLBuffer::VertexBuffer);
    render->glDisableVertexAttribArray(0);

    // Render and swap buffers
    if (m_fade)
    {
        render->DrawBitmap(m_texture[nextfbo], nullptr, m_area, m_area, nullptr);
        m_currentFBO = !m_currentFBO;
    }

    render->doneCurrent();
}

static class VideoVisualMonoScopeFactory : public VideoVisualFactory
{
  public:
    const QString &name(void) const override
    {
        static QString s_name("FadeScope");
        return s_name;
    }

    VideoVisual *Create(AudioPlayer *Audio, MythRender *Render) const override
    {
        return new VideoVisualMonoScope(Audio, Render, true);
    }

    bool SupportedRenderer(RenderType type) override
    {
        return (type == kRenderOpenGL);
    }
} VideoVisualMonoScopeFactory;

static class VideoVisualSimpleScopeFactory : public VideoVisualFactory
{
  public:
    const QString &name(void) const override
    {
        static QString s_name("SimpleScope");
        return s_name;
    }

    VideoVisual *Create(AudioPlayer *Audio, MythRender *Render) const override
    {
        return new VideoVisualMonoScope(Audio, Render, false);
    }

    bool SupportedRenderer(RenderType type) override
    {
        return (type == kRenderOpenGL);
    }
} VideoVisualSimpleScopeFactory;
