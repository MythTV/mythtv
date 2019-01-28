// MythTV headers
#include "openglvideo.h"
#include "mythcontext.h"
#include "tv.h"
#include "mythrender_opengl.h"
#include "mythavutil.h"
#include "openglvideoshaders.h"

#define LOC QString("GLVid: ")
#define COLOUR_UNIFORM "m_colourMatrix"
#define MYTHTV_YV12 0x8a20

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

OpenGLVideo::OpenGLVideo() :
    videoType(kGLUYVY),
    gl_context(nullptr),
    video_disp_dim(0,0),
    video_dim(0,0),
    viewportSize(0,0),
    masterViewportSize(0,0),
    display_visible_rect(0,0,0,0),
    display_video_rect(0,0,0,0),
    video_rect(0,0,0,0),
    frameBufferRect(0,0,0,0),
    hardwareDeinterlacing(false),
    colourSpace(nullptr),
    viewportControl(false),
    inputTextureSize(0,0),
    refsNeeded(0),
    textureType(GL_TEXTURE_2D),
    m_extraFeatures(0),
    forceResize(false)
{
    forceResize = gCoreContext->GetBoolSetting("OpenGLExtraStage", false);
    discardFramebuffers = gCoreContext->GetBoolSetting("OpenGLDiscardFB", false);
    enablePBOs = gCoreContext->GetBoolSetting("OpenGLEnablePBO", true);
}

OpenGLVideo::~OpenGLVideo()
{
    OpenGLLocker ctx_lock(gl_context);
    Teardown();
}

void OpenGLVideo::Teardown(void)
{
    if (viewportControl)
        gl_context->DeleteFence();

    DeleteTextures(&inputTextures);
    DeleteTextures(&referenceTextures);

    while (!filters.empty())
        RemoveFilter(filters.begin()->first);
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

bool OpenGLVideo::Init(MythRenderOpenGL *glcontext, VideoColourSpace *colourspace,
                       QSize videoDim, QSize videoDispDim,
                       QRect displayVisibleRect,
                       QRect displayVideoRect, QRect videoRect,
                       bool viewport_control,
                       VideoType Type)
{
    if (!glcontext)
        return false;

    gl_context            = glcontext;
    OpenGLLocker ctx_lock(gl_context);

    video_dim             = videoDim;
    video_disp_dim        = videoDispDim;
    display_visible_rect  = displayVisibleRect;
    display_video_rect    = displayVideoRect;
    video_rect            = videoRect;
    masterViewportSize    = display_visible_rect.size();
    frameBufferRect       = QRect(QPoint(0,0), video_disp_dim);
    softwareDeinterlacer  = "";
    hardwareDeinterlacing = false;
    colourSpace           = colourspace;
    viewportControl       = viewport_control;
    inputTextureSize      = QSize(0,0);
    videoType             = Type;

    // Set OpenGL feature support
    m_features      = gl_context->GetFeatures();
    m_extraFeatures = gl_context->GetExtraFeatures();

    // Enable/Disable certain features based on settings
    if (gl_context->isOpenGLES())
    {
        if ((m_extraFeatures & kGLExtFBDiscard) && discardFramebuffers)
            LOG(VB_GENERAL, LOG_INFO, LOC + "Enabling Framebuffer discards");
        else
            m_extraFeatures &= ~kGLExtFBDiscard;
    }

    if (viewportControl)
        gl_context->SetFence();

    SetViewPort(masterViewportSize);

    bool glsl  = m_features & QOpenGLFunctions::Shaders;
    bool fbos  = m_features & QOpenGLFunctions::Framebuffers;
    VideoType fallback = glsl ? (fbos ? kGLUYVY : kGLYV12) : kGLRGBA;

    // check for feature support
    bool unsupported = (kGLYV12  == videoType) && !glsl;
    unsupported     |= (kGLUYVY  == videoType) && (!glsl || !fbos);
    unsupported     |= (kGLHQUYV == videoType) && !glsl;

    if (unsupported)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + QString(
                "'%1' OpenGLVideo type not available - falling back to '%2'")
                .arg(TypeToString(videoType)).arg(TypeToString(fallback)));
        videoType = fallback;
    }

    // check picture attributes support - colourspace adjustments require
    // shaders to operate on YUV textures
    if (kGLRGBA == videoType || kGLGPU == videoType)
        colourspace->SetSupportedAttributes(kPictureAttributeSupported_None);

    // Create initial input texture and associated filter stage
    MythGLTexture *tex = CreateVideoTexture(video_dim, inputTextureSize);
    bool    ok = false;

    if (kGLYV12 == videoType)
        ok = tex && AddFilter(kGLFilterYV12RGB);
    else if ((kGLUYVY == videoType) || (kGLHQUYV == videoType))
        ok = tex && AddFilter(kGLFilterYUV2RGB);
    else
        ok = tex && AddFilter(kGLFilterResize);

    if (ok)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Using '%1' for OpenGL video type")
            .arg(TypeToString(videoType)));
        inputTextures.push_back(tex);
    }
    else
    {
        Teardown();
    }

    if (filters.empty())
    {
        bool fatalerror = kGLRGBA == videoType;

        if (!fatalerror)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                    "Failed to setup colourspace conversion.\n\t\t\t"
                    "Falling back to software conversion.\n\t\t\t"
                    "Any opengl filters will also be disabled.");
            MythGLTexture *rgba32tex = CreateVideoTexture(video_dim, inputTextureSize);

            if (rgba32tex && AddFilter(kGLFilterResize))
            {
                inputTextures.push_back(rgba32tex);
                colourSpace->SetSupportedAttributes(kPictureAttributeSupported_None);
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

    bool mmx = false;
    bool neon = false;
#ifdef MMX
    // cppcheck-suppress redundantAssignment
    mmx = true;
#endif
#ifdef HAVE_NEON
    neon = true;
#endif

    CheckResize(false);
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("MMX: %1 NEON: %2 ForceResize: %3")
            .arg(mmx).arg(neon).arg(forceResize));
    return true;
}

