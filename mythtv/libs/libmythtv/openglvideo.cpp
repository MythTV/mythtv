// MythTV headers
#include "openglvideo.h"
#include "mythcontext.h"
#include "tv.h"
#include "mythrender_opengl.h"
#include "mythavutil.h"

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
        vector<GLuint> fragmentPrograms;
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
    inputTextureSize(0,0),    currentFrameNum(0),
    inputUpdated(false),      refsNeeded(0),
    textureRects(false),      textureType(GL_TEXTURE_2D),
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
                       const QString& options)
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
    currentFrameNum       = -1;
    inputUpdated          = false;
    videoType             = Type;

    // Set OpenGL feature support
    gl_features = gl_context->GetFeatures();

    if (viewportControl)
        gl_context->SetFence();

    SetViewPort(masterViewportSize);

    bool glsl    = (gl_features & kGLSL) != 0U;
    bool shaders = glsl || ((gl_features & kGLExtFragProg) != 0U);
    bool fbos    = (gl_features & kGLExtFBufObj) != 0U;
    bool ycbcr   = ((gl_features & kGLMesaYCbCr) != 0U) || ((gl_features & kGLAppleYCbCr) != 0U);
    VideoType fallback = shaders ? (glsl ? (fbos ? kGLUYVY : kGLYV12) : kGLHQUYV) : ycbcr ? kGLYCbCr : kGLRGBA;

    // check for feature support
    bool unsupported = (kGLYCbCr == videoType) && !ycbcr;
    unsupported     |= (kGLYV12  == videoType) && !glsl;
    unsupported     |= (kGLUYVY  == videoType) && (!shaders || !fbos);
    unsupported     |= (kGLHQUYV == videoType) && !shaders;

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
        if (shaders && fbos && (gl_features & kGLExtRGBA16))
            defaultUpsize = kGLFilterBicubic;
        else
            LOG(VB_PLAYBACK, LOG_ERR, LOC + "No OpenGL feature support for Bicubic filter.");
    }

    // decide on best input texture type - GLX surfaces (for VAAPI) and the
    // bicubic filter do not work with rectangular textures. YV12 now supports
    // rectangular textures but POT textures are retained for the time being.
    if ((gl_features & kGLExtRect) && (kGLGPU != videoType) &&
        (kGLYV12 != videoType) && (kGLFilterBicubic != defaultUpsize))
    {
        textureType = gl_context->GetTextureType(textureRects);
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
    resize_down |= ((filters.count(kGLFilterYUV2RGB) == 0U) &&
                    (filters.count(kGLFilterYV12RGB) == 0U));

    // Extra stage needed on Fire Stick 4k, maybe others, because of blank screen when playing.
    resize_down |= forceResize;

    if (resize_up && (defaultUpsize == kGLFilterBicubic))
    {
        RemoveFilter(kGLFilterResize);
        AddFilter(kGLFilterBicubic);
        SetFiltering();
        return;
    }

    if ((resize_up && (defaultUpsize == kGLFilterResize)) || resize_down)
    {
        RemoveFilter(kGLFilterBicubic);
        AddFilter(kGLFilterResize);
        SetFiltering();
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
                    it->second->frameBuffers.push_back(tmp_buf);
                    it->second->frameBufferTextures.push_back(tmp_tex);
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
        if ((!(gl_features & kGLExtFragProg) && !(gl_features & kGLSL)) || !(gl_features & kGLExtFBufObj))
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC +
                "Features not available for bicubic filter.");
            return false;
        }
        break;

      case kGLFilterYUV2RGB:
        if (!(gl_features & kGLExtFragProg) && !(gl_features & kGLSL))
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
        GLuint program = AddFragmentProgram(filter);
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

    vector<GLuint> temp;
    vector<GLuint>::iterator it;

    temp = filters[filter]->fragmentPrograms;
    for (it = temp.begin(); it != temp.end(); ++it)
        gl_context->DeleteShaderObject(*it);
    filters[filter]->fragmentPrograms.clear();

    temp = filters[filter]->frameBuffers;
    for (it = temp.begin(); it != temp.end(); ++it)
        gl_context->DeleteFrameBuffer(*it);
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
            gl_context->DeleteShaderObject(tmp->fragmentPrograms.back());
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
    if (!(gl_features & kGLExtFragProg) && !(gl_features & kGLSL))
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC +
            "No shader support for OpenGL deinterlacing.");
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

    uint prog1 = AddFragmentProgram(type, deinterlacer, kScan_Interlaced);
    uint prog2 = AddFragmentProgram(type, deinterlacer, kScan_Intr2ndField);

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

