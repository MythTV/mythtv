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
        vector<GLuint> frameBuffers;
        vector<GLuint> frameBufferTextures;
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
 *  In the most basic case, for example, a YV12 frame pre-converted in software
 *  to RGBA format is simply blitted to the frame buffer.
 *  Currently, the most complicated example is the rendering of a standard
 *  definition, interlaced frame to a high(er) definition display using
 *  OpenGL (i.e. hardware based) deinterlacing, colourspace conversion and
 *  bicubic upsampling.
 *
 *  Higher level tasks such as coordination between OpenGLVideo instances,
 *  video buffer management, audio/video synchronisation etc are handled by
 *  the higher level classes VideoOutput and NuppelVideoPlayer. The bulk of
 *  the lower level interface with the window system and OpenGL is handled by
 *  MythRenderOpenGL.
 *
 *  N.B. Direct use of OpenGL calls is minimised to maintain platform
 *  independance. The only member function where this is impractical is
 *  PrepareFrame().
 *
 *  \warning Any direct OpenGL calls must be wrapped by calls to
 *  gl_context->MakeCurrent(). Alternatively use the convenience class
 *  OpenGLLocker.
 */

/**
 *  Create a new OpenGLVideo instance that must be initialised
 *  with a call to OpenGLVideo::Init()
 */

OpenGLVideo::OpenGLVideo() :
    videoType(kGLUYVY),
    gl_context(nullptr),      video_disp_dim(0,0),
    video_dim(0,0),           viewportSize(0,0),
    masterViewportSize(0,0),  display_visible_rect(0,0,0,0),
    display_video_rect(0,0,0,0), video_rect(0,0,0,0),
    frameBufferRect(0,0,0,0), hardwareDeinterlacing(false),
    colourSpace(nullptr),     viewportControl(false),
    inputTextureSize(0,0),    refsNeeded(0),
    textureType(GL_TEXTURE_2D),
    helperTexture(0),         defaultUpsize(kGLFilterResize),
    gl_features(0),           forceResize(false)
{
    forceResize = gCoreContext->GetBoolSetting("OpenGLExtraStage", false);
}

OpenGLVideo::~OpenGLVideo()
{
    OpenGLLocker ctx_lock(gl_context);
    Teardown();
}

void OpenGLVideo::Teardown(void)
{
    if (helperTexture)
        gl_context->DeleteTexture(helperTexture);
    helperTexture = 0;

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
 *  \param options            a string defining OpenGL features to disable
 *  \param hw_accel           if true, a GPU decoder will copy frames directly
     to an RGBA texture
 */

bool OpenGLVideo::Init(MythRenderOpenGL *glcontext, VideoColourSpace *colourspace,
                       QSize videoDim, QSize videoDispDim,
                       QRect displayVisibleRect,
                       QRect displayVideoRect, QRect videoRect,
                       bool viewport_control,
                       VideoType Type,
                       QString options)
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
    gl_features = gl_context->GetFeatures();

    if (viewportControl)
        gl_context->SetFence();

    SetViewPort(masterViewportSize);

    bool glsl  = gl_features & kGLSL;
    bool fbos  = gl_features & kGLExtFBufObj;
    bool ycbcr = (gl_features & kGLMesaYCbCr) || (gl_features & kGLAppleYCbCr);
    VideoType fallback = glsl ? (fbos ? kGLUYVY : kGLYV12) : ycbcr ? kGLYCbCr : kGLRGBA;

    // check for feature support
    bool unsupported = (kGLYCbCr == videoType) && !ycbcr;
    unsupported     |= (kGLYV12  == videoType) && !glsl;
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
    if (kGLRGBA == videoType || kGLGPU == videoType || kGLYCbCr == videoType)
        colourspace->SetSupportedAttributes(kPictureAttributeSupported_None);

    // turn on bicubic filtering
    if (options.contains("openglbicubic"))
    {
        if (glsl && fbos && (gl_features & kGLExtRGBA16))
            defaultUpsize = kGLFilterBicubic;
        else
            LOG(VB_PLAYBACK, LOG_ERR, LOC + "No OpenGL feature support for Bicubic filter.");
    }

    // Create initial input texture and associated filter stage
    GLuint tex = CreateVideoTexture(video_dim, inputTextureSize);
    bool    ok = false;

    if (kGLYV12 == videoType)
        ok = tex && AddFilter(kGLFilterYV12RGB);
    else if ((kGLUYVY == videoType) || (kGLHQUYV == videoType) || (kGLYCbCr == videoType))
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
            GLuint rgba32tex = CreateVideoTexture(video_dim, inputTextureSize);

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
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("MMX: %1 NEON: %2 PBO: %3 ForceResize: %4 Rects: %5")
            .arg(mmx).arg(neon).arg((gl_features & kGLExtPBufObj) > 0)
            .arg(forceResize).arg(textureType != GL_TEXTURE_2D));
    return true;
}