/**
 *   Determine if the output is to be scaled at all and create or destroy
 *   the appropriate filter as necessary.
 */
void OpenGLVideo::CheckResize(bool deinterlacing)
{
    // to improve performance on slower cards when deinterlacing
    bool resize = ((video_disp_dim.height() < display_video_rect.height()) ||
                   (video_disp_dim.width() < display_video_rect.width()));

    // to ensure deinterlacing works correctly
    resize |= (video_disp_dim.height() > display_video_rect.height()) &&
                        deinterlacing;

    // UYVY packed pixels must be sampled exactly and any overscan settings will
    // break sampling - so always force an extra stage
    // TODO optimise this away when 1 for 1 mapping is guaranteed
    resize |= (kGLUYVY == videoType);

    // we always need at least one filter (i.e. a resize that simply blits the texture
    // to screen)
    resize |= !filters.count(kGLFilterYUV2RGB) && !filters.count(kGLFilterYV12RGB);

    // Extra stage needed on Fire Stick 4k, maybe others, because of blank screen when playing.
    resize |= forceResize;

    if (resize)
        AddFilter(kGLFilterResize);
    else
        RemoveFilter(kGLFilterResize);
    OptimiseFilters();
}

void OpenGLVideo::SetVideoRect(const QRect &dispvidrect, const QRect &vidrect)
{
    if (vidrect == video_rect && dispvidrect == display_video_rect)
        return;

    display_video_rect = dispvidrect;
    video_rect = vidrect;
    gl_context->makeCurrent();
    CheckResize(hardwareDeinterlacing);
    gl_context->doneCurrent();
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
    for (it = filters.rbegin(); it != filters.rend(); ++it)
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
                    QOpenGLFramebufferObject* framebuffer = gl_context->CreateFramebuffer(video_disp_dim);
                    if (framebuffer)
                    {
                        MythGLTexture *texture = gl_context->CreateFramebufferTexture(framebuffer);
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
                    gl_context->DeleteFramebuffer(filt->frameBuffers.back());
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
    if (filters.size() < 2)
    {
        SetTextureFilters(&inputTextures, QOpenGLTexture::Linear, QOpenGLTexture::ClampToEdge);
        SetTextureFilters(&referenceTextures, QOpenGLTexture::Linear, QOpenGLTexture::ClampToEdge);
        return;
    }

    SetTextureFilters(&inputTextures, QOpenGLTexture::Nearest, QOpenGLTexture::ClampToEdge);
    SetTextureFilters(&referenceTextures, QOpenGLTexture::Nearest, QOpenGLTexture::ClampToEdge);

    glfilt_map_t::reverse_iterator rit;
    int last_filter = 0;

    for (rit = filters.rbegin(); rit != filters.rend(); ++rit)
    {
        if (last_filter == 1)
        {
            foreach (QOpenGLFramebufferObject* fb, rit->second->frameBuffers)
                gl_context->SetTextureFilters(fb->texture(), QOpenGLTexture::Linear, QOpenGLTexture::ClampToEdge);
        }
        else if (last_filter > 1)
        {
            foreach (QOpenGLFramebufferObject* fb, rit->second->frameBuffers)
                gl_context->SetTextureFilters(fb->texture(), QOpenGLTexture::Nearest, QOpenGLTexture::ClampToEdge);
        }
        ++last_filter;
    }
}

/**
 *  Add a new filter stage and create any additional resources needed.
 */

bool OpenGLVideo::AddFilter(OpenGLFilterType filter)
{
    if (filters.count(filter))
        return true;

    switch (filter)
    {
      case kGLFilterNone:
          // Nothing to do. Prevents compiler warning.
          break;

      case kGLFilterResize:
        if (!(m_features & QOpenGLFunctions::Framebuffers) && !filters.empty())
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC + "Framebuffers not available for scaling/resizing filter.");
            return false;
        }
        break;

      case kGLFilterYUV2RGB:
        if (!(m_features & QOpenGLFunctions::Framebuffers))
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC +
                "No shader support for OpenGL deinterlacing.");
            return false;
        }
        break;

      case kGLFilterYV12RGB:
        if (!(m_features & QOpenGLFunctions::Framebuffers))
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC +
                "No shader support for OpenGL deinterlacing.");
            return false;
        }
        break;
    }

    bool success = true;

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Creating %1 filter.")
            .arg(FilterToString(filter)));

    OpenGLFilter *temp = new OpenGLFilter();

    temp->numInputs = 1;
    QOpenGLShaderProgram *program = nullptr;

    if (success &&
        (((filter != kGLFilterNone) && (filter != kGLFilterResize)) ||
         ((m_features & QOpenGLFunctions::Framebuffers) && (filter == kGLFilterResize))))
    {
        program = AddFragmentProgram(filter);
        if (!program)
            success = false;
        else
            temp->fragmentPrograms.push_back(program);
    }

    if (success)
    {
        temp->outputBuffer = kDefaultBuffer;
        temp->frameBuffers.clear();
        filters[filter] = temp;
        temp = nullptr;
        success &= OptimiseFilters();
    }

    if (!success)
    {
        RemoveFilter(filter);
        delete temp;
        return false;
    }

    return true;
}

