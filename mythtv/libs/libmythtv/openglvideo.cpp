// MythTV
#include "openglvideo.h"
#include "mythcontext.h"
#include "tv.h"
#include "mythrender_opengl.h"
#include "mythavutil.h"
#include "openglvideoshaders.h"

#define LOC QString("GLVid: ")
#define MAX_VIDEO_TEXTURES 10 // YV12 Kernel deinterlacer + 1

/**
 * \class OpenGLVideo
 *
 * OpenGLVideo is responsible for displaying video frames on screen using OpenGL.
 * Frames may be sourced from main or graphics memory and OpenGLVideo must
 * handle colourspace conversion, deinterlacing and scaling as required.
*/
OpenGLVideo::OpenGLVideo(MythRenderOpenGL *Render, VideoColourSpace *ColourSpace,
                         QSize VideoDim, QSize VideoDispDim,
                         QRect DisplayVisibleRect, QRect DisplayVideoRect, QRect VideoRect,
                         bool  ViewportControl, FrameType Type)
  : QObject(),
    m_valid(false),
    m_frameType(Type),
    m_render(Render),
    m_videoDispDim(VideoDispDim),
    m_videoDim(VideoDim),
    m_masterViewportSize(DisplayVisibleRect.size()),
    m_displayVisibleRect(DisplayVisibleRect),
    m_displayVideoRect(DisplayVideoRect),
    m_videoRect(VideoRect),
    m_hardwareDeinterlacer(),
    m_hardwareDeinterlacing(false),
    m_videoColourSpace(ColourSpace),
    m_viewportControl(ViewportControl),
    m_referenceTextures(),
    m_inputTextures(),
    m_frameBuffer(nullptr),
    m_frameBufferTexture(nullptr),
    m_inputTextureSize(0,0),
    m_referenceTexturesNeeded(0),
    m_features(),
    m_extraFeatures(0),
    m_copyCtx(),
    m_resizing(false),
    m_forceResize()
{
    memset(m_shaders, 0, sizeof(m_shaders));
    memset(m_shaderCost, 1, sizeof(m_shaderCost));

    if (!m_render || !m_videoColourSpace)
        return;

    OpenGLLocker ctx_lock(m_render);
    m_render->IncrRef();

    m_videoColourSpace->IncrRef();
    connect(m_videoColourSpace, &VideoColourSpace::Updated, this, &OpenGLVideo::UpdateColourSpace);
    if (kGLGPU == m_frameType)
        m_videoColourSpace->SetSupportedAttributes(kPictureAttributeSupported_None);

    // Set OpenGL feature support
    m_features      = m_render->GetFeatures();
    m_extraFeatures = m_render->GetExtraFeatures();

    // Enable/Disable certain features based on settings
    if (m_render->isOpenGLES())
    {
        if ((m_extraFeatures & kGLExtFBDiscard) && gCoreContext->GetBoolSetting("OpenGLDiscardFB", false))
            LOG(VB_GENERAL, LOG_INFO, LOC + "Enabling Framebuffer discards");
        else
            m_extraFeatures &= ~kGLExtFBDiscard;
    }

    // Create initial input texture
    MythGLTexture *texture = CreateVideoTexture(m_videoDim, m_inputTextureSize);
    if (!texture)
        return;
    m_inputTextures.push_back(texture);
    if (kGLYV12 == m_frameType)
    {
        QSize temp;
        QSize chroma(m_videoDim.width() >> 1, m_videoDim.height() >> 1);
        texture = CreateVideoTexture(chroma, temp);
        if (!texture) return;
        m_inputTextures.push_back(texture);
        texture = CreateVideoTexture(chroma, temp);
        if (!texture) return;
        m_inputTextures.push_back(texture);
    }

    // Create initial shaders - default and optionally progressive
    if (!CreateVideoShader(Default))
        return;
    if (kGLGPU != m_frameType)
        if(!CreateVideoShader(Progressive))
            return;

    UpdateColourSpace();
    UpdateShaderParameters();

    // we have a shader and a texture...
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Using '%1' for OpenGL video type").arg(TypeToString(m_frameType)));

    if (m_viewportControl)
        m_render->SetFence();

    m_forceResize = gCoreContext->GetBoolSetting("OpenGLExtraStage", false);
    m_valid = true;
}