uint OpenGLVideo::AddFragmentProgram(OpenGLFilterType name,
                                     const QString& deint, FrameScanType field)
{
    if (!gl_context)
        return 0;

    QString vertex, fragment;
    if (gl_features & kGLSL)
    {
        GetProgramStrings(vertex, fragment, name, deint, field);
    }
    else if (gl_features & kGLExtFragProg)
    {
        fragment = GetProgramString(name, deint, field);
    }
    else
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC + "No OpenGL shader/program support");
        return 0;
    }

    return gl_context->CreateShaderObject(vertex, fragment);
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
    bool use_pbo = (gl_features & kGLExtPBufObj) != 0U;
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

    tex_size = gl_context->GetTextureSize(textureType, size);
    return tmp_tex;
}

QSize OpenGLVideo::GetTextureSize(const QSize &size)
{
    if (textureRects)
        return size;

    int w = 64;
    int h = 64;

    while (w < size.width())
    {
        w *= 2;
    }

    while (h < size.height())
    {
        h *= 2;
    }

    return {w, h};
}

uint OpenGLVideo::GetInputTexture(void) const
{
    return inputTextures[0];
}

uint OpenGLVideo::GetTextureType(void) const
{
    return textureType;
}

void OpenGLVideo::SetInputUpdated(void)
{
    inputUpdated = true;
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
    inputUpdated = true;
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
                               long long frame, StereoscopicMode stereo,
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
    QSize realsize  = GetTextureSize(video_disp_dim);

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
            width /= 2.0F;

        QRectF trect(QPoint(0, 0), QSize(width, trueheight));

        // only apply overscan on last filter
        if (filter->outputBuffer == kDefaultBuffer)
            trect.setCoords(video_rect.left(),  video_rect.top(),
                            video_rect.left() + video_rect.width(),
                            video_rect.top()  + video_rect.height());

        if (!textureRects && (inputsize.height() > 0))
            trueheight /= inputsize.height();

        // software bobdeint
        if (actual)
        {
            bool top = (scan == kScan_Intr2ndField && topfieldfirst) ||
                       (scan == kScan_Interlaced && !topfieldfirst);
            bool bot = (scan == kScan_Interlaced && topfieldfirst) ||
                       (scan == kScan_Intr2ndField && !topfieldfirst);
            bool first = filters.size() < 2;
            float bob = (trueheight / (float)video_disp_dim.height()) / 4.0F;
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
                        / 2.0F;
            float field = kScan_Interlaced ? -1.0F : 1.0F;
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
        for (size_t i = 0; i < inputs.size(); i++)
            textures[texture_count++] = inputs[i];

        if (!referenceTextures.empty() &&
            hardwareDeinterlacing &&
            (type == kGLFilterYUV2RGB || type == kGLFilterYV12RGB))
        {
            for (size_t i = 0; i < referenceTextures.size(); i++)
                textures[texture_count++] = referenceTextures[i];
        }

        if (helperTexture && type == kGLFilterBicubic)
            textures[texture_count++] = helperTexture;

        // enable fragment program and set any environment variables
        GLuint program = 0;
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
            gl_context->SetShaderParams(program,
                GLMatrix4x4(reinterpret_cast<float*>(colourSpace->GetMatrix())),
                COLOUR_UNIFORM);
        }

        gl_context->DrawBitmap(textures, texture_count, target, &trect, &vrect,
                               program);

        inputs = filter->frameBufferTextures;
        inputsize = realsize;
    }

    currentFrameNum = frame;
    inputUpdated = false;
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

    for (size_t i = 0; i < (*textures).size(); i++)
        gl_context->DeleteTexture((*textures)[i]);
    (*textures).clear();
}