bool OpenGLVideo::RemoveFilter(OpenGLFilterType filter)
{
    if (!filters.count(filter))
        return true;

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Removing %1 filter")
            .arg(FilterToString(filter)));

    vector<QOpenGLShaderProgram*>::const_iterator it = filters[filter]->fragmentPrograms.cbegin();
    for ( ; it != filters[filter]->fragmentPrograms.cend(); ++it)
        gl_context->DeleteShaderProgram(*it);
    filters[filter]->fragmentPrograms.clear();

    vector<QOpenGLFramebufferObject*>::const_iterator it2 = filters[filter]->frameBuffers.cbegin();
    for ( ; it2 != filters[filter]->frameBuffers.cend(); ++it2)
        gl_context->DeleteFramebuffer(*it2);
    filters[filter]->frameBuffers.clear();

    vector<MythGLTexture*>::const_iterator it3 = filters[filter]->frameBufferTextures.cbegin();
    for ( ; it3 != filters[filter]->frameBufferTextures.cend(); ++it3)
        gl_context->DeleteTexture(*it3);
    filters[filter]->frameBufferTextures.clear();

    delete filters[filter];
    filters.erase(filter);
    return true;
}

void OpenGLVideo::TearDownDeinterlacer(void)
{
    glfilt_map_t::const_iterator it = filters.cbegin();
    for ( ; it != filters.cend(); ++it)
    {
        if (it->first != kGLFilterYUV2RGB && it->first != kGLFilterYV12RGB)
            continue;

        OpenGLFilter *tmp = it->second;
        while (tmp->fragmentPrograms.size() > 1)
        {
            gl_context->DeleteShaderProgram(tmp->fragmentPrograms.back());
            tmp->fragmentPrograms.pop_back();
        }
    }

    DeleteTextures(&referenceTextures);
    refsNeeded = 0;
}