OpenGLVideo::~OpenGLVideo()
{
    if (m_videoColourSpace)
        m_videoColourSpace->DecrRef();

    if (!m_render)
        return;

    m_render->makeCurrent();

    if (m_viewportControl)
        m_render->DeleteFence();
    if (m_frameBuffer)
        m_render->DeleteFramebuffer(m_frameBuffer);
    if (m_frameBufferTexture)
        m_render->DeleteTexture(m_frameBufferTexture);
    for (int i = 0; i < ShaderCount; ++i)
        if (m_shaders[i])
            m_render->DeleteShaderProgram(m_shaders[i]);
    DeleteTextures(&m_inputTextures);
    DeleteTextures(&m_referenceTextures);

    m_render->doneCurrent();
    m_render->DecrRef();
}

bool OpenGLVideo::IsValid(void) const
{
    return m_valid;
}

void OpenGLVideo::UpdateColourSpace(void)
{
    OpenGLLocker locker(m_render);
    for (int i = Progressive; i < ShaderCount; ++i)
        m_render->SetShaderProgramParams(m_shaders[i], *m_videoColourSpace, "m_colourMatrix");
}

void OpenGLVideo::UpdateShaderParameters(void)
{
    if (m_inputTextureSize.isEmpty())
        return;

    OpenGLLocker locker(m_render);
    GLfloat lineheight = 1.0f / m_inputTextureSize.height();
    GLfloat maxheight  = m_videoDispDim.height() / static_cast<GLfloat>(m_inputTextureSize.height());
    QVector4D parameters(lineheight,                                      /* lineheight */
                        static_cast<GLfloat>(m_inputTextureSize.width()), /* 'Y' select */
                         maxheight - lineheight,                          /* maxheight  */
                         m_inputTextureSize.height() / 2.0f               /* fieldsize  */);

    for (int i = Progressive; i < ShaderCount; ++i)
    {
        if (m_shaders[i])
        {
            m_render->EnableShaderProgram(m_shaders[i]);
            m_shaders[i]->setUniformValue("m_frameData", parameters);
        }
    }
}

void OpenGLVideo::SetMasterViewport(QSize Size)
{
    m_masterViewportSize = Size;
}

void OpenGLVideo::SetVideoRects(const QRect &DisplayVideoRect, const QRect &VideoRect)
{
    m_displayVideoRect = DisplayVideoRect;
    m_videoRect = VideoRect;
}

QString OpenGLVideo::GetDeinterlacer(void) const
{
    return m_hardwareDeinterlacer;
}

OpenGLVideo::FrameType OpenGLVideo::GetType(void) const
{
    return m_frameType;
}

QSize OpenGLVideo::GetVideoSize(void) const
{
    return m_videoDim;
}

bool OpenGLVideo::AddDeinterlacer(const QString &Deinterlacer)
{
    OpenGLLocker ctx_lock(m_render);

    if (m_hardwareDeinterlacer == Deinterlacer)
        return true;

    // Don't delete and recreate for the sake of doublerate vs singlerate
    // In the case of kernel, it also invalidates the reference textures
    bool kernel = Deinterlacer.contains("kernel");
    if (m_hardwareDeinterlacer.contains("kernel") && kernel)
        return true;
    if (m_hardwareDeinterlacer.contains("linear") && Deinterlacer.contains("linear"))
        return true;
    if (m_hardwareDeinterlacer.contains("bob") && Deinterlacer.contains("onefield"))
        return true;
    if (m_hardwareDeinterlacer.contains("onefield") && Deinterlacer.contains("bob"))
        return true;

    // delete old reference textures
    while (!m_referenceTextures.empty())
    {
        m_render->DeleteTexture(m_referenceTextures.back());
        m_referenceTextures.pop_back();
    }

    // create new deinterlacers - the old ones will be deleted
    if (!(CreateVideoShader(InterlacedBot, Deinterlacer) && CreateVideoShader(InterlacedTop, Deinterlacer)))
        return false;

    // create the correct number of reference textures
    uint refstocreate = kernel ? 2 : 0;
    m_referenceTexturesNeeded = refstocreate + 1;
    refstocreate *= (kGLYV12 == m_frameType) ? 3 : 1;
    while (m_referenceTextures.size() < refstocreate)
    {
        MythGLTexture *texture = CreateVideoTexture(m_videoDim, m_inputTextureSize);
        if (!texture) return false;
        m_referenceTextures.push_back(texture);
        if (kGLYV12 == m_frameType)
        {
            QSize temp;
            QSize chroma(m_videoDim.width() >> 1, m_videoDim.height() >> 1);
            texture = CreateVideoTexture(chroma, temp);
            if (!texture) return false;
            m_referenceTextures.push_back(CreateVideoTexture(chroma, temp));
            texture = CreateVideoTexture(chroma, temp);
            if (!texture) return false;
            m_referenceTextures.push_back(CreateVideoTexture(chroma, temp));
        }
    }

    // ensure they work correctly
    UpdateColourSpace();
    UpdateShaderParameters();
    m_hardwareDeinterlacer = Deinterlacer;
    return true;
}