void OpenGLVideo::SetTextureFilters(vector<GLuint> *textures,
                                    int filt, int wrap)
{
    if (textures->empty())
        return;

    for (size_t i = 0; i < textures->size(); i++)
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

static const QString attrib_fast =
"ATTRIB tex   = fragment.texcoord[0];\n"
"PARAM yuv[3] = { program.local[0..2] };\n";

static const QString tex_fast =
"TEX res, tex, texture[0], %1;\n";

static const QString var_fast =
"TEMP tmp, res;\n";

static const QString var_col =
"TEMP col;\n";

static const QString select_col =
"MUL col, tex.xxxx, %8;\n"
"FRC col, col;\n"
"SUB col, col, 0.5;\n"
"CMP res, col, res.rabg, res.rgba;\n";

static const QString end_fast =
"DPH tmp.r, res.%SWIZZLE%g, yuv[0];\n"
"DPH tmp.g, res.%SWIZZLE%g, yuv[1];\n"
"DPH tmp.b, res.%SWIZZLE%g, yuv[2];\n"
"MOV tmp.a, 1.0;\n"
"MOV result.color, tmp;\n";

static const QString var_deint =
"TEMP other, current, mov, prev;\n";

static const QString field_calc =
"MUL prev, tex.yyyy, %2;\n"
"FRC prev, prev;\n"
"SUB prev, prev, 0.5;\n";

static const QString bobdeint[2] = {
field_calc +
"ADD other, tex, {0.0, %3, 0.0, 0.0};\n"
"MIN other, other, {10000.0, %9, 10000.0, 10000.0};\n"
"TEX other, other, texture[0], %1;\n"
"CMP res, prev, res, other;\n",
field_calc +
"SUB other, tex, {0.0, %3, 0.0, 0.0};\n"
"TEX other, other, texture[0], %1;\n"
"CMP res, prev, other, res;\n"
};

static const QString deint_end_top =
"CMP res,  prev, current, other;\n";

static const QString deint_end_bot =
"CMP res,  prev, other, current;\n";

static const QString linearblend[2] = {
"TEX current, tex, texture[0], %1;\n"
"ADD other, tex, {0.0, %3, 0.0, 0.0};\n"
"MIN other, other, {10000.0, %9, 10000.0, 10000.0};\n"
"TEX other, other, texture[0], %1;\n"
"SUB mov, tex, {0.0, %3, 0.0, 0.0};\n"
"TEX mov, mov, texture[0], %1;\n"
"LRP other, 0.5, other, mov;\n"
+ field_calc + deint_end_top,

"TEX current, tex, texture[0], %1;\n"
"SUB other, tex, {0.0, %3, 0.0, 0.0};\n"
"TEX other, other, texture[0], %1;\n"
"ADD mov, tex, {0.0, %3, 0.0, 0.0};\n"
"TEX mov, mov, texture[0], %1;\n"
"LRP other, 0.5, other, mov;\n"
+ field_calc + deint_end_bot
};

static const QString kerneldeint[2] = {
"TEX current, tex, texture[1], %1;\n"
"TEX prev, tex, texture[2], %1;\n"
"MUL other, 0.125, prev;\n"
"MAD other, 0.125, current, other;\n"
"ADD prev, tex, {0.0, %3, 0.0, 0.0};\n"
"MIN prev, prev, {10000.0, %9, 10000.0, 10000.0};\n"
"TEX prev, prev, texture[1], %1;\n"
"MAD other, 0.5, prev, other;\n"
"SUB prev, tex, {0.0, %3, 0.0, 0.0};\n"
"TEX prev, prev, texture[1], %1;\n"
"MAD other, 0.5, prev, other;\n"
"ADD prev, tex, {0.0, %4, 0.0, 0.0};\n"
"TEX tmp, prev, texture[1], %1;\n"
"MAD other, -0.0625, tmp, other;\n"
"TEX tmp, prev, texture[2], %1;\n"
"MAD other, -0.0625, tmp, other;\n"
"SUB prev, tex, {0.0, %4, 0.0, 0.0};\n"
"TEX tmp, prev, texture[1], %1;\n"
"MAD other, -0.0625, tmp, other;\n"
"TEX tmp, prev, texture[2], %1;\n"
"MAD other, -0.0625, tmp, other;\n"
+ field_calc + deint_end_top,

"TEX current, tex, texture[1], %1;\n"
"MUL other, 0.125, res;\n"
"MAD other, 0.125, current, other;\n"
"ADD prev, tex, {0.0, %3, 0.0, 0.0};\n"
"TEX prev, prev, texture[1], %1;\n"
"MAD other, 0.5, prev, other;\n"
"SUB prev, tex, {0.0, %3, 0.0, 0.0};\n"
"TEX prev, prev, texture[1], %1;\n"
"MAD other, 0.5, prev, other;\n"
"ADD prev, tex, {0.0, %4, 0.0, 0.0};\n"
"TEX tmp, prev, texture[1], %1;\n"
"MAD other, -0.0625, tmp, other;\n"
"TEX tmp, prev, texture[0], %1;\n"
"MAD other, -0.0625, tmp, other;\n"
"SUB prev, tex, {0.0, %4, 0.0, 0.0};\n"
"TEX tmp, prev, texture[1], %1;\n"
"MAD other, -0.0625, tmp, other;\n"
"TEX tmp, prev, texture[0], %1;\n"
"MAD other, -0.0625, tmp, other;\n"
+ field_calc + deint_end_bot
};

static const QString bicubic =
"TEMP coord, coord2, cdelta, parmx, parmy, a, b, c, d;\n"
"MAD coord.xy, fragment.texcoord[0], {%6, 0.0}, {0.5, 0.5};\n"
"MAD coord2.xy, fragment.texcoord[0], {0.0, %7}, {0.5, 0.5};\n"
"TEX parmx, coord.xy, texture[1], 2D;\n"
"TEX parmy, coord2.yx, texture[1], 2D;\n"
"MUL cdelta.xz, parmx.rrgg, {-%5, 0, %5, 0};\n"
"MUL cdelta.yw, parmy.rrgg, {0, -%3, 0, %3};\n"
"ADD coord, fragment.texcoord[0].xyxy, cdelta.xyxw;\n"
"ADD coord2, fragment.texcoord[0].xyxy, cdelta.zyzw;\n"
"TEX a, coord.xyxy, texture[0], 2D;\n"
"TEX b, coord.zwzw, texture[0], 2D;\n"
"TEX c, coord2.xyxy, texture[0], 2D;\n"
"TEX d, coord2.zwzw, texture[0], 2D;\n"
"LRP a, parmy.b, a, b;\n"
"LRP c, parmy.b, c, d;\n"
"LRP result.color, parmx.b, a, c;\n";

QString OpenGLVideo::GetProgramString(OpenGLFilterType name,
                                      const QString& deint, FrameScanType field)
{
    QString ret =
        "!!ARBfp1.0\n"
        "OPTION ARB_precision_hint_fastest;\n";

    switch (name)
    {
        case kGLFilterYUV2RGB:
        {
            bool need_tex = true;
            bool packed = (kGLUYVY == videoType);
            QString deint_bit = "";
            if (deint != "")
            {
                uint tmp_field = 0;
                if (field == kScan_Intr2ndField)
                    tmp_field = 1;
                if (deint == "openglbobdeint" ||
                    deint == "openglonefield" ||
                    deint == "opengldoubleratefieldorder")
                {
                    deint_bit = bobdeint[tmp_field];
                }
                else if (deint == "opengllinearblend" ||
                         deint == "opengldoubleratelinearblend")
                {
                    deint_bit = linearblend[tmp_field];
                    if (!tmp_field) { need_tex = false; }
                }
                else if (deint == "openglkerneldeint" ||
                         deint == "opengldoubleratekerneldeint")
                {
                    deint_bit = kerneldeint[tmp_field];
                    if (!tmp_field) { need_tex = false; }
                }
                else
                {
                    LOG(VB_PLAYBACK, LOG_ERR, LOC +
                        "Unrecognised OpenGL deinterlacer");
                }
            }

            ret += attrib_fast;
            ret += (deint != "") ? var_deint : "";
            ret += packed ? var_col : "";
            ret += var_fast + (need_tex ? tex_fast : "");
            ret += deint_bit;
            ret += packed ? select_col : "";
            ret += end_fast;
        }
            break;

        case kGLFilterNone:
        case kGLFilterResize:
            break;

        case kGLFilterBicubic:

            ret += bicubic;
            break;

        case kGLFilterYV12RGB: // TODO: extend this for opengl1
        default:
            LOG(VB_PLAYBACK, LOG_ERR, LOC + "Unknown fragment program.");
            break;
    }

    CustomiseProgramString(ret);
    ret += "END";

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Created %1 fragment program %2")
                .arg(FilterToString(name)).arg(deint));

    return ret;
}