/**
 *  Extends the functionality of the basic YUV->RGB filter stage to include
 *  deinterlacing (combining the stages is significantly more efficient than
 *  2 separate stages). Create 2 deinterlacing fragment programs, 1 for each
 *  required field.
 */

bool OpenGLVideo::AddDeinterlacer(const QString &deinterlacer)
{
    if (!(m_features & QOpenGLFunctions::Framebuffers))
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC + "No shader support for OpenGL deinterlacing.");
        return false;
    }

    OpenGLLocker ctx_lock(gl_context);

    if (filters.end() == filters.find(kGLFilterYUV2RGB) &&
        filters.end() == filters.find(kGLFilterYV12RGB) )
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC +
            "No YUV2RGB filter stage for OpenGL deinterlacing.");
        return false;
    }

    if (hardwareDeinterlacer == deinterlacer)
        return true;

    TearDownDeinterlacer();

    bool success = true;

    uint ref_size = 2;

    if (deinterlacer == "openglbobdeint" ||
        deinterlacer == "openglonefield" ||
        deinterlacer == "opengllinearblend" ||
        deinterlacer == "opengldoubleratelinearblend" ||
        deinterlacer == "opengldoubleratefieldorder")
    {
        ref_size = 0;
    }

    refsNeeded = ref_size;
    if (ref_size > 0)
    {
        for (; ref_size > 0; ref_size--)
        {
            MythGLTexture *tex = CreateVideoTexture(video_dim, inputTextureSize);
            if (tex)
                referenceTextures.push_back(tex);
            else
                success = false;
        }
    }

    OpenGLFilterType type = (kGLYV12 == videoType) ? kGLFilterYV12RGB : kGLFilterYUV2RGB;

    QOpenGLShaderProgram *prog1 = AddFragmentProgram(type, deinterlacer, kScan_Interlaced);
    QOpenGLShaderProgram *prog2 = AddFragmentProgram(type, deinterlacer, kScan_Intr2ndField);

    if (prog1 && prog2)
    {
        filters[type]->fragmentPrograms.push_back(prog1);
        filters[type]->fragmentPrograms.push_back(prog2);
    }
    else
    {
        success = false;
    }

    if (success)
    {
        CheckResize(hardwareDeinterlacing);
        hardwareDeinterlacer = deinterlacer;
        return true;
    }

    hardwareDeinterlacer = "";
    TearDownDeinterlacer();

    return false;
}

/**
 *  Create the correct fragment program for the given filter type
 */

QOpenGLShaderProgram* OpenGLVideo::AddFragmentProgram(OpenGLFilterType name,
                                     QString deint, FrameScanType field)
{
    if (!gl_context)
        return nullptr;

    QString vertex, fragment;
    if (m_features & QOpenGLFunctions::Framebuffers)
    {
        GetProgramStrings(vertex, fragment, name, deint, field);
    }
    else
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC + "No OpenGL GLSL support");
        return nullptr;
    }

    return gl_context->CreateShaderProgram(vertex, fragment);
}

