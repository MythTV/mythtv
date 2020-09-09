// MythTV
#include "opengl/mythrenderopenglshaders.h"
#include "mythvisualmonoscopeopengl.h"

MythVisualMonoScopeOpenGL::MythVisualMonoScopeOpenGL(AudioPlayer* Audio, MythRender* Render, bool Fade)
  : VideoVisualMonoScope(Audio, Render, Fade)
{
}

MythVisualMonoScopeOpenGL::~MythVisualMonoScopeOpenGL()
{
    MythVisualMonoScopeOpenGL::TearDown();
}

void MythVisualMonoScopeOpenGL::TearDown()
{
    auto * render = dynamic_cast<MythRenderOpenGL*>(m_render);
    if (!render)
        return;

    OpenGLLocker locker(render);
    render->DeleteShaderProgram(m_openglShader);
    delete m_vbo;
    delete m_fbos[0];
    delete m_fbos[1];
    render->DeleteTexture(m_textures[0]);
    render->DeleteTexture(m_textures[1]);
}

void MythVisualMonoScopeOpenGL::Draw(const QRect& Area, MythPainter* /*Painter*/, QPaintDevice* /*Device*/)
{
    MythRenderOpenGL* render = Initialise(Area);
    if (!render)
        return;

    UpdateTime();

    render->makeCurrent();

    auto lastfbo = static_cast<size_t>(m_currentFBO);
    auto nextfbo = static_cast<size_t>(!m_currentFBO);

    if (m_fade)
    {
        // bind the next framebuffer
        render->BindFramebuffer(m_fbos[nextfbo]);
        // Clear
        render->SetBackground(0, 0, 0, 255);
        render->SetViewPort(m_area);
        render->ClearFramebuffer();
        // Zoom out a little
        float zoom = 1.0F - (m_rate / 4000.0F);
        int width  = static_cast<int>(m_area.width() * zoom);
        int height = static_cast<int>(m_area.height() * zoom);
        auto dest = QRect((m_area.width() - width) / 2, (m_area.height() - height) / 2,
                           width, height);
        // render last framebuffer into next with a little alpha fade
        // N.B. this alpha fade, in conjunction with the clear alpha above, causes
        // us to grey out the underlying video (if rendered over video).
        render->DrawBitmap(m_textures[lastfbo], m_fbos[nextfbo], m_area, dest,
                           nullptr, static_cast<int>(255.0F - m_rate / 2));
    }

    m_vbo->bind();
    UpdateVertices(static_cast<float*>(m_vertices.data()));
    m_vbo->write(0, m_vertices.data(), static_cast<int>(m_vertices.size() * sizeof(GLfloat)));

    // Draw the scope - either to screen or to our framebuffer object
    auto color = QColor::fromHsvF(m_hue, 1.0, 1.0);

    render->glEnableVertexAttribArray(0);
    render->glVertexAttrib4f(1, static_cast<GLfloat>(color.redF()),
                                static_cast<GLfloat>(color.greenF()),
                                static_cast<GLfloat>(color.blueF()), 1.0F);
    render->SetShaderProjection(m_openglShader);
    render->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    render->glLineWidth(static_cast<int>(m_lineWidth));
    render->glDrawArrays(GL_LINE_STRIP, 0, NUM_SAMPLES);
    render->glLineWidth(1);
    QOpenGLBuffer::release(QOpenGLBuffer::VertexBuffer);
    render->glDisableVertexAttribArray(0);

    // Render and swap buffers
    if (m_fade)
    {
        render->DrawBitmap(m_textures[nextfbo], nullptr, m_area, m_area, nullptr);
        m_currentFBO = !m_currentFBO;
    }

    render->doneCurrent();
}

MythRenderOpenGL* MythVisualMonoScopeOpenGL::Initialise(const QRect& Area)
{
    auto * render = dynamic_cast<MythRenderOpenGL*>(m_render);
    if (!render)
        return nullptr;

    if (Area == m_area)
    {
        if (!m_fade && m_openglShader && m_vbo)
            return render;
        if (m_fade && m_openglShader && m_vbo && m_fbos[0] && m_fbos[1] && m_textures[0] && m_textures[1])
            return render;
    }

    InitCommon(Area);

    OpenGLLocker locker(render);

    if (!m_openglShader)
        m_openglShader = render->CreateShaderProgram(kSimpleVertexShader, kSimpleFragmentShader);
    if (!m_vbo)
        m_vbo = render->CreateVBO(NUM_SAMPLES * 2 * sizeof(GLfloat), false);

    delete m_fbos[0];
    delete m_fbos[1];
    render->DeleteTexture(m_textures[0]);
    render->DeleteTexture(m_textures[1]);
    m_fbos.fill(nullptr);
    m_textures.fill(nullptr);
    m_currentFBO = false;

    if (m_fade)
    {

        QSize size(m_area.size());
        m_fbos[0] = render->CreateFramebuffer(size);
        m_fbos[1] = render->CreateFramebuffer(size);
        render->SetBackground(0, 0, 0, 255);
        render->SetViewPort(m_area);
        if (m_fbos[0])
        {
            m_textures[0] = render->CreateFramebufferTexture(m_fbos[0]);
            render->BindFramebuffer(m_fbos[0]);
            render->ClearFramebuffer();
        }
        if (m_fbos[1])
        {
            m_textures[1] = render->CreateFramebufferTexture(m_fbos[1]);
            render->BindFramebuffer(m_fbos[1]);
            render->ClearFramebuffer();
        }
        if (m_textures[0])
            render->SetTextureFilters(m_textures[0],  QOpenGLTexture::Linear);
        if (m_textures[1])
            render->SetTextureFilters(m_textures[1], QOpenGLTexture::Linear);
        return (m_openglShader && m_vbo && m_fbos[0] && m_fbos[1] &&
                m_textures[0] && m_textures[1]) ? render : nullptr;
    }

    if (m_openglShader && m_vbo)
        return render;
    return nullptr;
}