/**
 *   Determine if the output is to be scaled at all and create or destroy
 *   the appropriate filter as necessary.
 */
void OpenGLVideo::CheckResize(bool deinterlacing, bool allow)
{
    // to improve performance on slower cards when deinterlacing
    bool resize_up = ((video_disp_dim.height() < display_video_rect.height()) ||
                     (video_disp_dim.width() < display_video_rect.width())) && allow;

    // to ensure deinterlacing works correctly
    bool resize_down = (video_disp_dim.height() > display_video_rect.height()) &&
                        deinterlacing && allow;

    // UYVY packed pixels must be sampled exactly and any overscan settings will
    // break sampling - so always force an extra stage
    // TODO optimise this away when 1 for 1 mapping is guaranteed
    resize_down |= (kGLUYVY == videoType);

    // we always need at least one filter (i.e. a resize that simply blits the texture
    // to screen)
    resize_down |= !filters.count(kGLFilterYUV2RGB) && !filters.count(kGLFilterYV12RGB);

    // Extra stage needed on Fire Stick 4k, maybe others, because of blank screen when playing.
    resize_down |= forceResize;

    if (resize_up && (defaultUpsize == kGLFilterBicubic))
    {
        RemoveFilter(kGLFilterResize);
        AddFilter(kGLFilterBicubic);
        return;
    }

    if ((resize_up && (defaultUpsize == kGLFilterResize)) || resize_down)
    {
        RemoveFilter(kGLFilterBicubic);
        AddFilter(kGLFilterResize);
        return;
    }

    if (!resize_down)
        RemoveFilter(kGLFilterResize);

    RemoveFilter(kGLFilterBicubic);
    OptimiseFilters();
}

