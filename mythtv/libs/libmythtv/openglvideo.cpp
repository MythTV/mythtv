// MythTV headers
#include "openglvideo.h"
#include "mythcontext.h"
#include "tv.h"
#include "mythrender_opengl.h"
#include "mythavutil.h"
#include "openglvideoshaders.h"

#define LOC QString("GLVid: ")

enum DisplayBuffer
{
    kDefaultBuffer,
    kFrameBufferObject
};

class OpenGLFilter
{
    public:
        vector<QOpenGLShaderProgram*> fragmentPrograms;
        uint           numInputs;
        vector<QOpenGLFramebufferObject*> frameBuffers;
        vector<MythGLTexture*>            frameBufferTextures;
        DisplayBuffer  outputBuffer;
};

/**
 * \class OpenGLVideo
 *  A class used to display video frames and associated imagery
 *  using the OpenGL API.
 *
 *  The basic operational concept is to use a series of filter stages to
 *  generate the desired video output, using limited software assistance
 *  alongside OpenGL fragment programs (deinterlacing and YUV->RGB conversion)
 *  , FrameBuffer Objects (flexible GPU storage) and PixelBuffer Objects
 *  (faster CPU->GPU memory transfers).
 *
 *  Higher level tasks such as coordination between OpenGLVideo instances,
 *  video buffer management, audio/video synchronisation etc are handled by
 *  the higher level classes VideoOutput and MythPlayer. The bulk of
 *  the lower level interface with the window system and OpenGL is handled by
 *  MythRenderOpenGL.
 *
 *  N.B. There should be no direct OpenGL calls from this class.
*/

/**
 *  Create a new OpenGLVideo instance that must be initialised
 *  with a call to OpenGLVideo::Init()
 */

OpenGLVideo::OpenGLVideo()
  : QObject(),
    m_frameType(kGLUYVY),
    m_render(nullptr),
    m_videoDispDim(0,0),
    m_videoDim(0,0),
    m_masterViewportSize(0,0),
    m_displayVisibleRect(0,0,0,0),
    m_displayVideoRect(0,0,0,0),
    m_videoRect(0,0,0,0),
    m_hardwareDeinterlacing(false),
    m_videoColourSpace(nullptr),
    m_viewportControl(false),
    m_inputTextureSize(0,0),
    m_referenceTexturesNeeded(0),
    m_extraFeatures(0),
    m_forceResize(false)
{
    m_forceResize = gCoreContext->GetBoolSetting("OpenGLExtraStage", false);
    m_discardFrameBuffers = gCoreContext->GetBoolSetting("OpenGLDiscardFB", false);
}

OpenGLVideo::~OpenGLVideo()
{
    if (m_render)
        m_render->makeCurrent();
    m_videoColourSpace->DecrRef();
    Teardown();
    if (m_render)
    {
        m_render->doneCurrent();
        m_render->DecrRef();
    }
}

void OpenGLVideo::Teardown(void)
{
    if (m_viewportControl)
        m_render->DeleteFence();

    DeleteTextures(&m_inputTextures);
    DeleteTextures(&m_referenceTextures);

    while (!m_filters.empty())
        RemoveFilter(m_filters.begin()->first);
}

/**
 *  \param glcontext          the MythRenderOpenGL object responsible for lower
 *   levelwindow and OpenGL context integration
 *  \param colourspace        the colourspace management object
 *  \param videoDim           the size of the video source
 *  \param videoDispDim       the size of the display
 *  \param displayVisibleRect the bounding rectangle of the OpenGL window
 *  \param displayVideoRect   the bounding rectangle for the area to display
 *   the video frame
 *  \param videoRect          the portion of the video frame to display in
     displayVideoRect
 *  \param viewport_control   if true, this instance may permanently change
     the OpenGL viewport
 *  \param hw_accel           if true, a GPU decoder will copy frames directly
     to an RGBA texture
 */

