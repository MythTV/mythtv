// MythTV
#include "libmythbase/mythlogging.h"
#include "opengl/mythopenglcomputeshaders.h"
#include "opengl/mythopengltonemap.h"

#define LOC QString("Tonemap: ")

#ifndef GL_SHADER_STORAGE_BUFFER
#define GL_SHADER_STORAGE_BUFFER          0x90D2
#endif
#ifndef GL_ALL_BARRIER_BITS
#define GL_ALL_BARRIER_BITS               0xFFFFFFFF
#endif
#ifndef GL_SHADER_IMAGE_ACCESS_BARRIER_BIT
#define GL_SHADER_IMAGE_ACCESS_BARRIER_BIT 0x00000020
#endif
#ifndef GL_STREAM_COPY
#define GL_STREAM_COPY                    0x88E2
#endif
#ifndef GL_WRITE_ONLY
#define GL_WRITE_ONLY                     0x88B9
#endif

MythOpenGLTonemap::MythOpenGLTonemap(MythRenderOpenGL* Render, MythVideoColourSpace* ColourSpace)
{
    if (Render)
    {
        Render->IncrRef();
        m_render = Render;
        m_extra = Render->extraFunctions();
    }

    if (ColourSpace)
    {
        ColourSpace->IncrRef();
        m_colourSpace = ColourSpace;
        connect(m_colourSpace, &MythVideoColourSpace::Updated, this, &MythOpenGLTonemap::UpdateColourSpace);
    }
}

MythOpenGLTonemap::~MythOpenGLTonemap()
{
    if (m_render)
    {
        m_render->makeCurrent();
        if (m_storageBuffer)
            m_render->glDeleteBuffers(1, &m_storageBuffer);
        delete m_shader;
        delete m_texture;
        m_render->doneCurrent();
        m_render->DecrRef();
    }

    if (m_colourSpace)
        m_colourSpace->DecrRef();
}

void MythOpenGLTonemap::UpdateColourSpace([[maybe_unused]] bool PrimariesChanged)
{
    OpenGLLocker locker(m_render);
    if (m_shader)
    {
        m_render->SetShaderProgramParams(m_shader, *m_colourSpace, "m_colourMatrix");
        m_render->SetShaderProgramParams(m_shader, m_colourSpace->GetPrimaryMatrix(), "m_primaryMatrix");
    }
}

MythVideoTextureOpenGL* MythOpenGLTonemap::GetTexture()
{
    return m_texture;
}

MythVideoTextureOpenGL* MythOpenGLTonemap::Map(std::vector<MythVideoTextureOpenGL*>& Inputs, QSize DisplaySize)
{
    size_t size = Inputs.size();
    if (!size || !m_render || !m_extra)
        return nullptr;

    OpenGLLocker locker(m_render);
    bool changed = m_outputSize != DisplaySize;

    if (!m_texture || changed)
        if (!CreateTexture(DisplaySize))
            return nullptr;

    changed |= (m_inputCount != size) || (m_inputType != Inputs[0]->m_frameFormat) ||
               (m_inputSize != Inputs[0]->m_size);

    if (!m_shader || changed)
        if (!CreateShader(size, Inputs[0]->m_frameFormat, Inputs[0]->m_size))
            return nullptr;

    if (!m_storageBuffer)
    {
        m_render->glGenBuffers(1, &m_storageBuffer);
        if (!m_storageBuffer)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to allocate storage buffer");
            return nullptr;
        }
        m_render->glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_storageBuffer);
        // Data structure passed to kernel. NOLINTNEXTLINE(modernize-avoid-c-arrays)
        struct dummy { float a[2] {0.0F}; uint32_t b {0}; uint32_t c {0}; uint32_t d {0}; } buffer;
        m_render->glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(dummy), &buffer, GL_STREAM_COPY);
        m_render->glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    m_render->EnableShaderProgram(m_shader);
    for (size_t i = 0; i < size; ++i)
    {
        m_render->ActiveTexture(GL_TEXTURE0 + static_cast<GLuint>(i));
        if (Inputs[i]->m_texture)
            Inputs[i]->m_texture->bind();
        else
            m_render->glBindTexture(Inputs[i]->m_target, Inputs[i]->m_textureId);
    }

    m_extra->glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_storageBuffer);
    m_extra->glMemoryBarrier(GL_ALL_BARRIER_BITS);
    m_extra->glBindImageTexture(0, m_texture->m_textureId, 0, GL_FALSE, 0, GL_WRITE_ONLY, QOpenGLTexture::RGBA16F);
    m_extra->glDispatchCompute((static_cast<GLuint>(m_texture->m_size.width()) + 1) >> 3,
                               (static_cast<GLuint>(m_texture->m_size.height()) + 1) >> 3, 1);
    m_extra->glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    m_extra->glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
    return m_texture;
}

bool MythOpenGLTonemap::CreateShader(size_t InputSize, VideoFrameType Type, QSize Size)
{
    delete m_shader;
    m_shader = nullptr;
    m_inputSize = Size;

    QString source = (m_render->isOpenGLES() ? "#version 310 es\n" : "#version 430\n");
    if (MythVideoFrame::FormatIs420(Type) || MythVideoFrame::FormatIs422(Type) || MythVideoFrame::FormatIs444(Type))
        source.append("#define YV12\n");
    if (m_render->isOpenGLES() && MythVideoFrame::ColorDepth(Type) > 8)
        source.append("#define UNSIGNED\n");
    source.append(GLSL430Tonemap);

    m_shader = m_render->CreateComputeShader(source);
    if (m_shader)
    {
        m_inputCount = InputSize;
        m_inputType  = Type;
        m_render->EnableShaderProgram(m_shader);
        for (size_t i = 0; i < InputSize; ++i)
            m_shader->setUniformValue(QString("texture%1").arg(i).toLatin1().constData(), static_cast<GLuint>(i));
        LOG(VB_GENERAL, LOG_INFO, QString("Created tonemapping compute shader (%1 inputs)")
            .arg(InputSize));
        UpdateColourSpace(false);
        return true;
    }

    return false;
}

bool MythOpenGLTonemap::CreateTexture(QSize Size)
{
    delete m_texture;
    m_texture = nullptr;
    m_outputSize = Size;
    GLuint textureid = 0;
    m_render->glGenTextures(1, &textureid);
    if (!textureid)
        return false;

    m_texture = new MythVideoTextureOpenGL(textureid);
    if (!m_texture)
        return false;

    m_texture->m_frameType   = FMT_RGBA32;
    m_texture->m_frameFormat = FMT_RGBA32;
    m_texture->m_target      = QOpenGLTexture::Target2D;
    m_texture->m_size        = Size;
    m_texture->m_totalSize   = m_render->GetTextureSize(Size, m_texture->m_target != QOpenGLTexture::TargetRectangle);
    m_texture->m_vbo         = m_render->CreateVBO(static_cast<int>(MythRenderOpenGL::kVertexSize));
    m_extra->glBindTexture(m_texture->m_target, m_texture->m_textureId);
    m_extra->glTexStorage2D(m_texture->m_target, 1, QOpenGLTexture::RGBA16F,
                            static_cast<GLsizei>(Size.width()), static_cast<GLsizei>(Size.height()));
    m_render->SetTextureFilters(m_texture, QOpenGLTexture::Linear);
    return true;
}