void OpenGLVideo::CustomiseProgramString(QString &string)
{
    string.replace("%1", textureRects ? "RECT" : "2D");

    if (!textureRects)
    {
        string.replace("GLSL_SAMPLER", "sampler2D");
        string.replace("GLSL_TEXTURE", "texture2D");
    }

    float lineHeight = 1.0F;
    float colWidth   = 1.0F;
    float yselect    = 1.0F;
    float maxwidth   = inputTextureSize.width();
    float maxheight  = inputTextureSize.height();
    float yv12height = video_dim.height();
    QSize fb_size = GetTextureSize(video_disp_dim);

    if (!textureRects && (inputTextureSize.height() > 0) && (inputTextureSize.width() > 0))
    {
        lineHeight /= inputTextureSize.height();
        colWidth   /= inputTextureSize.width();
        yselect     = (float)inputTextureSize.width();
        maxwidth    = video_dim.width()  / (float)inputTextureSize.width();
        maxheight   = video_dim.height() / (float)inputTextureSize.height();
        yv12height  = maxheight;
    }

    float fieldSize = 1.0F / (lineHeight * 2.0F);

    string.replace("%2", QString::number(fieldSize, 'f', 8));
    string.replace("%3", QString::number(lineHeight, 'f', 8));
    string.replace("%4", QString::number(lineHeight * 2.0F, 'f', 8));
    string.replace("%5", QString::number(colWidth, 'f', 8));
    string.replace("%6", QString::number((float)fb_size.width(), 'f', 1));
    string.replace("%7", QString::number((float)fb_size.height(), 'f', 1));
    string.replace("%8", QString::number(yselect, 'f', 8));
    string.replace("%9", QString::number(maxheight - lineHeight, 'f', 8));
    string.replace("%WIDTH%", QString::number(maxwidth, 'f', 8));
    string.replace("%HEIGHT%", QString::number(yv12height, 'f', 8));
    string.replace("COLOUR_UNIFORM", COLOUR_UNIFORM);
    // TODO fix alternative swizzling by changing the YUVA packing code
    string.replace("%SWIZZLE%", kGLUYVY == videoType ? "arb" : "abr");
}

static const QString YUV2RGBVertexShader =
"GLSL_DEFINES"
"attribute vec2 a_position;\n"
"attribute vec2 a_texcoord0;\n"
"varying   vec2 v_texcoord0;\n"
"uniform   mat4 u_projection;\n"
"void main() {\n"
"    gl_Position = u_projection * vec4(a_position, 0.0, 1.0);\n"
"    v_texcoord0 = a_texcoord0;\n"
"}\n";