bool OpenGLVideo::Init(MythRenderOpenGL *Render, VideoColourSpace *ColourSpace,
                       QSize VideoDim, QSize VideoDispDim,
                       QRect DisplayVisibleRect, QRect DisplayVideoRect, QRect VideoRect,
                       bool  ViewportControl, FrameType Type)
{
    if (!Render)
        return false;

    if (m_render)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Cannot reinit OpenGLVideo");
        return false;
    }

    m_render            = Render;
    m_render->IncrRef();
    OpenGLLocker ctx_lock(m_render);

    m_videoDim            = VideoDim;
    m_videoDispDim        = VideoDispDim;
    m_displayVisibleRect  = DisplayVisibleRect;
    m_displayVideoRect    = DisplayVideoRect;
    m_videoRect           = VideoRect;
    m_masterViewportSize  = m_displayVisibleRect.size();
    m_softwareDeiterlacer = "";
    m_hardwareDeinterlacing = false;
    m_videoColourSpace      = ColourSpace;
    m_viewportControl       = ViewportControl;
    m_inputTextureSize      = QSize(0,0);
    m_frameType             = Type;

    m_videoColourSpace->IncrRef();
    connect(m_videoColourSpace, &VideoColourSpace::Updated, this, &OpenGLVideo::UpdateColourSpace);

    // Set OpenGL feature support
    m_features      = m_render->GetFeatures();
    m_extraFeatures = m_render->GetExtraFeatures();

    // Enable/Disable certain features based on settings
    if (m_render->isOpenGLES())
    {
        if ((m_extraFeatures & kGLExtFBDiscard) && m_discardFrameBuffers)
            LOG(VB_GENERAL, LOG_INFO, LOC + "Enabling Framebuffer discards");
        else
            m_extraFeatures &= ~kGLExtFBDiscard;
    }

    if (m_viewportControl)
        m_render->SetFence();

    bool glsl  = m_features & QOpenGLFunctions::Shaders;
    bool fbos  = m_features & QOpenGLFunctions::Framebuffers;
    FrameType fallback = glsl ? (fbos ? kGLUYVY : kGLYV12) : kGLRGBA;

    // check for feature support
    bool unsupported = (kGLYV12  == m_frameType) && !glsl;
    unsupported     |= (kGLUYVY  == m_frameType) && (!glsl || !fbos);
    unsupported     |= (kGLHQUYV == m_frameType) && !glsl;

    if (unsupported)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + QString(
                "'%1' OpenGLVideo type not available - falling back to '%2'")
                .arg(TypeToString(m_frameType)).arg(TypeToString(fallback)));
        m_frameType = fallback;
    }

    // check picture attributes support - colourspace adjustments require
    // shaders to operate on YUV textures
    if (kGLRGBA == m_frameType || kGLGPU == m_frameType)
        ColourSpace->SetSupportedAttributes(kPictureAttributeSupported_None);

    // Create initial input texture and associated filter stage
    MythGLTexture *tex = CreateVideoTexture(m_videoDim, m_inputTextureSize);
    bool    ok = false;

    if (kGLYV12 == m_frameType)
        ok = tex && AddFilter(kGLFilterYV12RGB);
    else if ((kGLUYVY == m_frameType) || (kGLHQUYV == m_frameType))
        ok = tex && AddFilter(kGLFilterYUV2RGB);
    else
        ok = tex && AddFilter(kGLFilterResize);

    if (ok)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Using '%1' for OpenGL video type")
            .arg(TypeToString(m_frameType)));
        m_inputTextures.push_back(tex);
    }
    else
    {
        Teardown();
    }

    if (m_filters.empty())
    {
        bool fatalerror = kGLRGBA == m_frameType;

        if (!fatalerror)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                    "Failed to setup colourspace conversion.\n\t\t\t"
                    "Falling back to software conversion.\n\t\t\t"
                    "Any opengl filters will also be disabled.");
            MythGLTexture *rgba32tex = CreateVideoTexture(m_videoDim, m_inputTextureSize);

            if (rgba32tex && AddFilter(kGLFilterResize))
            {
                m_inputTextures.push_back(rgba32tex);
                m_videoColourSpace->SetSupportedAttributes(kPictureAttributeSupported_None);
            }
            else
            {
                fatalerror = true;
            }
        }
        if (fatalerror)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Fatal error");
            Teardown();
            return false;
        }
    }

    CheckResize(false);
    return true;
}