/*! \brief Create the appropriate shader for the operation Type
 *
 * \note Shader cost is calculated as 1 per texture read and 2 per dependent read.
 * If there are alternative shader conditions, the worst case is used.
 * (A dependent read is defined as any texture read that does not use the exact
 * texture coordinates passed into the fragment shader)
*/
bool OpenGLVideo::CreateVideoShader(VideoShaderType Type, QString Deinterlacer)
{
    if (!m_render || !(m_features & QOpenGLFunctions::Shaders))
        return false;

    // delete the old
    if (m_shaders[Type])
        m_render->DeleteShaderProgram(m_shaders[Type]);
    m_shaders[Type] = nullptr;

    QString vertex = DefaultVertexShader;
    QString fragment;
    int cost = 1;

    if ((Default == Type) || (kGLGPU == m_frameType))
    {
        fragment = DefaultFragmentShader;
    }
    // no interlaced shaders yet
    else if ((Progressive == Type) || Interlaced == Type || Deinterlacer.isEmpty())
    {
        fragment = (kGLYV12 == m_frameType) ? YV12RGBFragmentShader : YUV2RGBFragmentShader;
        cost     = (kGLYV12 == m_frameType) ? 3 : 1;
    }
    else
    {
        uint bottom = (InterlacedBot == Type);
        if (kGLYV12 == m_frameType)
        {
            if (Deinterlacer == "openglonefield" || Deinterlacer == "openglbobdeint")
            {
                fragment = YV12RGBOneFieldFragmentShader[bottom];
                cost = 6;
            }
            else if (Deinterlacer == "opengllinearblend" || Deinterlacer == "opengldoubleratelinearblend")
            {
                fragment = YV12RGBLinearBlendFragmentShader[bottom];
                cost = 15;
            }
            else if (Deinterlacer == "openglkerneldeint" || Deinterlacer == "opengldoubleratekerneldeint")
            {
                fragment = YV12RGBKernelShader[bottom];
                cost = 45;
            }
            else
            {
                fragment = YV12RGBFragmentShader;
                cost = 3;
            }
        }
        else
        {
            if (Deinterlacer == "openglonefield" || Deinterlacer == "openglbobdeint")
            {
                fragment = OneFieldShader[bottom];
                cost = 2;
            }
            else if (Deinterlacer == "opengllinearblend" || Deinterlacer == "opengldoubleratelinearblend")
            {
                fragment = LinearBlendShader[bottom];
                cost = 5;
            }
            else if (Deinterlacer == "openglkerneldeint" || Deinterlacer == "opengldoubleratekerneldeint")
            {
                fragment = KernelShader[bottom];
                cost = 15;
            }
            else
            {
                fragment = YUV2RGBFragmentShader;
                cost = 1;
            }
        }
    }

    fragment.replace("SELECT_COLUMN", (kGLUYVY == m_frameType) ? SelectColumn : "");
    // update packing code so this can be removed
    fragment.replace("%SWIZZLE%", kGLUYVY == m_frameType ? "arb" : "abr");

    m_shaderCost[Type] = cost;
    QOpenGLShaderProgram *program = m_render->CreateShaderProgram(vertex, fragment);
    if (!program)
        return false;

    m_shaders[Type] = program;
    return true;
}

/*! \brief Create and initialise an OpenGL texture suitable for a YV12 video frame
 * of the given size.
*/
MythGLTexture* OpenGLVideo::CreateVideoTexture(QSize Size, QSize &ActualTextureSize)
{
    MythGLTexture *texture = nullptr;
    if (kGLYV12 == m_frameType)
    {
        texture = m_render->CreateTexture(Size,
            QOpenGLTexture::UInt8,  QOpenGLTexture::Luminance,
            QOpenGLTexture::Linear, QOpenGLTexture::ClampToEdge,
            QOpenGLTexture::LuminanceFormat);
    }
    else if (kGLUYVY == m_frameType)
    {
        Size.setWidth(Size.width() >> 1);
        texture = m_render->CreateTexture(Size);
    }
    else if ((kGLHQUYV == m_frameType) || (kGLGPU == m_frameType))
    {
        texture = m_render->CreateTexture(Size);
    }

    if (texture)
    {
        ActualTextureSize = texture->m_totalSize;
        if (m_resizing)
            m_render->SetTextureFilters(texture, QOpenGLTexture::Nearest, QOpenGLTexture::ClampToEdge);
    }
    return texture;
}