void OpenGLVideo::SetViewPort(const QSize &viewPortSize)
{
    uint w = max(viewPortSize.width(),  video_disp_dim.width());
    uint h = max(viewPortSize.height(), video_disp_dim.height());

    viewportSize = QSize(w, h);

    if (!viewportControl)
        return;

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Viewport: %1x%2") .arg(w).arg(h));
    gl_context->SetViewPort(QRect(QPoint(),viewportSize));
}

/**
 *  Create and initialise an OpenGL texture suitable for a YV12 video frame
 *  of the given size.
 */
MythGLTexture* OpenGLVideo::CreateVideoTexture(QSize size, QSize &tex_size)
{
    MythGLTexture *texture = nullptr;
    if (kGLYV12 == videoType)
    {
        size.setHeight((3 * size.height() + 1) / 2);
        texture = gl_context->CreateTexture(size,
            QOpenGLTexture::UInt8,  QOpenGLTexture::Luminance,
            QOpenGLTexture::Linear, QOpenGLTexture::ClampToEdge,
            QOpenGLTexture::LuminanceFormat);
    }
    else if (kGLUYVY == videoType)
    {
        size.setWidth(size.width() >> 1);
        texture = gl_context->CreateTexture(size);
    }
    else if ((kGLHQUYV == videoType) || (kGLGPU == videoType) || (kGLRGBA == videoType))
    {
        texture = gl_context->CreateTexture(size);
    }

    tex_size = gl_context->GetTextureSize(size);
    return texture;
}

MythGLTexture* OpenGLVideo::GetInputTexture(void) const
{
    return inputTextures[0];
}

uint OpenGLVideo::GetTextureType(void) const
{
    return textureType;
}

/*! \brief Update the current input texture using the data from the given YV12 video
 *  frame.
 */
void OpenGLVideo::UpdateInputFrame(const VideoFrame *frame)
{
    OpenGLLocker ctx_lock(gl_context);

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        gl_context->logDebugMarker(LOC + "UPDATE_FRAME_START");

    if (frame->width  != video_dim.width()  ||
        frame->height != video_dim.height() ||
        frame->width  < 1 || frame->height < 1 ||
        frame->codec != FMT_YV12 || (kGLGPU == videoType))
    {
        return;
    }
    if (hardwareDeinterlacing)
        RotateTextures();

    MythGLTexture *texture = inputTextures[0];
    if (!texture)
        return;

    void* buf = nullptr;
    if ((kGLYV12 == videoType) && (video_dim.width() == frame->pitches[0]))
    {
        buf = frame->buf;
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

        if (kGLYV12 == videoType)
        {
            copybuffer((uint8_t*)buf, frame, video_dim.width());
        }
        else if (kGLUYVY == videoType)
        {
            AVFrame img_out;
            m_copyCtx.Copy(&img_out, frame, (unsigned char*)buf, AV_PIX_FMT_UYVY422);
        }
        else if (kGLRGBA == videoType)
        {
            AVFrame img_out;
            m_copyCtx.Copy(&img_out, frame, (unsigned char*)buf, AV_PIX_FMT_RGBA);
        }
        else if (kGLHQUYV == videoType && frame->interlaced_frame)
        {
            pack_yv12interlaced(frame->buf, (unsigned char*)buf, frame->offsets,
                                frame->pitches, video_dim);
        }
        else if (kGLHQUYV == videoType)
        {
            pack_yv12progressive(frame->buf, (unsigned char*)buf, frame->offsets,
                                 frame->pitches, video_dim);
        }
    }

    texture->m_texture->setData(texture->m_pixelFormat, texture->m_pixelType, buf);

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        gl_context->logDebugMarker(LOC + "UPDATE_FRAME_END");
}

void OpenGLVideo::SetDeinterlacing(bool deinterlacing)
{
    hardwareDeinterlacing = deinterlacing;
    OpenGLLocker ctx_lock(gl_context);
    CheckResize(hardwareDeinterlacing);
}

void OpenGLVideo::SetSoftwareDeinterlacer(const QString &filter)
{
    if (softwareDeinterlacer != filter)
    {
        gl_context->makeCurrent();
        CheckResize(false);
        gl_context->doneCurrent();
    }
    softwareDeinterlacer = filter;
}