/**
 *   Determine if the output is to be scaled at all and create or destroy
 *   the appropriate filter as necessary.
 */
void OpenGLVideo::CheckResize(bool Deinterlacing)
{
    // to improve performance on slower cards when deinterlacing
    bool resize = ((m_videoDispDim.height() < m_displayVideoRect.height()) ||
                   (m_videoDispDim.width() < m_displayVideoRect.width()));

    // to ensure deinterlacing works correctly
    resize |= (m_videoDispDim.height() > m_displayVideoRect.height()) &&
                        Deinterlacing;

    // UYVY packed pixels must be sampled exactly and any overscan settings will
    // break sampling - so always force an extra stage
    // TODO optimise this away when 1 for 1 mapping is guaranteed
    resize |= (kGLUYVY == m_frameType);

    // we always need at least one filter (i.e. a resize that simply blits the texture
    // to screen)
    resize |= !m_filters.count(kGLFilterYUV2RGB) && !m_filters.count(kGLFilterYV12RGB);

    // Extra stage needed on Fire Stick 4k, maybe others, because of blank screen when playing.
    resize |= m_forceResize;

    if (resize)
        AddFilter(kGLFilterResize);
    else
        RemoveFilter(kGLFilterResize);
    OptimiseFilters();
}

void OpenGLVideo::UpdateColourSpace(void)
{
    OpenGLLocker locker(m_render);
    glfilt_map_t::const_iterator filter = m_filters.find(kGLFilterYUV2RGB);
    if (filter == m_filters.cend())
        filter = m_filters.find(kGLFilterYV12RGB);
    if (filter == m_filters.cend())
        return;

    vector<QOpenGLShaderProgram*>::const_iterator it = filter->second->fragmentPrograms.cbegin();
    for ( ; it != filter->second->fragmentPrograms.cend(); ++it)
        m_render->SetShaderProgramParams(*it, *m_videoColourSpace, "m_colourMatrix");
}

void OpenGLVideo::UpdateShaderParameters(void)
{
    if (m_inputTextureSize.isEmpty())
        return;

    OpenGLLocker locker(m_render);
    glfilt_map_t::const_iterator filter = m_filters.find(kGLFilterYUV2RGB);
    if (filter == m_filters.cend())
        filter = m_filters.find(kGLFilterYV12RGB);
    if (filter == m_filters.cend())
        return;

    qreal lineheight = 1.0 / m_inputTextureSize.height();
    qreal maxheight  = m_videoDispDim.height() / (qreal)m_inputTextureSize.height();
    QVector4D parameters(lineheight,                        /* lineheight */
                        (qreal)m_inputTextureSize.width(),  /* 'Y' select */
                        maxheight - lineheight,             /* maxheight  */
                        m_inputTextureSize.height() / 2.0   /* fieldsize  */);

    vector<QOpenGLShaderProgram*>::const_iterator it = filter->second->fragmentPrograms.cbegin();
    for ( ; it != filter->second->fragmentPrograms.cend(); ++it)
    {
        QOpenGLShaderProgram *program = *it;
        m_render->EnableShaderProgram(program);
        program->setUniformValue("m_frameData", parameters);
    }
}

void OpenGLVideo::SetMasterViewport(QSize Size)
{
    m_masterViewportSize = Size;
}