static const QString SelectColumn =
"    if (fract(v_texcoord0.x * %8) < 0.5)\n"
"        yuva = yuva.rabg;\n";

static const QString YUV2RGBFragmentShader =
"GLSL_DEFINES"
"uniform GLSL_SAMPLER s_texture0;\n"
"uniform mat4 COLOUR_UNIFORM;\n"
"varying vec2 v_texcoord0;\n"
"void main(void)\n"
"{\n"
"    vec4 yuva    = GLSL_TEXTURE(s_texture0, v_texcoord0);\n"
"SELECT_COLUMN"
"    gl_FragColor = vec4(yuva.%SWIZZLE%, 1.0) * COLOUR_UNIFORM;\n"
"}\n";

static const QString OneFieldShader[2] = {
"GLSL_DEFINES"
"uniform GLSL_SAMPLER s_texture0;\n"
"uniform mat4 COLOUR_UNIFORM;\n"
"varying vec2 v_texcoord0;\n"
"void main(void)\n"
"{\n"
"    float field = v_texcoord0.y + (step(0.5, fract(v_texcoord0.y * %2)) * %3);\n"
"    field       = clamp(field, 0.0, %9);\n"
"    vec4 yuva   = GLSL_TEXTURE(s_texture0, vec2(v_texcoord0.x, field));\n"
"SELECT_COLUMN"
"    gl_FragColor = vec4(yuva.%SWIZZLE%, 1.0) * COLOUR_UNIFORM;\n"
"}\n",

"GLSL_DEFINES"
"uniform GLSL_SAMPLER s_texture0;\n"
"uniform mat4 COLOUR_UNIFORM;\n"
"varying vec2 v_texcoord0;\n"
"void main(void)\n"
"{\n"
"    vec2 field   = vec2(0.0, step(0.5, 1.0 - fract(v_texcoord0.y * %2)) * %3);\n"
"    vec4 yuva    = GLSL_TEXTURE(s_texture0, v_texcoord0 + field);\n"
"SELECT_COLUMN"
"    gl_FragColor = vec4(yuva.%SWIZZLE%, 1.0) * COLOUR_UNIFORM;\n"
"}\n"
};

static const QString LinearBlendShader[2] = {
"GLSL_DEFINES"
"uniform GLSL_SAMPLER s_texture0;\n"
"uniform mat4 COLOUR_UNIFORM;\n"
"varying vec2 v_texcoord0;\n"
"void main(void)\n"
"{\n"
"    vec4 yuva  = GLSL_TEXTURE(s_texture0, v_texcoord0);\n"
"    vec4 above = GLSL_TEXTURE(s_texture0, vec2(v_texcoord0.x, min(v_texcoord0.y + %3, %9)));\n"
"    vec4 below = GLSL_TEXTURE(s_texture0, vec2(v_texcoord0.x, max(v_texcoord0.y - %3, %3)));\n"
"    if (fract(v_texcoord0.y * %2) >= 0.5)\n"
"        yuva = mix(above, below, 0.5);\n"
"SELECT_COLUMN"
"    gl_FragColor = vec4(yuva.%SWIZZLE%, 1.0) * COLOUR_UNIFORM;\n"
"}\n",

"GLSL_DEFINES"
"uniform GLSL_SAMPLER s_texture0;\n"
"uniform mat4 COLOUR_UNIFORM;\n"
"varying vec2 v_texcoord0;\n"
"void main(void)\n"
"{\n"
"    vec4 yuva  = GLSL_TEXTURE(s_texture0, v_texcoord0);\n"
"    vec4 above = GLSL_TEXTURE(s_texture0, vec2(v_texcoord0.x, min(v_texcoord0.y + %3, %9)));\n"
"    vec4 below = GLSL_TEXTURE(s_texture0, vec2(v_texcoord0.x, max(v_texcoord0.y - %3, %3)));\n"
"    if (fract(v_texcoord0.y * %2) < 0.5)\n"
"        yuva = mix(above, below, 0.5);\n"
"SELECT_COLUMN"
"    gl_FragColor = vec4(yuva.%SWIZZLE%, 1.0) * COLOUR_UNIFORM;\n"
"}\n"
};