/**
 *  Render the contents of the current input texture to the framebuffer
 *  using the currently enabled filters.
 *  \param topfieldfirst         the frame is interlaced and top_field_first
 *                               is set
 *  \param scan                  interlaced or progressive?
 *  \param softwareDeinterlacing the frame has been deinterlaced in software
 *  \param frame                 the frame number
 *  \param stereo                Whether/how to drop stereo video information
 *  \param draw_border           if true, draw a red border around the frame
 *  \warning This function is a finely tuned, sensitive beast. Tinker at
 *   your own risk.
 */

void OpenGLVideo::PrepareFrame(bool topfieldfirst, FrameScanType scan,
                               long long, StereoscopicMode stereo,
                               bool draw_border)
{
    if (inputTextures.empty() || filters.empty())
        return;

    OpenGLLocker ctx_lock(gl_context);

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        gl_context->logDebugMarker(LOC + "PREP_FRAME_START");

    vector<MythGLTexture*> inputs = inputTextures;
    QSize inputsize = inputTextureSize;
    QSize realsize  = gl_context->GetTextureSize(video_disp_dim);
    QOpenGLFramebufferObject* lastFramebuffer = nullptr;

    glfilt_map_t::iterator it;
    for (it = filters.begin(); it != filters.end(); ++it)
    {
        OpenGLFilterType type = it->first;
        OpenGLFilter *filter = it->second;

        if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
            gl_context->logDebugMarker(LOC + QString("FILTER_START_%1").arg(FilterToString(type)));

        // texture coordinates
        float width = video_disp_dim.width();
        if (kGLUYVY == videoType)
            width /= 2.0f;
        QRect trect(QPoint(0, 0), QSize(width, video_disp_dim.height()));

        // only apply overscan on last filter
        if (filter->outputBuffer == kDefaultBuffer)
            trect = video_rect;

        // discard stereoscopic fields
        if (filter->outputBuffer == kDefaultBuffer)
        {
            if (kStereoscopicModeSideBySideDiscard == stereo)
                trect = QRect(trect.left() >> 1,  trect.top(),
                              trect.width() >> 1, trect.height());
            if (kStereoscopicModeTopAndBottomDiscard == stereo)
                trect = QRect(trect.left(),  trect.top() >> 1,
                              trect.width(), trect.height() >> 1);
        }

        // vertex coordinates
        QRect vrect = (filter->outputBuffer == kDefaultBuffer) ? display_video_rect : frameBufferRect;

        // bind correct frame buffer (default onscreen) and set viewport
        QOpenGLFramebufferObject *target = nullptr;
        switch (filter->outputBuffer)
        {
            case kDefaultBuffer:
                gl_context->BindFramebuffer(target);
                if (viewportControl)
                    gl_context->SetViewPort(QRect(QPoint(), display_visible_rect.size()));
                else
                    gl_context->SetViewPort(QRect(QPoint(), masterViewportSize));
                break;
            case kFrameBufferObject:
                if (!filter->frameBuffers.empty())
                {
                    target = filter->frameBuffers[0];
                    gl_context->BindFramebuffer(target);
                    gl_context->SetViewPort(QRect(QPoint(), frameBufferRect.size()));
                }
                break;
            default:
                continue;
        }

        if (draw_border && filter->outputBuffer == kDefaultBuffer)
        {
            QRectF piprectf = vrect.adjusted(-10, -10, +10, +10);
            QRect  piprect(piprectf.left(), piprectf.top(),
                           piprectf.width(), piprectf.height());
            static const QPen nopen(Qt::NoPen);
            static const QBrush redbrush(QBrush(QColor(127, 0, 0, 255)));
            gl_context->DrawRect(target, piprect, redbrush, nopen, 255);
        }

        // bind correct textures
        MythGLTexture* textures[4]; // NB
        uint texture_count = 0;
        for (uint i = 0; i < inputs.size(); i++)
            textures[texture_count++] = inputs[i];

        if (!referenceTextures.empty() &&
            hardwareDeinterlacing &&
            (type == kGLFilterYUV2RGB || type == kGLFilterYV12RGB))
        {
            for (uint i = 0; i < referenceTextures.size(); i++)
                textures[texture_count++] = referenceTextures[i];
        }

        // enable fragment program and set any environment variables
        QOpenGLShaderProgram *program = nullptr;
        if (((type != kGLFilterNone) && (type != kGLFilterResize)) ||
            ((m_features & QOpenGLFunctions::Framebuffers) && (type == kGLFilterResize)))
        {
            GLuint prog_ref = 0;

            if (type == kGLFilterYUV2RGB || type == kGLFilterYV12RGB)
            {
                if (hardwareDeinterlacing &&
                    filter->fragmentPrograms.size() == 3 &&
                    !refsNeeded)
                {
                    if (scan == kScan_Interlaced)
                        prog_ref = topfieldfirst ? 1 : 2;
                    else if (scan == kScan_Intr2ndField)
                        prog_ref = topfieldfirst ? 2 : 1;
                }
            }
            program = filter->fragmentPrograms[prog_ref];
        }

        if ((type == kGLFilterYUV2RGB || type == kGLFilterYV12RGB))
        {
            gl_context->SetShaderProgramParams(program, *colourSpace, COLOUR_UNIFORM);
        }

        gl_context->DrawBitmap(textures, texture_count, target, trect, vrect,
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
                gl_context->DiscardFramebuffer(lastFramebuffer);
            else if (filter->outputBuffer == kDefaultBuffer)
                gl_context->DiscardFramebuffer(nullptr);
            lastFramebuffer = target;
        }
    }

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        gl_context->logDebugMarker(LOC + "PREP_FRAME_END");
}