void OpenGLVideo::SetVideoRect(const QRect &DisplayVideoRect, const QRect &VideoRect)
{
    if (VideoRect == m_videoRect && DisplayVideoRect == m_displayVideoRect)
        return;

    m_displayVideoRect = DisplayVideoRect;
    m_videoRect = VideoRect;
    m_render->makeCurrent();
    CheckResize(m_hardwareDeinterlacing);
    m_render->doneCurrent();
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

/**
 *  Ensure the current chain of OpenGLFilters is logically correct
 *  and has the resources required to complete rendering.
 */

bool OpenGLVideo::OptimiseFilters(void)
{
    glfilt_map_t::reverse_iterator it;

    // add/remove required frame buffer objects
    // and link filters
    uint buffers_needed = 1;
    bool last_filter    = true;
    for (it = m_filters.rbegin(); it != m_filters.rend(); ++it)
    {
        if (!last_filter)
        {
            it->second->outputBuffer = kFrameBufferObject;
            uint buffers_have = it->second->frameBuffers.size();
            int buffers_diff = buffers_needed - buffers_have;
            if (buffers_diff > 0)
            {
                for (int i = 0; i < buffers_diff; i++)
                {
                    QOpenGLFramebufferObject* framebuffer = m_render->CreateFramebuffer(m_videoDispDim);
                    if (framebuffer)
                    {
                        MythGLTexture *texture = m_render->CreateFramebufferTexture(framebuffer);
                        it->second->frameBuffers.push_back(framebuffer);
                        it->second->frameBufferTextures.push_back(texture);
                    }
                    else
                        return false;
                }
            }
            else if (buffers_diff < 0)
            {
                for (int i = 0; i > buffers_diff; i--)
                {
                    OpenGLFilter *filt = it->second;
                    m_render->DeleteFramebuffer(filt->frameBuffers.back());
                    filt->frameBuffers.pop_back();
                }
            }
        }
        else
        {
            it->second->outputBuffer = kDefaultBuffer;
            last_filter = false;
        }
        buffers_needed = it->second->numInputs;
    }

    SetFiltering();
    return true;
}

/**
 *  Set the OpenGL texture mapping functions to optimise speed and quality.
 */

void OpenGLVideo::SetFiltering(void)
{
    if (m_filters.size() < 2)
    {
        SetTextureFilters(&m_inputTextures, QOpenGLTexture::Linear, QOpenGLTexture::ClampToEdge);
        SetTextureFilters(&m_referenceTextures, QOpenGLTexture::Linear, QOpenGLTexture::ClampToEdge);
        return;
    }

    SetTextureFilters(&m_inputTextures, QOpenGLTexture::Nearest, QOpenGLTexture::ClampToEdge);
    SetTextureFilters(&m_referenceTextures, QOpenGLTexture::Nearest, QOpenGLTexture::ClampToEdge);

    glfilt_map_t::reverse_iterator rit;
    int last_filter = 0;

    for (rit = m_filters.rbegin(); rit != m_filters.rend(); ++rit)
    {
        if (last_filter == 1)
        {
            foreach (QOpenGLFramebufferObject* fb, rit->second->frameBuffers)
                m_render->SetTextureFilters(fb->texture(), QOpenGLTexture::Linear, QOpenGLTexture::ClampToEdge);
        }
        else if (last_filter > 1)
        {
            foreach (QOpenGLFramebufferObject* fb, rit->second->frameBuffers)
                m_render->SetTextureFilters(fb->texture(), QOpenGLTexture::Nearest, QOpenGLTexture::ClampToEdge);
        }
        ++last_filter;
    }
}

/**
 *  Add a new filter stage and create any additional resources needed.
 */

bool OpenGLVideo::AddFilter(OpenGLFilterType Filter)
{
    if (m_filters.count(Filter))
        return true;

    bool success = true;

    OpenGLFilter *temp = new OpenGLFilter();

    temp->numInputs = 1;
    QOpenGLShaderProgram *program = nullptr;

    if (success)
    {
        program = AddFragmentProgram(Filter);
        if (!program)
            success = false;
        else
            temp->fragmentPrograms.push_back(program);
    }

    if (success)
    {
        temp->outputBuffer = kDefaultBuffer;
        temp->frameBuffers.clear();
        m_filters[Filter] = temp;
        temp = nullptr;
        success &= OptimiseFilters();
        if ((kGLFilterYUV2RGB == Filter) || (kGLFilterYV12RGB == Filter))
        {
            UpdateColourSpace();
            UpdateShaderParameters();
        }
    }

    if (!success)
    {
        RemoveFilter(Filter);
        delete temp;
        return false;
    }

    return true;
}

bool OpenGLVideo::RemoveFilter(OpenGLFilterType Filter)
{
    if (!m_filters.count(Filter))
        return true;

    vector<QOpenGLShaderProgram*>::const_iterator it = m_filters[Filter]->fragmentPrograms.cbegin();
    for ( ; it != m_filters[Filter]->fragmentPrograms.cend(); ++it)
        m_render->DeleteShaderProgram(*it);
    m_filters[Filter]->fragmentPrograms.clear();

    vector<QOpenGLFramebufferObject*>::const_iterator it2 = m_filters[Filter]->frameBuffers.cbegin();
    for ( ; it2 != m_filters[Filter]->frameBuffers.cend(); ++it2)
        m_render->DeleteFramebuffer(*it2);
    m_filters[Filter]->frameBuffers.clear();

    vector<MythGLTexture*>::const_iterator it3 = m_filters[Filter]->frameBufferTextures.cbegin();
    for ( ; it3 != m_filters[Filter]->frameBufferTextures.cend(); ++it3)
        m_render->DeleteTexture(*it3);
    m_filters[Filter]->frameBufferTextures.clear();

    delete m_filters[Filter];
    m_filters.erase(Filter);
    return true;
}

void OpenGLVideo::TearDownDeinterlacer(void)
{
    glfilt_map_t::const_iterator it = m_filters.cbegin();
    for ( ; it != m_filters.cend(); ++it)
    {
        if (it->first != kGLFilterYUV2RGB && it->first != kGLFilterYV12RGB)
            continue;

        OpenGLFilter *tmp = it->second;
        while (tmp->fragmentPrograms.size() > 1)
        {
            m_render->DeleteShaderProgram(tmp->fragmentPrograms.back());
            tmp->fragmentPrograms.pop_back();
        }
    }

    DeleteTextures(&m_referenceTextures);
    m_referenceTexturesNeeded = 0;
}

/**
 *  Extends the functionality of the basic YUV->RGB filter stage to include
 *  deinterlacing (combining the stages is significantly more efficient than
 *  2 separate stages). Create 2 deinterlacing fragment programs, 1 for each
 *  required field.
 */

bool OpenGLVideo::AddDeinterlacer(const QString &Deinterlacer)
{
    OpenGLLocker ctx_lock(m_render);
    if (m_filters.end() == m_filters.find(kGLFilterYUV2RGB) && m_filters.end() == m_filters.find(kGLFilterYV12RGB) )
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC +
            "No YUV2RGB filter stage for OpenGL deinterlacing.");
        return false;
    }

    if (m_hardwareDeinterlacer == Deinterlacer)
        return true;

    TearDownDeinterlacer();

    bool success = true;

    uint refstocreate = Deinterlacer.contains("kernel") ? 2 : 0;
    m_referenceTexturesNeeded = refstocreate;
    for ( ; refstocreate > 0; refstocreate--)
    {
        MythGLTexture *tex = CreateVideoTexture(m_videoDim, m_inputTextureSize);
        if (tex)
            m_referenceTextures.push_back(tex);
        else
            success = false;
    }

    if (success)
    {
        OpenGLFilterType type = (kGLYV12 == m_frameType) ? kGLFilterYV12RGB : kGLFilterYUV2RGB;
        QOpenGLShaderProgram *prog1 = AddFragmentProgram(type, Deinterlacer, kScan_Interlaced);
        QOpenGLShaderProgram *prog2 = AddFragmentProgram(type, Deinterlacer, kScan_Intr2ndField);
        success = prog1 && prog2;
        if (success)
        {
            m_filters[type]->fragmentPrograms.push_back(prog1);
            m_filters[type]->fragmentPrograms.push_back(prog2);
        }
    }

    if (success)
    {
        CheckResize(m_hardwareDeinterlacing);
        m_hardwareDeinterlacer = Deinterlacer;
        UpdateColourSpace();
        UpdateShaderParameters();
        return true;
    }

    m_hardwareDeinterlacer = "";
    TearDownDeinterlacer();
    return false;
}