static const QString KernelShader[2] = {
"GLSL_DEFINES"
"uniform GLSL_SAMPLER s_texture1;\n"
"uniform GLSL_SAMPLER s_texture2;\n"
"uniform mat4 COLOUR_UNIFORM;\n"
"varying vec2 v_texcoord0;\n"
"void main(void)\n"
"{\n"
"    vec4 yuva = GLSL_TEXTURE(s_texture1, v_texcoord0);\n"
"    if (fract(v_texcoord0.y * %2) >= 0.5)\n"
"    {\n"
"        vec2 oneup   = vec2(v_texcoord0.x, max(v_texcoord0.y - %3, %3));\n"
"        vec2 twoup   = vec2(v_texcoord0.x, max(v_texcoord0.y - %4, %3));\n"
"        vec2 onedown = vec2(v_texcoord0.x, min(v_texcoord0.y + %3, %9));\n"
"        vec2 twodown = vec2(v_texcoord0.x, min(v_texcoord0.y + %4, %9));\n"
"        vec4 line0   = GLSL_TEXTURE(s_texture1, twoup);\n"
"        vec4 line1   = GLSL_TEXTURE(s_texture1, oneup);\n"
"        vec4 line3   = GLSL_TEXTURE(s_texture1, onedown);\n"
"        vec4 line4   = GLSL_TEXTURE(s_texture1, twodown);\n"
"        vec4 line00  = GLSL_TEXTURE(s_texture2, twoup);\n"
"        vec4 line20  = GLSL_TEXTURE(s_texture2, v_texcoord0);\n"
"        vec4 line40  = GLSL_TEXTURE(s_texture2, twodown);\n"
"        yuva = (yuva   * 0.125);\n"
"        yuva = (line20 * 0.125) + yuva;\n"
"        yuva = (line1  * 0.5) + yuva;\n"
"        yuva = (line3  * 0.5) + yuva;\n"
"        yuva = (line0  * -0.0625) + yuva;\n"
"        yuva = (line4  * -0.0625) + yuva;\n"
"        yuva = (line00 * -0.0625) + yuva;\n"
"        yuva = (line40 * -0.0625) + yuva;\n"
"    }\n"
"SELECT_COLUMN"
"    gl_FragColor = vec4(yuva.%SWIZZLE%, 1.0) * COLOUR_UNIFORM;\n"
"}\n",

"GLSL_DEFINES"
"uniform GLSL_SAMPLER s_texture0;\n"
"uniform GLSL_SAMPLER s_texture1;\n"
"uniform mat4 COLOUR_UNIFORM;\n"
"varying vec2 v_texcoord0;\n"
"void main(void)\n"
"{\n"
"    vec4 yuva = GLSL_TEXTURE(s_texture1, v_texcoord0);\n"
"    if (fract(v_texcoord0.y * %2) < 0.5)\n"
"    {\n"
"        vec2 oneup   = vec2(v_texcoord0.x, max(v_texcoord0.y - %3, %3));\n"
"        vec2 twoup   = vec2(v_texcoord0.x, max(v_texcoord0.y - %4, %3));\n"
"        vec2 onedown = vec2(v_texcoord0.x, min(v_texcoord0.y + %3, %9));\n"
"        vec2 twodown = vec2(v_texcoord0.x, min(v_texcoord0.y + %4, %9));\n"
"        vec4 line0   = GLSL_TEXTURE(s_texture1, twoup);\n"
"        vec4 line1   = GLSL_TEXTURE(s_texture1, oneup);\n"
"        vec4 line3   = GLSL_TEXTURE(s_texture1, onedown);\n"
"        vec4 line4   = GLSL_TEXTURE(s_texture1, twodown);\n"
"        vec4 line00  = GLSL_TEXTURE(s_texture0, twoup);\n"
"        vec4 line20  = GLSL_TEXTURE(s_texture0, v_texcoord0);\n"
"        vec4 line40  = GLSL_TEXTURE(s_texture0, twodown);\n"
"        yuva = (yuva   * 0.125);\n"
"        yuva = (line20 * 0.125) + yuva;\n"
"        yuva = (line1  * 0.5) + yuva;\n"
"        yuva = (line3  * 0.5) + yuva;\n"
"        yuva = (line0  * -0.0625) + yuva;\n"
"        yuva = (line4  * -0.0625) + yuva;\n"
"        yuva = (line00 * -0.0625) + yuva;\n"
"        yuva = (line40 * -0.0625) + yuva;\n"
"    }\n"
"SELECT_COLUMN"
"    gl_FragColor = vec4(yuva.%SWIZZLE%, 1.0) * COLOUR_UNIFORM;\n"
"}\n"
};

static const QString BicubicShader =
"GLSL_DEFINES"
"uniform sampler2D s_texture0;\n"
"uniform sampler2D s_texture1;\n"
"varying vec2 v_texcoord0;\n"
"void main(void)\n"
"{\n"
"    vec2 coord = (v_texcoord0 * vec2(%6, %7)) - vec2(0.5, 0.5);\n"
"    vec4 parmx = texture2D(s_texture1, vec2(coord.x, 0.0));\n"
"    vec4 parmy = texture2D(s_texture1, vec2(coord.y, 0.0));\n"
"    vec2 e_x = vec2(%5, 0.0);\n"
"    vec2 e_y = vec2(0.0, %3);\n"
"    vec2 coord10 = v_texcoord0 + parmx.x * e_x;\n"
"    vec2 coord00 = v_texcoord0 - parmx.y * e_x;\n"
"    vec2 coord11 = coord10     + parmy.x * e_y;\n"
"    vec2 coord01 = coord00     + parmy.x * e_y;\n"
"    coord10      = coord10     - parmy.y * e_y;\n"
"    coord00      = coord00     - parmy.y * e_y;\n"
"    vec4 tex00   = texture2D(s_texture0, coord00);\n"
"    vec4 tex10   = texture2D(s_texture0, coord10);\n"
"    vec4 tex01   = texture2D(s_texture0, coord01);\n"
"    vec4 tex11   = texture2D(s_texture0, coord11);\n"
"    tex00        = mix(tex00, tex01, parmy.z);\n"
"    tex10        = mix(tex10, tex11, parmy.z);\n"
"    gl_FragColor = mix(tex00, tex10, parmx.z);\n"
"}\n";