MythGLTexture* OpenGLVideo::GetInputTexture(void) const
{
    return m_inputTextures[0];
}

/*! \brief Update the current input texture using the data from the given YV12 video
 *  frame.
 */
void OpenGLVideo::UpdateInputFrame(const VideoFrame *Frame)
{
    OpenGLLocker ctx_lock(m_render);

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_render->logDebugMarker(LOC + "UPDATE_FRAME_START");

    if (Frame->width  != m_videoDim.width()  ||
        Frame->height != m_videoDim.height() ||
        Frame->width  < 1 || Frame->height < 1 ||
        Frame->codec != FMT_YV12 || (kGLGPU == m_frameType))
    {
        return;
    }
    if (m_hardwareDeinterlacing)
        RotateTextures();

    m_videoColourSpace->UpdateColourSpace(Frame);

    MythGLTexture *texture = m_inputTextures[0];
    if (!texture)
        return;

    void* buf = nullptr;
    if ((kGLYV12 == m_frameType) && (m_videoDim.width() == Frame->pitches[0]))
    {
        buf = Frame->buf;
    }
    else
    {
        // create a storage buffer
        if (!texture->m_data)
        {
            int size = texture->m_bufferSize;
            // extra needed for raw YV12 frame
            if (kGLYV12 == m_frameType)
                size = size + (size >> 1);
            unsigned char *scratch = new unsigned char[size];
            if (scratch)
            {
                memset(scratch, 0, static_cast<size_t>(texture->m_bufferSize));
                texture->m_data = scratch;
            }
        }
        if (!texture->m_data)
            return;
        buf = texture->m_data;

        if (kGLYV12 == m_frameType)
        {
            copybuffer(static_cast<uint8_t*>(buf), Frame, m_videoDim.width());
        }
        else if (kGLUYVY == m_frameType)
        {
            AVFrame img_out;
            m_copyCtx.Copy(&img_out, Frame, static_cast<unsigned char*>(buf), AV_PIX_FMT_UYVY422);
        }
        else if (kGLHQUYV == m_frameType && Frame->interlaced_frame)
        {
            pack_yv12interlaced(Frame->buf, static_cast<unsigned char*>(buf), Frame->offsets,
                                Frame->pitches, m_videoDim);
        }
        else if (kGLHQUYV == m_frameType)
        {
            pack_yv12progressive(Frame->buf, static_cast<unsigned char*>(buf), Frame->offsets,
                                 Frame->pitches, m_videoDim);
        }
    }

    texture->m_texture->setData(texture->m_pixelFormat, texture->m_pixelType, buf);

    // update chroma for YV12 textures
    if ((kGLYV12 == m_frameType) && (m_inputTextures.size() == 3))
    {
        int offset = (m_videoDim.width() * m_videoDim.height());
        m_inputTextures[1]->m_texture->setData(m_inputTextures[1]->m_pixelFormat,
                m_inputTextures[1]->m_pixelType, static_cast<uint8_t*>(buf) + offset);
        m_inputTextures[2]->m_texture->setData(m_inputTextures[2]->m_pixelFormat,
                m_inputTextures[2]->m_pixelType, static_cast<uint8_t*>(buf) + offset + (offset >> 2));
    }

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_render->logDebugMarker(LOC + "UPDATE_FRAME_END");
}

void OpenGLVideo::SetDeinterlacing(bool Deinterlacing)
{
    m_hardwareDeinterlacing = Deinterlacing;
}