/**
 *  Create the correct fragment program for the given filter type
 */

QOpenGLShaderProgram* OpenGLVideo::AddFragmentProgram(OpenGLFilterType Filter, QString Deinterlacer, FrameScanType Field)
{
    if (!m_render || !(m_features & QOpenGLFunctions::Shaders))
        return nullptr;

    QString vertex = YUV2RGBVertexShader;
    QString fragment = DefaultFragmentShader;
    uint bottom = Field == kScan_Intr2ndField;
    switch (Filter)
    {
        case kGLFilterYUV2RGB:
        {
            if (Deinterlacer == "openglonefield" || Deinterlacer == "openglbobdeint")
                fragment = OneFieldShader[bottom];
            else if (Deinterlacer == "opengllinearblend" ||
                     Deinterlacer == "opengldoubleratelinearblend")
                fragment = LinearBlendShader[bottom];
            else if (Deinterlacer == "openglkerneldeint" ||
                     Deinterlacer == "opengldoubleratekerneldeint")
                fragment = KernelShader[bottom];
            else
                fragment = YUV2RGBFragmentShader;
            fragment.replace("SELECT_COLUMN", (kGLUYVY == m_frameType) ? SelectColumn : "");
            break;
        }
        case kGLFilterYV12RGB:
            vertex = YV12RGBVertexShader;
            if (Deinterlacer == "openglonefield" || Deinterlacer == "openglbobdeint")
                fragment = YV12RGBOneFieldFragmentShader[bottom];
            else if (Deinterlacer == "opengllinearblend" || Deinterlacer == "opengldoubleratelinearblend")
                fragment = YV12RGBLinearBlendFragmentShader[bottom];
            else if (Deinterlacer == "openglkerneldeint" || Deinterlacer == "opengldoubleratekerneldeint")
                fragment = YV12RGBKernelShader[bottom];
            else
                fragment = YV12RGBFragmentShader;
            break;
        case kGLFilterResize:
        default:
            break;
    }

    // update packing code so this can be removed
    fragment.replace("%SWIZZLE%", kGLUYVY == m_frameType ? "arb" : "abr");

    return m_render->CreateShaderProgram(vertex, fragment);
}