void OpenGLVideo::SetVideoRect(const QRect &dispvidrect, const QRect &vidrect)
{
    if (vidrect == video_rect && dispvidrect == display_video_rect)
        return;

    display_video_rect = dispvidrect;
    video_rect = vidrect;
    gl_context->makeCurrent();
    CheckResize(hardwareDeinterlacing, softwareDeinterlacer.isEmpty() ? true : softwareDeinterlacer != "bobdeint");
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
                uint tmp_buf, tmp_tex;
                for (int i = 0; i < buffers_diff; i++)
                {
                    if (!AddFrameBuffer(tmp_buf, tmp_tex, video_disp_dim))
                        return false;
                    else
                    {
                        it->second->frameBuffers.push_back(tmp_buf);
                        it->second->frameBufferTextures.push_back(tmp_tex);
                    }
                }
            }
            else if (buffers_diff < 0)
            {
                for (int i = 0; i > buffers_diff; i--)
                {
                    OpenGLFilter *filt = it->second;

                    gl_context->DeleteFrameBuffer(
                        filt->frameBuffers.back());
                    gl_context->DeleteTexture(
                        filt->frameBufferTextures.back());

                    filt->frameBuffers.pop_back();
                    filt->frameBufferTextures.pop_back();
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
        SetTextureFilters(&inputTextures, GL_LINEAR, GL_CLAMP_TO_EDGE);
        SetTextureFilters(&referenceTextures, GL_LINEAR, GL_CLAMP_TO_EDGE);
        return;
    }

    SetTextureFilters(&inputTextures, GL_NEAREST, GL_CLAMP_TO_EDGE);
    SetTextureFilters(&referenceTextures, GL_NEAREST, GL_CLAMP_TO_EDGE);

    glfilt_map_t::reverse_iterator rit;
    int last_filter = 0;

    for (rit = filters.rbegin(); rit != filters.rend(); ++rit)
    {
        if (last_filter == 1)
        {
            SetTextureFilters(&(rit->second->frameBufferTextures),
                              GL_LINEAR, GL_CLAMP_TO_EDGE);
        }
        else if (last_filter > 1)
        {
            SetTextureFilters(&(rit->second->frameBufferTextures),
                              GL_NEAREST, GL_CLAMP_TO_EDGE);
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
        if (!(gl_features & kGLExtFBufObj) && !filters.empty())
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC +
                "GL_EXT_framebuffer_object not available "
                "for scaling/resizing filter.");
            return false;
        }
        break;

      case kGLFilterBicubic:
        if (!(gl_features & kGLSL) || !(gl_features & kGLExtFBufObj))
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC + "Features not available for bicubic filter.");
            return false;
        }
        break;

      case kGLFilterYUV2RGB:
        if (!(gl_features & kGLSL))
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC +
                "No shader support for OpenGL deinterlacing.");
            return false;
        }
        break;

      case kGLFilterYV12RGB:
        if (!(gl_features & kGLSL))
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

    if (filter == kGLFilterBicubic)
    {
        if (helperTexture)
            gl_context->DeleteTexture(helperTexture);

        helperTexture = gl_context->CreateHelperTexture();
        if (!helperTexture)
            success = false;
    }

    if (success &&
        (((filter != kGLFilterNone) && (filter != kGLFilterResize)) ||
         ((gl_features & kGLSL) && (filter == kGLFilterResize))))
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
        temp->frameBufferTextures.clear();
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

    vector<GLuint>::const_iterator it2 = filters[filter]->frameBuffers.cbegin();
    for ( ; it2 != filters[filter]->frameBuffers.cend(); ++it2)
        gl_context->DeleteFrameBuffer(*it2);
    filters[filter]->frameBuffers.clear();

    DeleteTextures(&(filters[filter]->frameBufferTextures));

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
    if (!(gl_features & kGLSL))
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
            GLuint tex = CreateVideoTexture(video_dim, inputTextureSize);
            if (tex)
            {
                referenceTextures.push_back(tex);
            }
            else
            {
                success = false;
            }
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
    if (gl_features & kGLSL)
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

/**
 *  Add a FrameBuffer object of the correct size to the given texture.
 */

bool OpenGLVideo::AddFrameBuffer(uint &framebuffer,
                                 uint &texture, QSize vid_size)
{
    if (!(gl_features & kGLExtFBufObj))
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC + "Framebuffer binding not supported.");
        return false;
    }

    texture = gl_context->CreateTexture(vid_size, false, textureType);

    bool ok = gl_context->CreateFrameBuffer(framebuffer, texture);

    if (!ok)
        gl_context->DeleteTexture(texture);

    return ok;
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
uint OpenGLVideo::CreateVideoTexture(QSize size, QSize &tex_size)
{
    uint tmp_tex = 0;
    bool use_pbo = gl_features & kGLExtPBufObj;
    if (kGLYCbCr == videoType)
    {
        uint type = (gl_features & kGLMesaYCbCr) ? GL_YCBCR_MESA : GL_YCBCR_422_APPLE;
        tmp_tex = gl_context->CreateTexture(size, use_pbo, textureType,
                                            GL_UNSIGNED_SHORT_8_8_MESA,
                                            type, GL_RGBA);
    }
    else if (kGLYV12 == videoType)
    {
        size.setHeight((3 * size.height() + 1) / 2);
        tmp_tex = gl_context->CreateTexture(size, use_pbo, textureType,
                                            GL_UNSIGNED_BYTE,   // data_type
                                            GL_LUMINANCE,       // data_fmt
                                            GL_LUMINANCE        // internal_fmt
                                            );
    }
    else if (kGLUYVY == videoType)
    {
        size.setWidth(size.width() >> 1);
        tmp_tex = gl_context->CreateTexture(size, use_pbo, textureType,
                                            GL_UNSIGNED_BYTE, GL_RGBA, GL_RGBA);
    }
    else if ((kGLHQUYV == videoType) || (kGLGPU == videoType) || (kGLRGBA == videoType))
    {
        tmp_tex = gl_context->CreateTexture(size, use_pbo, textureType);
    }

    tex_size = gl_context->GetTextureSize(size);
    return tmp_tex;
}

