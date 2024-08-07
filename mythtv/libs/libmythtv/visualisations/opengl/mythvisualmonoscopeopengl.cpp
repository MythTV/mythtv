// MythTV
#include "libmythui/opengl/mythrenderopenglshaders.h"
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
    for (auto & vbo : m_vbos)
        delete vbo.first;
    m_openglShader = nullptr;
    m_vbos.clear();
    m_vertices.clear();
}

void MythVisualMonoScopeOpenGL::Draw(const QRect Area, MythPainter* /*Painter*/, QPaintDevice* /*Device*/)
{
    MythRenderOpenGL* render = Initialise(Area);
    if (!render)
        return;

    UpdateTime();

    render->makeCurrent();

    // Rotate the vertex buffers and state
    if (m_fade)
    {
        // fade away...
        float fade = 1.0F - (m_rate / 150.0F);
        float zoom = 1.0F - (m_rate / 4000.0F);
        for (auto & state : m_vbos)
        {
            state.second[1] *= fade;
            state.second[2] *= zoom;
        }

        // drop oldest
        auto vertex = m_vbos.front();
        m_vbos.pop_front();
        m_vbos.append(vertex);
    }

    // Update the newest vertex buffer
    auto & vbo = m_vbos.back();
    vbo.second[0] = m_hue;
    vbo.second[1] = 1.0;
    vbo.second[2] = 1.0;

    bool updated = false;
    vbo.first->bind();
    if (m_bufferMaps)
    {
        void* buffer = vbo.first->map(QOpenGLBuffer::WriteOnly);
        updated = UpdateVertices(static_cast<float*>(buffer));
        vbo.first->unmap();
    }
    else
    {
        updated = UpdateVertices(m_vertices.data());
        vbo.first->write(0, m_vertices.data(), static_cast<int>(m_vertices.size() * sizeof(GLfloat)));
    }

    if (!updated)
    {
        // this is generally when the visualiser is embedding and hidden. We must
        // still drain the buffers but don't actually draw
        render->doneCurrent();
        return;
    }

    // Common calls
    render->glEnableVertexAttribArray(0);
    QPointF center { m_area.left() + (static_cast<qreal>(m_area.width()) / 2),
                     m_area.top() + (static_cast<qreal>(m_area.height()) / 2) };

    // Draw lines
    for (auto & vertex : m_vbos)
    {
        vertex.first->bind();
        render->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

        if (m_fade)
        {
            UIEffects fx;
            fx.m_vzoom = vertex.second[2];
            fx.m_hzoom = vertex.second[2];
            render->PushTransformation(fx, center);
        }

        render->SetShaderProjection(m_openglShader);
        auto color = QColor::fromHsvF(static_cast<qreal>(vertex.second[0]), 1.0, 1.0);
        render->glVertexAttrib4f(1, static_cast<GLfloat>(color.redF()),
                                    static_cast<GLfloat>(color.greenF()),
                                    static_cast<GLfloat>(color.blueF()),
                                    vertex.second[1]);
        render->glLineWidth(std::clamp(m_lineWidth * vertex.second[2], 1.0F, m_maxLineWidth));
        render->glDrawArrays(GL_LINE_STRIP, 0, NUM_SAMPLES);

        if (m_fade)
            render->PopTransformation();
    }

    // Cleanup
    render->glLineWidth(1);
    QOpenGLBuffer::release(QOpenGLBuffer::VertexBuffer);
    render->glDisableVertexAttribArray(0);
    render->doneCurrent();
}

MythRenderOpenGL* MythVisualMonoScopeOpenGL::Initialise(QRect Area)
{
    auto * render = dynamic_cast<MythRenderOpenGL*>(m_render);
    if (!render)
        return nullptr;

    if ((Area == m_area) && m_openglShader && !m_vbos.empty())
        return render;

    TearDown();
    InitCommon(Area);

    OpenGLLocker locker(render);

    // Check line width constraints
    std::array<GLfloat,2> ranges { 1.0 };
    render->glGetFloatv(GL_ALIASED_LINE_WIDTH_RANGE, ranges.data());
    m_maxLineWidth = ranges[1];

    // Use direct updates if available, otherwise create an intermediate buffer
    m_bufferMaps = (render->GetExtraFeatures() & kGLBufferMap) == kGLBufferMap;
    if (m_bufferMaps)
        m_vertices.clear();
    else
        m_vertices.resize(NUM_SAMPLES * 2);

    if (!m_openglShader)
        m_openglShader = render->CreateShaderProgram(kSimpleVertexShader, kSimpleFragmentShader);

    int size = m_fade ? 8 : 1;
    while (m_vbos.size() < size)
        m_vbos.push_back({render->CreateVBO(NUM_SAMPLES * 2 * sizeof(GLfloat), false), {}});

    if (m_openglShader && m_vbos.size() == size)
        return render;
    return nullptr;
}