void OpenGLVideo::PrepareFrame(bool TopFieldFirst, FrameScanType Scan,
                               StereoscopicMode Stereo, bool DrawBorder)
{
    if (m_inputTextures.empty() || !m_render)
        return;

    OpenGLLocker ctx_lock(m_render);

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_render->logDebugMarker(LOC + "PREP_FRAME_START");

    // First determine which shader to use. This helps optimise the resize check.

    bool deinterlacing = false;
    VideoShaderType program = (kGLGPU == m_frameType) ? Default : Progressive;
    if (m_hardwareDeinterlacing && !m_referenceTexturesNeeded)
    {
        if (Scan == kScan_Interlaced)
        {
            program = TopFieldFirst ? InterlacedTop : InterlacedBot;
            deinterlacing = true;
        }
        else if (Scan == kScan_Intr2ndField)
        {
            program = TopFieldFirst ? InterlacedBot : InterlacedTop;
            deinterlacing = true;
        }
    }

    // Decide whether to use render to texture - for performance or quality
    bool resize = false;
    if (kGLGPU != m_frameType)
    {
        // ensure deinterlacing works correctly when down scaling in height
        resize = (m_videoDispDim.height() > m_displayVideoRect.height()) && deinterlacing;
        // UYVY packed pixels must be sampled exactly
        resize |= (kGLUYVY == m_frameType);
        // user forced
        resize |= m_forceResize;

        if (!resize)
        {
            // improve performance. This is an educated guess on the relative cost
            // of render to texture versus straight rendering.
            int totexture    = m_videoDispDim.width() * m_videoDispDim.height() * m_shaderCost[program];
            int blitcost     = m_displayVideoRect.width() * m_displayVideoRect.height() * m_shaderCost[Default];
            int noresizecost = m_displayVideoRect.width() * m_displayVideoRect.height() * m_shaderCost[program];
            resize = (totexture + blitcost) < noresizecost;
        }
    }

    // set filtering if resizing has changed
    if (!resize && m_resizing)
    {
        if (m_frameBufferTexture)
        {
            m_render->DeleteTexture(m_frameBufferTexture);
            m_frameBufferTexture = nullptr;
        }
        if (m_frameBuffer)
        {
            m_render->DeleteFramebuffer(m_frameBuffer);
            m_frameBuffer = nullptr;
        }
        SetTextureFilters(&m_inputTextures,     QOpenGLTexture::Linear, QOpenGLTexture::ClampToEdge);
        SetTextureFilters(&m_referenceTextures, QOpenGLTexture::Linear, QOpenGLTexture::ClampToEdge);
        m_resizing = false;
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Disabled resizing");
    }
    else if (!m_resizing && resize)
    {
        SetTextureFilters(&m_inputTextures,     QOpenGLTexture::Nearest, QOpenGLTexture::ClampToEdge);
        SetTextureFilters(&m_referenceTextures, QOpenGLTexture::Nearest, QOpenGLTexture::ClampToEdge);
        m_resizing = true;
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Enabled resizing");
    }

    // inputs for the last stage
    vector<MythGLTexture*> inputtextures = m_inputTextures;
    bool discardframebuffer = false;

    if (resize)
    {
        // render to texture stage
        if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
            m_render->logDebugMarker(LOC + "RENDER_TO_TEXTURE");

        // we need a framebuffer
        if (!m_frameBuffer)
        {
            m_frameBuffer = m_render->CreateFramebuffer(m_videoDispDim);
            if (!m_frameBuffer)
                return;
        }

        // and its associated texture
        if (!m_frameBufferTexture)
        {
            m_frameBufferTexture = m_render->CreateFramebufferTexture(m_frameBuffer);
            if (!m_frameBufferTexture)
                return;
            m_render->SetTextureFilters(m_frameBufferTexture, QOpenGLTexture::Linear, QOpenGLTexture::ClampToEdge);
        }

        // coordinates
        QRect vrect(QPoint(0, 0), m_videoDispDim);
        QRect trect = vrect;
        if (kGLUYVY == m_frameType)
            trect.setWidth(m_videoDispDim.width() >> 1);

        // framebuffer
        m_render->BindFramebuffer(m_frameBuffer);
        m_render->SetViewPort(vrect);

        // bind correct textures
        MythGLTexture* textures[MAX_VIDEO_TEXTURES];
        uint numtextures = 0;
        for (uint i = 0; i < inputtextures.size(); i++)
            textures[numtextures++] = inputtextures[i];
        if (deinterlacing)
            for (uint i = 0; i < m_referenceTextures.size(); i++)
                textures[numtextures++] = m_referenceTextures[i];

        // render
        m_render->DrawBitmap(textures, numtextures, m_frameBuffer, trect, vrect, m_shaders[program]);

        // reset for next stage
        inputtextures.clear();
        inputtextures.push_back(m_frameBufferTexture);
        program = Default;
        discardframebuffer = true;
        deinterlacing = false;
    }

    // render to default framebuffer/screen
    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_render->logDebugMarker(LOC + "RENDER_TO_SCREEN");

    // texture coordinates
    QRect trect(m_videoRect);

    // discard stereoscopic fields
    if (kStereoscopicModeSideBySideDiscard == Stereo)
        trect = QRect(trect.left() >> 1, trect.top(), trect.width() >> 1, trect.height());
    if (kStereoscopicModeTopAndBottomDiscard == Stereo)
        trect = QRect(trect.left(), trect.top() >> 1, trect.width(), trect.height() >> 1);

    // bind default framebuffer
    m_render->BindFramebuffer(nullptr);
    if (m_viewportControl)
        m_render->SetViewPort(QRect(QPoint(), m_displayVisibleRect.size()));
    else
        m_render->SetViewPort(QRect(QPoint(), m_masterViewportSize));

    // PiP border
    if (DrawBorder)
    {
        QRect piprect = m_displayVideoRect.adjusted(-10, -10, +10, +10);
        static const QPen nopen(Qt::NoPen);
        static const QBrush redbrush(QBrush(QColor(127, 0, 0, 255)));
        m_render->DrawRect(nullptr, piprect, redbrush, nopen, 255);
    }

    // bind correct textures
    MythGLTexture* textures[MAX_VIDEO_TEXTURES];
    uint numtextures = 0;
    for (uint i = 0; i < inputtextures.size(); i++)
        textures[numtextures++] = inputtextures[i];

    if (deinterlacing)
        for (uint i = 0; i < m_referenceTextures.size(); i++)
            textures[numtextures++] = m_referenceTextures[i];

    // draw
    m_render->DrawBitmap(textures, numtextures, nullptr, trect, m_displayVideoRect, m_shaders[program]);

    // discard
    if (m_extraFeatures & kGLExtFBDiscard)
    {
        if (discardframebuffer && m_frameBuffer)
            m_render->DiscardFramebuffer(m_frameBuffer);
        m_render->DiscardFramebuffer(nullptr);
    }

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_render->logDebugMarker(LOC + "PREP_FRAME_END");
}