/**
 *  Create and initialise an OpenGL texture suitable for a YV12 video frame
 *  of the given size.
 */
MythGLTexture* OpenGLVideo::CreateVideoTexture(QSize Size, QSize &ActualTextureSize)
{
    MythGLTexture *texture = nullptr;
    if (kGLYV12 == m_frameType)
    {
        Size.setHeight((3 * Size.height() + 1) / 2);
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
    else if ((kGLHQUYV == m_frameType) || (kGLGPU == m_frameType) || (kGLRGBA == m_frameType))
    {
        texture = m_render->CreateTexture(Size);
    }

    ActualTextureSize = m_render->GetTextureSize(Size);
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
            unsigned char *scratch = new unsigned char[texture->m_bufferSize];
            if (scratch)
            {
                memset(scratch, 0, texture->m_bufferSize);
                texture->m_data = scratch;
            }
        }
        if (!texture->m_data)
            return;
        buf = texture->m_data;

        if (kGLYV12 == m_frameType)
        {
            copybuffer((uint8_t*)buf, Frame, m_videoDim.width());
        }
        else if (kGLUYVY == m_frameType)
        {
            AVFrame img_out;
            m_copyCtx.Copy(&img_out, Frame, (unsigned char*)buf, AV_PIX_FMT_UYVY422);
        }
        else if (kGLRGBA == m_frameType)
        {
            AVFrame img_out;
            m_copyCtx.Copy(&img_out, Frame, (unsigned char*)buf, AV_PIX_FMT_RGBA);
        }
        else if (kGLHQUYV == m_frameType && Frame->interlaced_frame)
        {
            pack_yv12interlaced(Frame->buf, (unsigned char*)buf, Frame->offsets,
                                Frame->pitches, m_videoDim);
        }
        else if (kGLHQUYV == m_frameType)
        {
            pack_yv12progressive(Frame->buf, (unsigned char*)buf, Frame->offsets,
                                 Frame->pitches, m_videoDim);
        }
    }

    texture->m_texture->setData(texture->m_pixelFormat, texture->m_pixelType, buf);

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_render->logDebugMarker(LOC + "UPDATE_FRAME_END");
}