static const QString DefaultFragmentShader =
"GLSL_DEFINES"
"uniform GLSL_SAMPLER s_texture0;\n"
"varying vec2 v_texcoord0;\n"
"void main(void)\n"
"{\n"
"    vec4 color   = GLSL_TEXTURE(s_texture0, v_texcoord0);\n"
"    gl_FragColor = vec4(color.xyz, 1.0);\n"
"}\n";

static const QString YV12RGBVertexShader =
"//YV12RGBVertexShader\n"
"GLSL_DEFINES"
"attribute vec2 a_position;\n"
"attribute vec2 a_texcoord0;\n"
"varying   vec2 v_texcoord0;\n"
"uniform   mat4 u_projection;\n"
"void main() {\n"
"    gl_Position = u_projection * vec4(a_position, 0.0, 1.0);\n"
"    v_texcoord0 = a_texcoord0;\n"
"}\n";

#define SAMPLEYVU "\
vec3 sampleYVU(in GLSL_SAMPLER texture, vec2 texcoordY)\n\
{\n\
    vec2 texcoordV, texcoordU;\n\
    texcoordV = vec2(texcoordY.s / 2.0, %HEIGHT% + texcoordY.t / 4.0);\n\
    texcoordU = vec2(texcoordV.s, texcoordV.t + %HEIGHT% / 4.0);\n\
    if (fract(texcoordY.t * %2) >= 0.5)\n\
    {\n\
        texcoordV.s += %WIDTH% / 2.0;\n\
        texcoordU.s += %WIDTH% / 2.0;\n\
    }\n\
    vec3 yvu;\n\
    yvu.r = GLSL_TEXTURE(texture, texcoordY).r;\n\
    yvu.g = GLSL_TEXTURE(texture, texcoordV).r;\n\
    yvu.b = GLSL_TEXTURE(texture, texcoordU).r;\n\
    return yvu;\n\
}\n"

static const QString YV12RGBFragmentShader =
"//YV12RGBFragmentShader\n"
"GLSL_DEFINES"
"uniform GLSL_SAMPLER s_texture0; // 4:1:1 YVU planar\n"
"uniform mat4 COLOUR_UNIFORM;\n"
"varying vec2 v_texcoord0;\n"
SAMPLEYVU
"void main(void)\n"
"{\n"
"    vec3 yvu = sampleYVU(s_texture0, v_texcoord0);\n"
"    gl_FragColor = vec4(yvu, 1.0) * COLOUR_UNIFORM;\n"
"}\n";

static const QString YV12RGBOneFieldFragmentShader[2] = {
"//YV12RGBOneFieldFragmentShader 1\n"
"GLSL_DEFINES"
"uniform GLSL_SAMPLER s_texture0;\n"
"uniform mat4 COLOUR_UNIFORM;\n"
"varying vec2 v_texcoord0;\n"
SAMPLEYVU
"void main(void)\n"
"{\n"
"    float field  = min(v_texcoord0.y + (step(0.5, fract(v_texcoord0.y * %2))) * %3, %HEIGHT% - %3);\n"
"    vec3 yvu     = sampleYVU(s_texture0, vec2(v_texcoord0.x, field));\n"
"    gl_FragColor = vec4(yvu, 1.0) * COLOUR_UNIFORM;\n"
"}\n",

"//YV12RGBOneFieldFragmentShader 2\n"
"GLSL_DEFINES"
"uniform GLSL_SAMPLER s_texture0;\n"
"uniform mat4 COLOUR_UNIFORM;\n"
"varying vec2 v_texcoord0;\n"
SAMPLEYVU
"void main(void)\n"
"{\n"
"    float field  = max(v_texcoord0.y + (step(0.5, 1.0 - fract(v_texcoord0.y * %2))) * %3, 0.0);\n"
"    vec3 yvu     = sampleYVU(s_texture0, vec2(v_texcoord0.x, field));\n"
"    gl_FragColor = vec4(yvu, 1.0) * COLOUR_UNIFORM;\n"
"}\n"
};