void OpenGLVideo::RotateTextures(void)
{
    if (referenceTextures.size() < 2)
        return;

    if (refsNeeded > 0)
        refsNeeded--;

    MythGLTexture *tmp = referenceTextures[referenceTextures.size() - 1];

    for (uint i = referenceTextures.size() - 1; i > 0;  i--)
        referenceTextures[i] = referenceTextures[i - 1];

    referenceTextures[0] = inputTextures[0];
    inputTextures[0] = tmp;
}

void OpenGLVideo::DeleteTextures(vector<MythGLTexture*> *textures)
{
    if (textures->empty())
        return;

    for (uint i = 0; i < textures->size(); i++)
        gl_context->DeleteTexture((*textures)[i]);
    textures->clear();
}

void OpenGLVideo::SetTextureFilters(vector<MythGLTexture *> *Textures,
                                    QOpenGLTexture::Filter Filter,
                                    QOpenGLTexture::WrapMode Wrap)
{
    if (!Textures || (Textures && Textures->empty()))
        return;
    for (uint i = 0; i < Textures->size(); i++)
        gl_context->SetTextureFilters((*Textures)[i], Filter, Wrap);
}

OpenGLVideo::OpenGLFilterType OpenGLVideo::StringToFilter(const QString &filter)
{
    OpenGLFilterType ret = kGLFilterNone;

    if (filter.contains("yuv2rgb"))
        ret = kGLFilterYUV2RGB;
    else if (filter.contains("resize"))
        ret = kGLFilterResize;
    else if (filter.contains("yv12rgb"))
        ret = kGLFilterYV12RGB;

    return ret;
}

QString OpenGLVideo::FilterToString(OpenGLFilterType filt)
{
    switch (filt)
    {
        case kGLFilterNone:
            break;
        case kGLFilterYUV2RGB:
            return "yuv2rgb";
        case kGLFilterResize:
            return "resize";
        case kGLFilterYV12RGB:
            return "yv12rgb";
    }

    return "";
}

OpenGLVideo::VideoType OpenGLVideo::StringToType(const QString &Type)
{
    QString type = Type.toLower().trimmed();
    if ("opengl-yv12" == type)  return kGLYV12;
    if ("opengl-hquyv" == type) return kGLHQUYV;
    if ("opengl-rgba" == type)  return kGLRGBA;
    if ("opengl-gpu" == type)   return kGLGPU;
    return kGLUYVY; // opengl
}