void OpenGLVideo::SetDeinterlacing(bool Deinterlacing)
{
    m_hardwareDeinterlacing = Deinterlacing;
    OpenGLLocker ctx_lock(m_render);
    CheckResize(m_hardwareDeinterlacing);
}

void OpenGLVideo::SetSoftwareDeinterlacer(const QString &Filter)
{
    if (m_softwareDeiterlacer != Filter)
    {
        m_render->makeCurrent();
        CheckResize(false);
        m_render->doneCurrent();
    }
    m_softwareDeiterlacer = Filter;
}

/**
 *  Render the contents of the current input texture to the framebuffer
 *  using the currently enabled filters.
 *  \param TopFieldFirst         the frame is interlaced and top_field_first is set
 *  \param Scan                  interlaced or progressive
 *  \param Stereo                Whether/how to drop stereo video information
 *  \param DrawBorder            if true, draw a red border around the frame
 *  \warning This function is a finely tuned, sensitive beast. Tinker at
 *   your own risk.
 */

void OpenGLVideo::PrepareFrame(bool TopFieldFirst, FrameScanType Scan,
                               StereoscopicMode Stereo, bool DrawBorder)
{
    if (m_inputTextures.empty() || m_filters.empty())
        return;

    OpenGLLocker ctx_lock(m_render);

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_render->logDebugMarker(LOC + "PREP_FRAME_START");

    vector<MythGLTexture*> inputs = m_inputTextures;
    QSize inputsize = m_inputTextureSize;
    QSize realsize  = m_render->GetTextureSize(m_videoDispDim);
    QOpenGLFramebufferObject* lastFramebuffer = nullptr;

    glfilt_map_t::iterator it;
    for (it = m_filters.begin(); it != m_filters.end(); ++it)
    {
        OpenGLFilterType type = it->first;
        OpenGLFilter *filter = it->second;

        if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
            m_render->logDebugMarker(LOC + QString("FILTER_START_%1").arg(type));

        // texture coordinates
        float width = m_videoDispDim.width();
        if (kGLUYVY == m_frameType)
            width /= 2.0f;
        QRect trect(QPoint(0, 0), QSize(width, m_videoDispDim.height()));

        // only apply overscan on last filter
        if (filter->outputBuffer == kDefaultBuffer)
            trect = m_videoRect;

        // discard stereoscopic fields
        if (filter->outputBuffer == kDefaultBuffer)
        {
            if (kStereoscopicModeSideBySideDiscard == Stereo)
                trect = QRect(trect.left() >> 1,  trect.top(),
                              trect.width() >> 1, trect.height());
            if (kStereoscopicModeTopAndBottomDiscard == Stereo)
                trect = QRect(trect.left(),  trect.top() >> 1,
                              trect.width(), trect.height() >> 1);
        }

        // vertex coordinates
        QRect vrect = (filter->outputBuffer == kDefaultBuffer) ? m_displayVideoRect : QRect(QPoint(0,0), m_videoDispDim);

        // bind correct frame buffer (default onscreen) and set viewport
        QOpenGLFramebufferObject *target = nullptr;
        switch (filter->outputBuffer)
        {
            case kDefaultBuffer:
                m_render->BindFramebuffer(target);
                if (m_viewportControl)
                    m_render->SetViewPort(QRect(QPoint(), m_displayVisibleRect.size()));
                else
                    m_render->SetViewPort(QRect(QPoint(), m_masterViewportSize));
                break;
            case kFrameBufferObject:
                if (!filter->frameBuffers.empty())
                {
                    target = filter->frameBuffers[0];
                    m_render->BindFramebuffer(target);
                    m_render->SetViewPort(QRect(QPoint(0, 0), m_videoDispDim));
                }
                break;
            default:
                continue;
        }

        if (DrawBorder && filter->outputBuffer == kDefaultBuffer)
        {
            QRectF piprectf = vrect.adjusted(-10, -10, +10, +10);
            QRect  piprect(piprectf.left(), piprectf.top(),
                           piprectf.width(), piprectf.height());
            static const QPen nopen(Qt::NoPen);
            static const QBrush redbrush(QBrush(QColor(127, 0, 0, 255)));
            m_render->DrawRect(target, piprect, redbrush, nopen, 255);
        }

        // bind correct textures
        MythGLTexture* textures[4]; // NB
        uint texture_count = 0;
        for (uint i = 0; i < inputs.size(); i++)
            textures[texture_count++] = inputs[i];

        if (!m_referenceTextures.empty() &&
            m_hardwareDeinterlacing &&
            (type == kGLFilterYUV2RGB || type == kGLFilterYV12RGB))
        {
            for (uint i = 0; i < m_referenceTextures.size(); i++)
                textures[texture_count++] = m_referenceTextures[i];
        }

        // enable fragment program
        QOpenGLShaderProgram *program = filter->fragmentPrograms[0];
        if (type == kGLFilterYUV2RGB || type == kGLFilterYV12RGB)
        {
            if (m_hardwareDeinterlacing &&
                filter->fragmentPrograms.size() == 3 &&
                !m_referenceTexturesNeeded)
            {
                if (Scan == kScan_Interlaced)
                    program = filter->fragmentPrograms[TopFieldFirst ? 1 : 2];
                else if (Scan == kScan_Intr2ndField)
                    program = filter->fragmentPrograms[TopFieldFirst ? 2 : 1];
            }
        }

        m_render->DrawBitmap(textures, texture_count, target, trect, vrect,
                               program);


        inputs = filter->frameBufferTextures;
        inputsize = realsize;

        if (m_extraFeatures & kGLExtFBDiscard)
        {
            // If lastFramebuffer is set, it was from the last filter stage and
            // has now been used to render. It can be discarded.
            // If this is the last filter, we have rendered to the default framebuffer
            // and it can be discarded.
            if (lastFramebuffer)
                m_render->DiscardFramebuffer(lastFramebuffer);
            else if (filter->outputBuffer == kDefaultBuffer)
                m_render->DiscardFramebuffer(nullptr);
            lastFramebuffer = target;
        }
    }

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_render->logDebugMarker(LOC + "PREP_FRAME_END");
}