static const QString YV12RGBLinearBlendFragmentShader[2] = {
"// YV12RGBLinearBlendFragmentShader - Top\n"
"GLSL_DEFINES"
"uniform GLSL_SAMPLER s_texture0; // 4:1:1 YVU planar\n"
"uniform mat4 COLOUR_UNIFORM;\n"
"varying vec2 v_texcoord0;\n"
SAMPLEYVU
"void main(void)\n"
"{\n"
"    vec3 current = sampleYVU(s_texture0, v_texcoord0);\n"
"    if (fract(v_texcoord0.y * %2) >= 0.5)\n"
"    {\n"
"        vec3 above = sampleYVU(s_texture0, vec2(v_texcoord0.x, min(v_texcoord0.y + %3, %HEIGHT% - %3)));\n"
"        vec3 below = sampleYVU(s_texture0, vec2(v_texcoord0.x, max(v_texcoord0.y - %3, 0.0)));\n"
"        current = mix(above, below, 0.5);\n"
"    }\n"
"    gl_FragColor = vec4(current, 1.0) * COLOUR_UNIFORM;\n"
"}\n",

"// YV12RGBLinearBlendFragmentShader - Bottom\n"
"GLSL_DEFINES"
"uniform GLSL_SAMPLER s_texture0; // 4:1:1 YVU planar\n"
"uniform mat4 COLOUR_UNIFORM;\n"
"varying vec2 v_texcoord0;\n"
SAMPLEYVU
"void main(void)\n"
"{\n"
"    vec3 current = sampleYVU(s_texture0, v_texcoord0);\n"
"    if (fract(v_texcoord0.y * %2) < 0.5)\n"
"    {\n"
"        vec3 above = sampleYVU(s_texture0, vec2(v_texcoord0.x, min(v_texcoord0.y + %3, %HEIGHT% - %3)));\n"
"        vec3 below = sampleYVU(s_texture0, vec2(v_texcoord0.x, max(v_texcoord0.y - %3, 0.0)));\n"
"        current = mix(above, below, 0.5);\n"
"    }\n"
"    gl_FragColor = vec4(current, 1.0) * COLOUR_UNIFORM;\n"
"}\n"};

#define KERNELYVU "\
vec3 kernelYVU(in vec3 yvu, GLSL_SAMPLER texture1, GLSL_SAMPLER texture2)\n\
{\n\
    vec2 twoup   = v_texcoord0 - vec2(0.0, %4);\n\
    vec2 twodown = v_texcoord0 + vec2(0.0, %4);\n\
    twodown.t = min(twodown.t, %HEIGHT% - %3);\n\
    vec2 onedown = v_texcoord0 + vec2(0.0, %3);\n\
    onedown.t = min(onedown.t, %HEIGHT% - %3);\n\
    vec3 line0   = sampleYVU(texture1, twoup);\n\
    vec3 line1   = sampleYVU(texture1, v_texcoord0 - vec2(0.0, %3));\n\
    vec3 line3   = sampleYVU(texture1, onedown);\n\
    vec3 line4   = sampleYVU(texture1, twodown);\n\
    vec3 line00  = sampleYVU(texture2, twoup);\n\
    vec3 line20  = sampleYVU(texture2, v_texcoord0);\n\
    vec3 line40  = sampleYVU(texture2, twodown);\n\
    yvu *=           0.125;\n\
    yvu += line20 *  0.125;\n\
    yvu += line1  *  0.5;\n\
    yvu += line3  *  0.5;\n\
    yvu += line0  * -0.0625;\n\
    yvu += line4  * -0.0625;\n\
    yvu += line00 * -0.0625;\n\
    yvu += line40 * -0.0625;\n\
    return yvu;\n\
}\n"

static const QString YV12RGBKernelShader[2] = {
"//YV12RGBKernelShader 1\n"
"GLSL_DEFINES"
"uniform GLSL_SAMPLER s_texture1, s_texture2; // 4:1:1 YVU planar\n"
"uniform mat4 COLOUR_UNIFORM;\n"
"varying vec2 v_texcoord0;\n"
SAMPLEYVU
KERNELYVU
"void main(void)\n"
"{\n"
"    vec3 yvu = sampleYVU(s_texture1, v_texcoord0);\n"
"    if (fract(v_texcoord0.t * %2) >= 0.5)\n"
"        yvu = kernelYVU(yvu, s_texture1, s_texture2);\n"
"    gl_FragColor = vec4(yvu, 1.0) * COLOUR_UNIFORM;\n"
"}\n",

"//YV12RGBKernelShader 2\n"
"GLSL_DEFINES"
"uniform GLSL_SAMPLER s_texture0, s_texture1; // 4:1:1 YVU planar\n"
"uniform mat4 COLOUR_UNIFORM;\n"
"varying vec2 v_texcoord0;\n"
SAMPLEYVU
KERNELYVU
"void main(void)\n"
"{\n"
"    vec3 yvu = sampleYVU(s_texture1, v_texcoord0);\n"
"    if (fract(v_texcoord0.t * %2) < 0.5)\n"
"        yvu = kernelYVU(yvu, s_texture1, s_texture0);\n"
"    gl_FragColor = vec4(yvu, 1.0) * COLOUR_UNIFORM;\n"
"}\n"
};

void OpenGLVideo::GetProgramStrings(QString &vertex, QString &fragment,
                                    OpenGLFilterType filter,
                                    const QString& deint, FrameScanType field)
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