QString OpenGLVideo::TypeToString(VideoType Type)
{
    switch (Type)
    {
        case kGLGPU:   return "opengl-gpu";
        case kGLUYVY:  return "opengl";      // compatibility with old profiles
        case kGLYV12:  return "opengl-yv12";
        case kGLHQUYV: return "opengl-hquyv";
        case kGLRGBA:  return "opengl-rgba";
    }
    return "opengl";
}

void OpenGLVideo::CustomiseProgramString(QString &string)
{
    if (inputTextureSize.isEmpty())
        return;

    qreal lineHeight = 1.0 / inputTextureSize.height();
    qreal colWidth   = 1.0 / inputTextureSize.width();
    qreal yselect    = (qreal)inputTextureSize.width();
    qreal maxheight  = video_disp_dim.height() / (qreal)inputTextureSize.height();
    QSize fb_size    = gl_context->GetTextureSize(video_disp_dim);
    qreal fieldSize  = inputTextureSize.height() / 2.0;

    string.replace("%FIELDHEIGHT%",   QString::number(fieldSize, 'f', 16));
    string.replace("%LINEHEIGHT%",    QString::number(lineHeight, 'f', 16));
    string.replace("%2LINEHEIGHT%",   QString::number(lineHeight * 2.0, 'f', 16));
    string.replace("%BICUBICCWIDTH%", QString::number(colWidth, 'f', 16));
    string.replace("%BICUBICWIDTH%",  QString::number((qreal)fb_size.width(), 'f', 8));
    string.replace("%BICUBICHEIGHT%", QString::number((qreal)fb_size.height(), 'f', 8));
    string.replace("%SELECTCOL%",     QString::number(yselect, 'f', 16));
    string.replace("%MAXHEIGHT%",     QString::number(maxheight - lineHeight, 'f', 16));
    string.replace("COLOUR_UNIFORM",  COLOUR_UNIFORM);
    // TODO fix alternative swizzling by changing the YUVA packing code
    string.replace("%SWIZZLE%", kGLUYVY == videoType ? "arb" : "abr");
}

void OpenGLVideo::GetProgramStrings(QString &vertex, QString &fragment,
                                    OpenGLFilterType filter,
                                    QString deint, FrameScanType field)
{
    uint bottom = field == kScan_Intr2ndField;
    vertex = YUV2RGBVertexShader;
    switch (filter)
    {
        case kGLFilterYUV2RGB:
        {
            if (deint == "openglonefield" || deint == "openglbobdeint")
                fragment = OneFieldShader[bottom];
            else if (deint == "opengllinearblend" ||
                     deint == "opengldoubleratelinearblend")
                fragment = LinearBlendShader[bottom];
            else if (deint == "openglkerneldeint" ||
                     deint == "opengldoubleratekerneldeint")
                fragment = KernelShader[bottom];
            else
                fragment = YUV2RGBFragmentShader;
            fragment.replace("SELECT_COLUMN", (kGLUYVY == videoType) ? SelectColumn : "");
            break;
        }
        case kGLFilterYV12RGB:
            vertex = YV12RGBVertexShader;
            if (deint == "openglonefield" || deint == "openglbobdeint")
                fragment = YV12RGBOneFieldFragmentShader[bottom];
            else if (deint == "opengllinearblend" || deint == "opengldoubleratelinearblend")
                fragment = YV12RGBLinearBlendFragmentShader[bottom];
            else if (deint == "openglkerneldeint" || deint == "opengldoubleratekerneldeint")
                fragment = YV12RGBKernelShader[bottom];
            else
                fragment = YV12RGBFragmentShader;
            break;
        case kGLFilterNone:
            break;
        case kGLFilterResize:
            fragment = DefaultFragmentShader;
            break;
        default:
            LOG(VB_PLAYBACK, LOG_ERR, LOC + "Unknown filter");
            break;
    }
    CustomiseProgramString(vertex);
    CustomiseProgramString(fragment);
}