void OpenGLVideo::RotateTextures(void)
{
    if (m_referenceTextures.size() < 2)
        return;

    if (m_referenceTexturesNeeded > 0)
        m_referenceTexturesNeeded--;

    MythGLTexture *tmp = m_referenceTextures[m_referenceTextures.size() - 1];

    for (uint i = m_referenceTextures.size() - 1; i > 0;  i--)
        m_referenceTextures[i] = m_referenceTextures[i - 1];

    m_referenceTextures[0] = m_inputTextures[0];
    m_inputTextures[0] = tmp;
}

void OpenGLVideo::DeleteTextures(vector<MythGLTexture*> *textures)
{
    if (textures->empty())
        return;

    for (uint i = 0; i < textures->size(); i++)
        m_render->DeleteTexture((*textures)[i]);
    textures->clear();
}

void OpenGLVideo::SetTextureFilters(vector<MythGLTexture *> *Textures,
                                    QOpenGLTexture::Filter Filter,
                                    QOpenGLTexture::WrapMode Wrap)
{
    if (!Textures || (Textures && Textures->empty()))
        return;
    for (uint i = 0; i < Textures->size(); i++)
        m_render->SetTextureFilters((*Textures)[i], Filter, Wrap);
}

OpenGLVideo::FrameType OpenGLVideo::StringToType(const QString &Type)
{
    QString type = Type.toLower().trimmed();
    if ("opengl-yv12" == type)  return kGLYV12;
    if ("opengl-hquyv" == type) return kGLHQUYV;
    if ("opengl-rgba" == type)  return kGLRGBA;
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
        case kGLRGBA:  return "opengl-rgba";
    }
    return "opengl";
}
