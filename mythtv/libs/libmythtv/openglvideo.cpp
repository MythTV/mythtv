// MythTV
#include "mythcontext.h"
#include "tv.h"
#include "mythrender_opengl.h"
#include "mythavutil.h"
#include "openglvideoshaders.h"
#include "openglvideo.h"

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
                         bool  ViewportControl, QString Profile)
  : QObject(),
    m_valid(false),
    m_inputType(FMT_YV12),
    m_outputType(FMT_YV12),
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
    m_inputTextures(),
    m_prevTextures(),
    m_nextTextures(),
    m_frameBuffer(nullptr),
    m_frameBufferTexture(nullptr),
    m_inputTextureSize(m_videoDim),
    m_features(),
    m_extraFeatures(0),
    m_resizing(false),
    m_forceResize(false),
    m_textureTarget(QOpenGLTexture::Target2D)
{
    memset(m_shaders, 0, sizeof(m_shaders));
    memset(m_shaderCost, 1, sizeof(m_shaderCost));

    if (!m_render || !m_videoColourSpace)
        return;

    OpenGLLocker ctx_lock(m_render);
    m_render->IncrRef();

    m_videoColourSpace->IncrRef();
    connect(m_videoColourSpace, &VideoColourSpace::Updated, this, &OpenGLVideo::UpdateColourSpace);

    // Set OpenGL feature support
    m_features      = m_render->GetFeatures();
    m_extraFeatures = m_render->GetExtraFeatures();

    // Create initial input texture(s). Hardware textures are created on demand.
    m_inputType = ProfileToType(Profile);
    if (!format_is_hw(m_inputType))
    {
        m_outputType = m_inputType;
        m_inputType = FMT_YV12;
        vector<QSize> sizes;
        sizes.push_back(QSize(m_videoDim));
        m_inputTextures = MythVideoTexture::CreateTextures(m_render, m_inputType, m_outputType, sizes);
        if (m_inputTextures.empty())
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create input textures");
        else
            m_inputTextureSize = m_inputTextures[0]->m_totalSize;
    }

    // Create initial shaders - default and progressive
    if (!CreateVideoShader(Default) || !CreateVideoShader(Progressive))
        return;

    UpdateColourSpace();
    UpdateShaderParameters();

    // we have a shader and a texture...
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Using '%1' for OpenGL video type").arg(GetProfile()));

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
    MythVideoTexture::DeleteTextures(m_render, m_inputTextures);
    MythVideoTexture::DeleteTextures(m_render, m_prevTextures);
    MythVideoTexture::DeleteTextures(m_render, m_nextTextures);

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

QString OpenGLVideo::GetProfile(void) const
{
    if (format_is_hw(m_inputType))
        return TypeToProfile(m_inputType);
    return TypeToProfile(m_outputType);
}

QSize OpenGLVideo::GetVideoSize(void) const
{
    return m_videoDim;
}

bool OpenGLVideo::AddDeinterlacer(QString &Deinterlacer)
{
    OpenGLLocker ctx_lock(m_render);

    if (format_is_hw(m_inputType))
        return false;

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
    MythVideoTexture::DeleteTextures(m_render, m_prevTextures);
    MythVideoTexture::DeleteTextures(m_render, m_nextTextures);

    // sanity check max texture units. Should only be an issue on old hardware (e.g. Pi)
    int max = m_render->GetMaxTextureUnits();
    uint refstocreate = kernel ? 2 : 0;
    int totaltextures = planes(m_outputType) * static_cast<int>(refstocreate + 1);
    if (totaltextures > max)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Insufficent texture units for '%1' (%2 < %3)")
            .arg(Deinterlacer).arg(max).arg(totaltextures));
        Deinterlacer = Deinterlacer.contains("double") ? "openglbobdeint" : "openglonefield";
        LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Falling back to '%1'").arg(Deinterlacer));
    }

    // create new deinterlacers - the old ones will be deleted
    if (!(CreateVideoShader(InterlacedBot, Deinterlacer) && CreateVideoShader(InterlacedTop, Deinterlacer)))
        return false;

    // create the correct number of reference textures
    if (refstocreate)
    {
        vector<QSize> sizes;
        sizes.push_back(QSize(m_videoDim));
        m_prevTextures = MythVideoTexture::CreateTextures(m_render, m_inputType, m_outputType, sizes);
        m_nextTextures = MythVideoTexture::CreateTextures(m_render, m_inputType, m_outputType, sizes);
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
 *
 * \note m_interopFrameType is used to override the type of shader created for
 * interop input. So kGLInterop frame type and kGLInterop interop type = RGB!
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

    if ((Default == Type) || (!format_is_yuv(m_outputType)))
    {
        fragment = DefaultFragmentShader;
    }
    // no interlaced shaders yet
    else if ((Progressive == Type) || (Interlaced == Type) || Deinterlacer.isEmpty())
    {
        switch (m_outputType)
        {
            case FMT_NV12:  fragment = NV12FragmentShader;    cost = 2; break;
            case FMT_YV12:  fragment = YV12RGBFragmentShader; cost = 3; break;
            case FMT_YUY2:
            case FMT_YUYVHQ:
            default:        fragment = YUV2RGBFragmentShader; cost = 1; break;
        }
    }
    else
    {
        uint bottom = (InterlacedBot == Type);
        if (FMT_YV12 == m_outputType)
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
        else if (FMT_NV12 == m_outputType)
        {
            if (Deinterlacer == "openglonefield" || Deinterlacer == "openglbobdeint")
            {
                fragment = NV12OneFieldFragmentShader[bottom];
                cost = 4;
            }
            else if (Deinterlacer == "opengllinearblend" || Deinterlacer == "opengldoubleratelinearblend")
            {
                fragment = NV12LinearBlendFragmentShader[bottom];
                cost = 10;
            }
            else if (Deinterlacer == "openglkerneldeint" || Deinterlacer == "opengldoubleratekerneldeint")
            {
                // NB NO kernel shader yet. Need to figure out reference frames for interop.
                fragment = NV12LinearBlendFragmentShader[bottom];
                cost = 10;
            }
            else
            {
                fragment = NV12FragmentShader;
                cost = 2;
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

    fragment.replace("SELECT_COLUMN", (FMT_YUY2 == m_outputType) ? SelectColumn : "");
    // update packing code so this can be removed
    fragment.replace("%SWIZZLE%", (FMT_YUY2 == m_outputType) ? "arb" : "abr");

    // N.B. Rectangular texture support is only currently used for VideoToolBox
    // video frames which are NV12. Do not use rectangular textures for the 'default'
    // shaders as it breaks video resizing and would require changes to our
    // FramebufferObject code.
    if ((m_textureTarget == QOpenGLTexture::TargetRectangle) && (Default != Type))
    {
        // N.B. Currently only NV12 shaders are supported and deinterlacing parameters
        // need fixing as well (when interop deinterlacing is fixed)
        fragment.replace("%NV12_UV_RECT%", " * vec2(0.5, 0.5)");
        fragment.replace("sampler2D", "sampler2DRect");
        fragment.replace("texture2D", "texture2DRect");
        fragment.prepend("#extension GL_ARB_texture_rectangle : enable\n");
    }
    else if (m_textureTarget == GL_TEXTURE_EXTERNAL_OES)
    {
        fragment.replace("%NV12_UV_RECT%", "");
        fragment.replace("sampler2D", "samplerExternalOES");
        fragment.prepend("#extension GL_OES_EGL_image_external : require\n");
    }
    else
    {
        fragment.replace("%NV12_UV_RECT%", "");
    }

    m_shaderCost[Type] = cost;
    QOpenGLShaderProgram *program = m_render->CreateShaderProgram(vertex, fragment);
    if (!program)
        return false;

    m_shaders[Type] = program;
    return true;
}

/*! \brief Update the current input texture using the data from the given YV12 video
 *  frame.
 */
void OpenGLVideo::UpdateInputFrame(const VideoFrame *Frame)
{
    OpenGLLocker ctx_lock(m_render);

    if (Frame->width  != m_videoDim.width()  ||
        Frame->height != m_videoDim.height() ||
        Frame->width  < 1 || Frame->height < 1 ||
        Frame->codec != m_inputType || format_is_hw(Frame->codec))
    {
        return;
    }

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_render->logDebugMarker(LOC + "UPDATE_FRAME_START");

    m_videoColourSpace->UpdateColourSpace(Frame);

    // Rotate textures if necessary
    bool current = true;
    if (m_hardwareDeinterlacing)
    {
        if (!m_nextTextures.empty() && !m_prevTextures.empty())
        {
            vector<MythVideoTexture*> temp = m_prevTextures;
            m_prevTextures = m_inputTextures;
            m_inputTextures = m_nextTextures;
            m_nextTextures = temp;
            current = false;
        }
    }

    MythVideoTexture::UpdateTextures(m_render, Frame, current ? m_inputTextures : m_nextTextures);

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_render->logDebugMarker(LOC + "UPDATE_FRAME_END");
}

void OpenGLVideo::SetDeinterlacing(bool Deinterlacing)
{
    m_hardwareDeinterlacing = Deinterlacing;
}

void OpenGLVideo::PrepareFrame(VideoFrame *Frame, bool TopFieldFirst, FrameScanType Scan,
                               StereoscopicMode Stereo, bool DrawBorder)
{
    if (!m_render)
        return;

    OpenGLLocker ctx_lock(m_render);

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_render->logDebugMarker(LOC + "PREP_FRAME_START");

    // Set required input textures for the last stage
    vector<MythVideoTexture*> inputtextures;

    // Pull in any hardware frames
    if (format_is_hw(m_inputType))
    {
        inputtextures = MythOpenGLInterop::Retrieve(m_render, m_videoColourSpace, Frame, Scan);
        if (!inputtextures.empty())
        {
            QSize newsize = inputtextures[0]->m_size;
            VideoFrameType newsourcetype = inputtextures[0]->m_frameType;
            VideoFrameType newtargettype = inputtextures[0]->m_frameFormat;
            GLenum newtargettexture = inputtextures[0]->m_target;
            if ((m_outputType != newtargettype) || (m_textureTarget != newtargettexture) ||
                (m_inputType != newsourcetype) || (newsize != m_inputTextureSize))
            {
                LOG(VB_GENERAL, LOG_WARNING, LOC +
                    QString("Interop change %1:%2 %3x%4 (Texture: %5) -> %6:%7 %8x%9 (Texture: %10)")
                    .arg(m_inputType).arg(m_outputType).arg(m_inputTextureSize.width())
                    .arg(m_inputTextureSize.height()).arg(m_textureTarget)
                    .arg(newsourcetype).arg(newtargettype)
                    .arg(newsize.width()).arg(newsize.height()).arg(newtargettexture));

                m_inputTextureSize = newsize;
                m_inputType = newsourcetype;
                m_outputType = newtargettype;
                m_textureTarget = newtargettexture;
                CreateVideoShader(Default);
                CreateVideoShader(Progressive);
                UpdateColourSpace();
                UpdateShaderParameters();
            }
        }
    }
    else
    {
        inputtextures = m_inputTextures;
    }
    if (inputtextures.empty())
        return;

    // Determine which shader to use. This helps optimise the resize check.
    bool deinterlacing = false;
    VideoShaderType program = Progressive;
    if (!format_is_yuv(m_outputType))
        program = Default;

    if (m_hardwareDeinterlacing)
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
    if (format_is_yuv(m_outputType))
    {
        // ensure deinterlacing works correctly when down scaling in height
        resize = (m_videoDispDim.height() > m_displayVideoRect.height()) && deinterlacing;
        // UYVY packed pixels must be sampled exactly
        resize |= (FMT_YUY2 == m_outputType);
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
        MythVideoTexture::SetTextureFilters(m_render, m_inputTextures, QOpenGLTexture::Linear, QOpenGLTexture::ClampToEdge);
        MythVideoTexture::SetTextureFilters(m_render, m_prevTextures, QOpenGLTexture::Linear, QOpenGLTexture::ClampToEdge);
        MythVideoTexture::SetTextureFilters(m_render, m_nextTextures, QOpenGLTexture::Linear, QOpenGLTexture::ClampToEdge);
        m_resizing = false;
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Disabled resizing");
    }
    else if (!m_resizing && resize)
    {
        MythVideoTexture::SetTextureFilters(m_render, m_inputTextures, QOpenGLTexture::Nearest, QOpenGLTexture::ClampToEdge);
        MythVideoTexture::SetTextureFilters(m_render, m_prevTextures, QOpenGLTexture::Nearest, QOpenGLTexture::ClampToEdge);
        MythVideoTexture::SetTextureFilters(m_render, m_nextTextures, QOpenGLTexture::Nearest, QOpenGLTexture::ClampToEdge);
        m_resizing = true;
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Enabled resizing");
    }

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
            m_frameBufferTexture = reinterpret_cast<MythVideoTexture*>(m_render->CreateFramebufferTexture(m_frameBuffer));
            if (!m_frameBufferTexture)
                return;
            m_render->SetTextureFilters(m_frameBufferTexture, QOpenGLTexture::Linear, QOpenGLTexture::ClampToEdge);
        }

        // coordinates
        QRect vrect(QPoint(0, 0), m_videoDispDim);
        QRect trect = vrect;
        if (FMT_YUY2 == m_outputType)
            trect.setWidth(m_videoDispDim.width() >> 1);

        // framebuffer
        m_render->BindFramebuffer(m_frameBuffer);
        m_render->SetViewPort(vrect);

        // bind correct textures
        MythGLTexture* textures[MAX_VIDEO_TEXTURES];
        uint numtextures = 0;
        LoadTextures(deinterlacing, inputtextures, &textures[0], numtextures);

        // render
        m_render->DrawBitmap(textures, numtextures, m_frameBuffer, trect, vrect, m_shaders[program]);

        // reset for next stage
        inputtextures.clear();
        inputtextures.push_back(m_frameBufferTexture);
        program = Default;
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
    LoadTextures(deinterlacing, inputtextures, &textures[0], numtextures);

    // draw
    m_render->DrawBitmap(textures, numtextures, nullptr, trect, m_displayVideoRect, m_shaders[program]);

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_render->logDebugMarker(LOC + "PREP_FRAME_END");
}

/// \brief Clear reference frames after a seek as they will contain old images.
void OpenGLVideo::ResetTextures(void)
{
    for (uint i = 0; i < m_inputTextures.size(); ++i)
        m_inputTextures[i]->m_valid = false;
    for (uint i = 0; i < m_prevTextures.size(); ++i)
        m_prevTextures[i]->m_valid = false;
    for (uint i = 0; i < m_nextTextures.size(); ++i)
        m_nextTextures[i]->m_valid = false;
}

void OpenGLVideo::LoadTextures(bool Deinterlacing, vector<MythVideoTexture*> &Current,
                               MythGLTexture **Textures, uint &TextureCount)
{
    if (Deinterlacing && (m_nextTextures.size() == Current.size()) && (m_prevTextures.size() == Current.size()))
    {
        // if we are using reference frames, we want the current frame in the middle
        // but next will be the first valid, followed by current...
        uint count = Current.size();
        vector<MythVideoTexture*> &current = Current[0]->m_valid ? Current : m_nextTextures;
        vector<MythVideoTexture*> &prev    = m_prevTextures[0]->m_valid ? m_prevTextures : current;

        for (uint i = 0; i < count; ++i)
            Textures[TextureCount++] = reinterpret_cast<MythGLTexture*>(prev[i]);
        for (uint i = 0; i < count; ++i)
            Textures[TextureCount++] = reinterpret_cast<MythGLTexture*>(current[i]);
        for (uint i = 0; i < count; ++i)
            Textures[TextureCount++] = reinterpret_cast<MythGLTexture*>(m_nextTextures[i]);
    }
    else
    {
        for (uint i = 0; i < Current.size(); ++i)
            Textures[TextureCount++] = reinterpret_cast<MythGLTexture*>(Current[i]);
    }
}

VideoFrameType OpenGLVideo::ProfileToType(const QString &Profile)
{
    QString type = Profile.toLower().trimmed();
    if ("opengl-yv12" == type)  return FMT_YV12;
    if ("opengl-hquyv" == type) return FMT_YUYVHQ;
    if ("opengl-nv12" == type)  return FMT_NV12;
    if ("opengl-hw" == type)    return FMT_VDPAU; // Placeholder value
    return FMT_YUY2; // opengl
}

QString OpenGLVideo::TypeToProfile(VideoFrameType Type)
{
    if (format_is_hw(Type))
        return "opengl-hw";

    switch (Type)
    {
        case FMT_YUY2:    return "opengl"; // compatibility with old profiles
        case FMT_YV12:    return "opengl-yv12";
        case FMT_YUYVHQ:  return "opengl-hquyv";
        case FMT_NV12:    return "opengl-nv12";
        default: break;
    }
    return "opengl";
}