void OpenGLVideo::RotateTextures(void)
{
    if (m_referenceTexturesNeeded > 0)
        m_referenceTexturesNeeded--;

    if (m_referenceTextures.empty() || (m_referenceTextures.size() % m_inputTextures.size()))
        return;

    uint rotate = static_cast<uint>(m_inputTextures.size());
    vector<MythGLTexture*> newinputs, newrefs;
    for (uint i = 0; i < rotate; ++i)
        newinputs.push_back(m_referenceTextures[i]);
    for (uint i = rotate; i < m_referenceTextures.size(); ++i)
        newrefs.push_back(m_referenceTextures[i]);
    for (uint i = 0; i < rotate; ++i)
        newrefs.push_back(m_inputTextures[i]);
    m_inputTextures = newinputs;
    m_referenceTextures = newrefs;
}

void OpenGLVideo::DeleteTextures(vector<MythGLTexture*> *Textures)
{
    if (Textures && m_render)
    {
        for (uint i = 0; i < Textures->size(); i++)
            m_render->DeleteTexture((*Textures)[i]);
        Textures->clear();
    }
}

void OpenGLVideo::SetTextureFilters(vector<MythGLTexture *> *Textures,
                                    QOpenGLTexture::Filter Filter,
                                    QOpenGLTexture::WrapMode Wrap)
{
    if (Textures && m_render)
        for (uint i = 0; i < Textures->size(); i++)
            m_render->SetTextureFilters((*Textures)[i], Filter, Wrap);
}

OpenGLVideo::FrameType OpenGLVideo::StringToType(const QString &Type)
{
    QString type = Type.toLower().trimmed();
    if ("opengl-yv12" == type)  return kGLYV12;
    if ("opengl-hquyv" == type) return kGLHQUYV;
    if ("opengl-gpu" == type)   return kGLGPU;
    return kGLUYVY; // opengl
}

QString OpenGLVideo::TypeToString(FrameType Type)
{
    switch (Type)
    {
        case kGLGPU:   return "opengl-gpu";
        case kGLUYVY:  return "opengl"; // compatibility with old profiles
        case kGLYV12:  return "opengl-yv12";
        case kGLHQUYV: return "opengl-hquyv";
    }
    return "opengl";
}