uint OpenGLVideo::GetInputTexture(void) const
{
    return inputTextures[0];
}

uint OpenGLVideo::GetTextureType(void) const
{
    return textureType;
}

/**
 *  Update the current input texture using the data from the given YV12 video
 *  frame. If the required hardware support is not available, fall back to
 *  software YUV->RGB conversion.
 */

void OpenGLVideo::UpdateInputFrame(const VideoFrame *frame, bool soft_bob)
{
    OpenGLLocker ctx_lock(gl_context);

    if (frame->width  != video_dim.width()  ||
        frame->height != video_dim.height() ||
        frame->width  < 1 || frame->height < 1 ||
        frame->codec != FMT_YV12 || (kGLGPU == videoType))
    {
        return;
    }
    if (hardwareDeinterlacing)
        RotateTextures();

    // We need to convert frames here to avoid dependencies in MythRenderOpenGL
    void* buf = gl_context->GetTextureBuffer(inputTextures[0]);
    if (!buf)
        return;

    if (kGLYV12 == videoType)
    {
        if (gl_features & kGLExtPBufObj)
        {
            // Copy the frame to the pixel buffer which updates the texture
            copybuffer((uint8_t*)buf, frame, video_dim.width());
        }
        else if (video_dim.width() != frame->pitches[0])
        {
            // Re-packing is needed
            copybuffer((uint8_t*)buf, frame, video_dim.width());
        }
        else
        {
            // UpdateTexture will copy the frame to the texture
            buf = frame->buf;
        }
    }
    else if ((kGLYCbCr == videoType) || (kGLUYVY == videoType))
    {
        AVFrame img_out;
        m_copyCtx.Copy(&img_out, frame, (unsigned char*)buf, AV_PIX_FMT_UYVY422);
    }
    else if (kGLRGBA == videoType)
    {
        AVFrame img_out;
        m_copyCtx.Copy(&img_out, frame, (unsigned char*)buf, AV_PIX_FMT_RGBA);
    }
    else if (kGLHQUYV == videoType && frame->interlaced_frame && !soft_bob)
    {
        pack_yv12interlaced(frame->buf, (unsigned char*)buf, frame->offsets,
                            frame->pitches, video_dim);
    }
    else if (kGLHQUYV == videoType)
    {
        pack_yv12progressive(frame->buf, (unsigned char*)buf, frame->offsets,
                             frame->pitches, video_dim);
    }

    gl_context->UpdateTexture(inputTextures[0], buf);
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
        CheckResize(false, filter != "bobdeint");
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
                               bool softwareDeinterlacing,
                               long long, StereoscopicMode stereo,
                               bool draw_border)
{
    if (inputTextures.empty() || filters.empty())
        return;

    OpenGLLocker ctx_lock(gl_context);

    // we need to special case software bobdeint for 1080i
    bool softwarebob = softwareDeinterlacer == "bobdeint" &&
                       softwareDeinterlacing;

    vector<GLuint> inputs = inputTextures;
    QSize inputsize = inputTextureSize;
    QSize realsize  = gl_context->GetTextureSize(video_disp_dim);

    glfilt_map_t::iterator it;
    for (it = filters.begin(); it != filters.end(); ++it)
    {
        OpenGLFilterType type = it->first;
        OpenGLFilter *filter = it->second;

        bool actual = softwarebob && (filter->outputBuffer == kDefaultBuffer);

        // texture coordinates
        float trueheight = (float)(actual ? video_dim.height() :
                                            video_disp_dim.height());
        float width = video_disp_dim.width();
        if (kGLUYVY == videoType)
            width /= 2.0f;

        QRectF trect(QPoint(0, 0), QSize(width, trueheight));

        // only apply overscan on last filter
        if (filter->outputBuffer == kDefaultBuffer)
            trect.setCoords(video_rect.left(),  video_rect.top(),
                            video_rect.left() + video_rect.width(),
                            video_rect.top()  + video_rect.height());

        if (inputsize.height() > 0)
            trueheight /= inputsize.height();

        // software bobdeint
        if (actual)
        {
            bool top = (scan == kScan_Intr2ndField && topfieldfirst) ||
                       (scan == kScan_Interlaced && !topfieldfirst);
            bool bot = (scan == kScan_Interlaced && topfieldfirst) ||
                       (scan == kScan_Intr2ndField && !topfieldfirst);
            bool first = filters.size() < 2;
            float bob = (trueheight / (float)video_disp_dim.height()) / 4.0f;
            if ((top && !first) || (bot && first))
            {
                trect.setBottom(trect.bottom() / 2);
                trect.setTop(trect.top() / 2);
                trect.adjust(0, bob, 0, bob);
            }
            if ((bot && !first) || (top && first))
            {
                trect.setTop(static_cast<qreal>(trueheight / 2) + (trect.top() / 2));
                trect.setBottom(static_cast<qreal>(trueheight / 2) + (trect.bottom() / 2));
                trect.adjust(0, -bob, 0, -bob);
            }
        }

        // discard stereoscopic fields
        if (filter->outputBuffer == kDefaultBuffer)
        {
            if (kStereoscopicModeSideBySideDiscard == stereo)
                trect = QRectF(trect.left() / 2.0,  trect.top(),
                               trect.width() / 2.0, trect.height());
            if (kStereoscopicModeTopAndBottomDiscard == stereo)
                trect = QRectF(trect.left(),  trect.top() / 2.0,
                               trect.width(), trect.height() / 2.0);
        }

        // vertex coordinates
        QRect display = (filter->outputBuffer == kDefaultBuffer) ?
                         display_video_rect : frameBufferRect;
        QRect visible = (filter->outputBuffer == kDefaultBuffer) ?
                         display_visible_rect : frameBufferRect;
        QRectF vrect(display);

        // invert if first filter
        if (it == filters.begin())
        {
            if (filters.size() > 1)
            {
                vrect.setTop((visible.height()) - display.top());
                vrect.setBottom(vrect.top() - (display.height()));
            }
            else
            {
                vrect.setBottom(display.top());
                vrect.setTop(display.top() + (display.height()));
            }
        }

        // hardware bobdeint
        if (filter->outputBuffer == kDefaultBuffer &&
            hardwareDeinterlacing &&
            hardwareDeinterlacer == "openglbobdeint")
        {
            float bob = ((float)display.height() / (float)video_rect.height())
                        / 2.0f;
            float field = kScan_Interlaced ? -1.0f : 1.0f;
            bob = bob * (topfieldfirst ? field : -field);
            vrect.adjust(0, bob, 0, bob);
        }

        uint target = 0;
        // bind correct frame buffer (default onscreen) and set viewport
        switch (filter->outputBuffer)
        {
            case kDefaultBuffer:
                gl_context->BindFramebuffer(0);
                if (viewportControl)
                    gl_context->SetViewPort(QRect(QPoint(), display_visible_rect.size()));
                else
                    gl_context->SetViewPort(QRect(QPoint(), masterViewportSize));
                break;
            case kFrameBufferObject:
                if (!filter->frameBuffers.empty())
                {
                    gl_context->BindFramebuffer(filter->frameBuffers[0]);
                    gl_context->SetViewPort(QRect(QPoint(), frameBufferRect.size()));
                    target = filter->frameBuffers[0];
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
            gl_context->DrawRect(piprect, redbrush, nopen, 255);
        }

        // bind correct textures
        uint textures[4]; // NB
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

        if (helperTexture && type == kGLFilterBicubic)
            textures[texture_count++] = helperTexture;

        // enable fragment program and set any environment variables
        QOpenGLShaderProgram *program = nullptr;
        if (((type != kGLFilterNone) && (type != kGLFilterResize)) ||
            ((gl_features & kGLSL) && (type == kGLFilterResize)))
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

        if (type == kGLFilterYUV2RGB || type == kGLFilterYV12RGB)
        {
            gl_context->SetShaderProgramParams(program,
                GLMatrix4x4(reinterpret_cast<float*>(colourSpace->GetMatrix())),
                COLOUR_UNIFORM);
        }

        gl_context->DrawBitmap(textures, texture_count, target, &trect, &vrect,
                               program);

        inputs = filter->frameBufferTextures;
        inputsize = realsize;
    }
}

void OpenGLVideo::RotateTextures(void)
{
    if (referenceTextures.size() < 2)
        return;

    if (refsNeeded > 0)
        refsNeeded--;

    GLuint tmp = referenceTextures[referenceTextures.size() - 1];

    for (uint i = referenceTextures.size() - 1; i > 0;  i--)
        referenceTextures[i] = referenceTextures[i - 1];

    referenceTextures[0] = inputTextures[0];
    inputTextures[0] = tmp;
}

void OpenGLVideo::DeleteTextures(vector<GLuint> *textures)
{
    if ((*textures).empty())
        return;

    for (uint i = 0; i < (*textures).size(); i++)
        gl_context->DeleteTexture((*textures)[i]);
    (*textures).clear();
}

void OpenGLVideo::SetTextureFilters(vector<GLuint> *textures,
                                    int filt, int wrap)
{
    if (textures->empty())
        return;

    for (uint i = 0; i < textures->size(); i++)
        gl_context->SetTextureFilters((*textures)[i], filt, wrap);
}

OpenGLVideo::OpenGLFilterType OpenGLVideo::StringToFilter(const QString &filter)
{
    OpenGLFilterType ret = kGLFilterNone;

    if (filter.contains("master"))
        ret = kGLFilterYUV2RGB;
    else if (filter.contains("resize"))
        ret = kGLFilterResize;
    else if (filter.contains("bicubic"))
        ret = kGLFilterBicubic;
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
            return "master";
        case kGLFilterResize:
            return "resize";
        case kGLFilterBicubic:
            return "bicubic";
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
    if ("opengl-lite" == type)  return kGLYCbCr;
    if ("opengl-gpu" == type)   return kGLGPU;
    return kGLUYVY; // opengl
}

QString OpenGLVideo::TypeToString(VideoType Type)
{
    switch (Type)
    {
        case kGLGPU:   return "opengl-gpu";
        case kGLYCbCr: return "opengl-lite"; // compatibility with old profiles
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

    float lineHeight = 1.0f / inputTextureSize.height();
    float colWidth   = 1.0f / inputTextureSize.width();
    float yselect    = (float)inputTextureSize.width();
    float maxwidth   = video_dim.width()  / (float)inputTextureSize.width();
    float maxheight  = video_dim.height() / (float)inputTextureSize.height();
    float yv12height = maxheight;
    QSize fb_size    = gl_context->GetTextureSize(video_disp_dim);
    float fieldSize  = inputTextureSize.height() / 2.0f;

    string.replace("%FIELDHEIGHT%",   QString::number(fieldSize, 'f', 8));
    string.replace("%LINEHEIGHT%",    QString::number(lineHeight, 'f', 8));
    string.replace("%2LINEHEIGHT%",   QString::number(lineHeight * 2.0f, 'f', 8));
    string.replace("%BICUBICCWIDTH%", QString::number(colWidth, 'f', 8));
    string.replace("%BICUBICWIDTH%",  QString::number((float)fb_size.width(), 'f', 1));
    string.replace("%BICUBICHEIGHT%", QString::number((float)fb_size.height(), 'f', 1));
    string.replace("%SELECTCOL%",     QString::number(yselect, 'f', 8));
    string.replace("%MAXHEIGHT%",     QString::number(maxheight - lineHeight, 'f', 8));
    string.replace("%WIDTH%",         QString::number(maxwidth, 'f', 8));
    string.replace("%HEIGHT%",        QString::number(yv12height, 'f', 8));
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
        case kGLFilterBicubic:
            fragment = BicubicShader;
            break;
        default:
            LOG(VB_PLAYBACK, LOG_ERR, LOC + "Unknown filter");
            break;
    }
    CustomiseProgramString(vertex);
    CustomiseProgramString(fragment);
}
