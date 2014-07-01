// MythTV headers
#include "mythcontext.h"
#include "tv.h"
#include "openglvideo.h"
#include "mythrender_opengl.h"
#include "mythavutil.h"

#define LOC QString("GLVid: ")
#define COLOUR_UNIFORM "m_colourMatrix"

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
 *  to BGRA format is simply blitted to the frame buffer.
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
 * \fn OpenGLVideo::OpenGLVideo()
 *  Create a new OpenGLVideo instance that must be initialised
 *  with a call to OpenGLVideo::Init()
 */

OpenGLVideo::OpenGLVideo() :
    gl_context(NULL),         video_disp_dim(0,0),
    video_dim(0,0),           viewportSize(0,0),
    masterViewportSize(0,0),  display_visible_rect(0,0,0,0),
    display_video_rect(0,0,0,0), video_rect(0,0,0,0),
    frameBufferRect(0,0,0,0), softwareDeinterlacer(QString::null),
    hardwareDeinterlacer(QString::null), hardwareDeinterlacing(false),
    colourSpace(NULL),        viewportControl(false),
    inputTextureSize(0,0),    currentFrameNum(0),
    inputUpdated(false),      refsNeeded(0),
    textureRects(false),      textureType(GL_TEXTURE_2D),
    helperTexture(0),         defaultUpsize(kGLFilterResize),
    gl_features(0),           videoTextureType(GL_BGRA),
    preferYCBCR(false)
{
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
    {
        RemoveFilter(filters.begin()->first);
        filters.erase(filters.begin());
    }
}

/**
 *  \fn OpenGLVideo::Init(MythRenderOpenGL *glcontext, bool colour_control,
                       QSize videoDim, QRect displayVisibleRect,
                       QRect displayVideoRect, QRect videoRect,
                       bool viewport_control, QString options, bool osd)
 *  \param glcontext          the MythRenderOpenGL object responsible for lower
 *   levelwindow and OpenGL context integration
 *  \param colour_control     if true, manipulation of video attributes
 *   (colour, contrast etc) will be enabled
 *  \param videoDim           the size of the video source
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
                       bool viewport_control, QString options,
                       bool hw_accel)
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

    // OpenGL-Lite - use implementation specific extensions for updating frames
    if (options.contains("preferycbcr"))
        preferYCBCR = true;

    // Set OpenGL feature support
    gl_features = gl_context->GetFeatures();

    if (viewportControl)
        gl_context->SetFence();

    SetViewPort(display_visible_rect.size());

    bool shaders = (gl_features & kGLExtFragProg) || (gl_features & kGLSL);
    bool fbos    = gl_features & kGLExtFBufObj;
    bool uyvy    = !getenv("OPENGL_NOUYVY");
    bool ycbcr   = (gl_features & kGLMesaYCbCr) || (gl_features & kGLAppleYCbCr);

    // warn about the lite profile when it offers no benefit
    if (!ycbcr && preferYCBCR)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "You have selected the opengl-lite profile but no required OpenGL "
            "extensions are available.");
    }

    // decide on best video input texture format
    videoTextureType = GL_BGRA;
    if (hw_accel)
        videoTextureType = GL_RGBA;
    else if ((shaders && fbos && uyvy) && !(ycbcr && preferYCBCR))
        videoTextureType = MYTHTV_UYVY;
    else if ((!shaders || preferYCBCR) && (gl_features & kGLMesaYCbCr))
        videoTextureType = GL_YCBCR_MESA;
    else if ((!shaders || preferYCBCR) && (gl_features & kGLAppleYCbCr))
        videoTextureType = GL_YCBCR_422_APPLE;

    // colourspace adjustments require shaders to operate on YUV textures
    if ((GL_BGRA != videoTextureType) && (MYTHTV_UYVY != videoTextureType))
        colourSpace->SetSupportedAttributes(kPictureAttributeSupported_None);

    // turn on bicubic filtering
    if (options.contains("openglbicubic"))
    {
        if (shaders && fbos)
            defaultUpsize = kGLFilterBicubic;
        else
            LOG(VB_PLAYBACK, LOG_ERR, LOC +
                "No OpenGL feature support for Bicubic filter.");
    }

    // decide on best input texture type
    if ((GL_RGBA != videoTextureType) && (defaultUpsize != kGLFilterBicubic) &&
        (gl_features & kGLExtRect))
    {
        textureType = gl_context->GetTextureType(textureRects);
    }

    // Create initial input texture and associated filter stage
    GLuint tex = CreateVideoTexture(video_dim, inputTextureSize);
    bool    ok = false;

    if ((GL_BGRA == videoTextureType) || (MYTHTV_UYVY == videoTextureType))
        ok = tex && AddFilter(kGLFilterYUV2RGB);
    else
        ok = tex && AddFilter(kGLFilterResize);

    if (ok)
    {
        if (GL_RGBA == videoTextureType)
            LOG(VB_GENERAL, LOG_INFO, LOC + "Using raw RGBA input textures.");
        else if ((GL_YCBCR_MESA == videoTextureType) ||
                 (GL_YCBCR_422_APPLE == videoTextureType))
            LOG(VB_GENERAL, LOG_INFO, LOC +
                "Using YCbCr->BGRA input textures.");
        else if (MYTHTV_UYVY == videoTextureType)
            LOG(VB_GENERAL, LOG_INFO, LOC +
                "Using custom UYVY input textures.");
        else
            LOG(VB_GENERAL, LOG_INFO, LOC +
                "Using plain BGRA input textures.");
        inputTextures.push_back(tex);
    }
    else
        Teardown();

    if (filters.empty())
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
                "Failed to setup colourspace conversion.\n\t\t\t"
                "Falling back to software conversion.\n\t\t\t"
                "Any opengl filters will also be disabled.");

        videoTextureType = GL_BGRA;
        GLuint bgra32tex = CreateVideoTexture(video_dim, inputTextureSize);

        if (bgra32tex && AddFilter(kGLFilterResize))
        {
            inputTextures.push_back(bgra32tex);
            colourSpace->SetSupportedAttributes(kPictureAttributeSupported_None);
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Fatal error");
            Teardown();
            return false;
        }
    }

    bool mmx = false;
#ifdef MMX
    mmx = true;
#endif

    CheckResize(false);

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("MMX: %1 PBO: %2")
            .arg(mmx).arg((gl_features & kGLExtPBufObj) > 0));

    return true;
}

/**
 *   Determine if the output is to be scaled at all and create or destroy
 *   the appropriate filter as necessary.
 */

void OpenGLVideo::CheckResize(bool deinterlacing, bool allow)
{
    // to improve performance on slower cards
    bool resize_up = ((video_disp_dim.height() < display_video_rect.height()) ||
                     (video_disp_dim.width() < display_video_rect.width())) && allow;

    // to ensure deinterlacing works correctly
    bool resize_down = (video_disp_dim.height() > display_video_rect.height()) &&
                        deinterlacing && allow;

    // UYVY packed pixels must be sampled exactly and any overscan settings will
    // break sampling - so always force an extra stage
    resize_down |= videoTextureType == MYTHTV_UYVY;

    if (resize_up && (defaultUpsize == kGLFilterBicubic))
    {
        RemoveFilter(kGLFilterResize);
        filters.erase(kGLFilterResize);
        AddFilter(kGLFilterBicubic);
        OptimiseFilters();
        return;
    }

    if ((resize_up && (defaultUpsize == kGLFilterResize)) || resize_down)
    {
        RemoveFilter(kGLFilterBicubic);
        filters.erase(kGLFilterBicubic);
        AddFilter(kGLFilterResize);
        OptimiseFilters();
        return;
    }

    RemoveFilter(kGLFilterBicubic);
    filters.erase(kGLFilterBicubic);
    OptimiseFilters();
}

/**
 * \fn OpenGLVideo::OptimiseFilters(void)
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
 * \fn OpenGLVideo::SetFiltering(void)
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
 * \fn OpenGLVideo::AddFilter(OpenGLFilterType filter)
 *  Add a new filter stage and create any additional resources needed.
 */

bool OpenGLVideo::AddFilter(OpenGLFilterType filter)
{
    if (filters.count(filter))
        return true;

    if (!(gl_features & kGLExtFBufObj) && (filter == kGLFilterResize) &&
        !filters.empty())
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC +
            "GL_EXT_framebuffer_object not available "
            "for scaling/resizing filter.");
        return false;
    }

    if (!((gl_features & kGLExtFragProg) && (gl_features & kGLExtFBufObj)) &&
        filter == kGLFilterBicubic)
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC +
            "Features not available for bicubic filter.");
        return false;
    }

    if (!(gl_features & kGLExtFragProg) && !(gl_features & kGLSL) &&
        (filter == kGLFilterYUV2RGB))
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC +
            "No shader support for OpenGL deinterlacing.");
        return false;
    }

    bool success = true;

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Creating %1 filter.")
            .arg(FilterToString(filter)));

    OpenGLFilter *temp = new OpenGLFilter();

    temp->numInputs = 1;
    GLuint program = 0;

    if (filter == kGLFilterBicubic)
    {
        if (helperTexture)
            gl_context->DeleteTexture(helperTexture);

        helperTexture = gl_context->CreateHelperTexture();
        if (!helperTexture)
            success = false;
    }

    if (success && (filter != kGLFilterNone) && (filter != kGLFilterResize))
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
        temp = NULL;
        success &= OptimiseFilters();
    }

    if (!success)
    {
        RemoveFilter(filter);
        filters.erase(filter);
        delete temp; // If temp wasn't added to the filter list, we need to delete
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
    filters[filter] = NULL;

    return true;
}

void OpenGLVideo::TearDownDeinterlacer(void)
{
    if (!filters.count(kGLFilterYUV2RGB))
        return;

    OpenGLFilter *tmp = filters[kGLFilterYUV2RGB];

    if (tmp->fragmentPrograms.size() == 3)
    {
        gl_context->DeleteShaderObject(tmp->fragmentPrograms[2]);
        tmp->fragmentPrograms.pop_back();
    }

    if (tmp->fragmentPrograms.size() == 2)
    {
        gl_context->DeleteShaderObject(tmp->fragmentPrograms[1]);
        tmp->fragmentPrograms.pop_back();
    }

    DeleteTextures(&referenceTextures);
    refsNeeded = 0;
}

/**
 * \fn OpenGLVideo::AddDeinterlacer(const QString &deinterlacer)
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

    if (!filters.count(kGLFilterYUV2RGB))
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC +
            "No YUV2RGB filter stage for OpenGL deinterlacing%1.");
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

    uint prog1 = AddFragmentProgram(kGLFilterYUV2RGB,
                                    deinterlacer, kScan_Interlaced);
    uint prog2 = AddFragmentProgram(kGLFilterYUV2RGB,
                                    deinterlacer, kScan_Intr2ndField);

    if (prog1 && prog2)
    {
        filters[kGLFilterYUV2RGB]->fragmentPrograms.push_back(prog1);
        filters[kGLFilterYUV2RGB]->fragmentPrograms.push_back(prog2);
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
 * \fn OpenGLVideo::AddFragmentProgram(OpenGLFilterType name,
                                       QString deint, FrameScanType field)
 *  Create the correct fragment program for the given filter type
 */

uint OpenGLVideo::AddFragmentProgram(OpenGLFilterType name,
                                     QString deint, FrameScanType field)
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
 * \fn OpenGLVideo::AddFrameBuffer(uint &framebuffer,
                                   uint &texture, QSize vid_size)
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
 * \fn OpenGLVideo::CreateVideoTexture(QSize size, QSize &tex_size)
 *  Create and initialise an OpenGL texture suitable for a YV12 video frame
 *  of the given size.
 */

uint OpenGLVideo::CreateVideoTexture(QSize size, QSize &tex_size)
{
    uint tmp_tex = 0;
    bool use_pbo = gl_features & kGLExtPBufObj;
    if (GL_YCBCR_MESA == videoTextureType)
    {
        tmp_tex = gl_context->CreateTexture(size, use_pbo, textureType,
                                            GL_UNSIGNED_SHORT_8_8_MESA,
                                            GL_YCBCR_MESA, GL_YCBCR_MESA);
    }
    else if (GL_YCBCR_422_APPLE == videoTextureType)
    {
        tmp_tex = gl_context->CreateTexture(size, use_pbo, textureType,
                                            GL_UNSIGNED_SHORT_8_8_MESA,
                                            GL_YCBCR_422_APPLE, GL_RGBA);
    }
    else if (MYTHTV_UYVY == videoTextureType)
    {
        QSize fix(size.width() / 2, size.height());
        tmp_tex = gl_context->CreateTexture(fix, use_pbo, textureType,
                                            GL_UNSIGNED_BYTE, GL_RGBA, GL_RGBA);
    }
    else
        tmp_tex = gl_context->CreateTexture(size, use_pbo, textureType);

    tex_size = gl_context->GetTextureSize(textureType, size);
    if (!tmp_tex)
        return 0;

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

    return QSize(w, h);
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
 * \fn OpenGLVideo::UpdateInputFrame(const VideoFrame *frame, bool soft_bob)
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
        frame->codec != FMT_YV12)
    {
        return;
    }
    if (hardwareDeinterlacing)
        RotateTextures();

    // We need to convert frames here to avoid dependencies in MythRenderOpenGL
    void* buf = gl_context->GetTextureBuffer(inputTextures[0]);
    if (!buf)
        return;

    if (!filters.count(kGLFilterYUV2RGB) ||
        MYTHTV_UYVY == videoTextureType)
    {
        // software conversion
        AVPicture img_out;
        PixelFormat out_fmt = PIX_FMT_BGRA;
        if ((GL_YCBCR_MESA == videoTextureType) ||
            (GL_YCBCR_422_APPLE == videoTextureType) ||
            (MYTHTV_UYVY == videoTextureType))
        {
            out_fmt = PIX_FMT_UYVY422;
        }
        m_copyCtx.Copy(&img_out, frame, (unsigned char*)buf, out_fmt);
    }
    else if (frame->interlaced_frame && !soft_bob)
    {
        pack_yv12interlaced(frame->buf, (unsigned char*)buf, frame->offsets,
                            frame->pitches, video_dim);
    }
    else
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
        CheckResize(false, filter != "bobdeint");
    softwareDeinterlacer = filter;
    softwareDeinterlacer.detach();
}

/**
 * \fn OpenGLVideo::PrepareFrame(bool topfieldfirst, FrameScanType scan,
                                 bool softwareDeinterlacing,
                                 long long frame, bool draw_border)
 *  Render the contents of the current input texture to the framebuffer
 *  using the currently enabled filters.
 *  \param topfieldfirst        the frame is interlaced and top_field_first
 *   is set
 *  \param scan                 interlaced or progressive?
 *  \param softwareDeinerlacing the frame has been deinterlaced in software
 *  \param frame                the frame number
 *  \param draw_border          if true, draw a red border around the frame
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
        if ((type == kGLFilterYUV2RGB) && (videoTextureType == MYTHTV_UYVY))
            width /= 2.0f;

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
            float bob = (trueheight / (float)video_disp_dim.height()) / 4.0f;
            if ((top && !first) || (bot && first))
            {
                trect.setBottom(trect.bottom() / 2);
                trect.setTop(trect.top() / 2);
                trect.adjust(0, bob, 0, bob);
            }
            if ((bot && !first) || (top && first))
            {
                trect.setTop((trueheight / 2) + (trect.top() / 2));
                trect.setBottom((trueheight / 2) + (trect.bottom() / 2));
                trect.adjust(0, -bob, 0, -bob);
            }
        }

        // discard stereoscopic fields
        if (filter->outputBuffer == kDefaultBuffer)
        {
            if (kStereoscopicModeSideBySideDiscard == stereo)
                trect = QRectF(trect.left() / 2.0f,  trect.top(),
                               trect.width() / 2.0f, trect.height());
            if (kStereoscopicModeTopAndBottomDiscard == stereo)
                trect = QRectF(trect.left(),  trect.top() / 2.0f,
                               trect.width(), trect.height() / 2.0f);
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
            type == kGLFilterYUV2RGB)
        {
            for (uint i = 0; i < referenceTextures.size(); i++)
                textures[texture_count++] = referenceTextures[i];
        }

        if (helperTexture && type == kGLFilterBicubic)
            textures[texture_count++] = helperTexture;

        // enable fragment program and set any environment variables
        GLuint program = 0;
        if ((type != kGLFilterNone) && (type != kGLFilterResize))
        {
            GLuint prog_ref = 0;

            if (type == kGLFilterYUV2RGB)
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

        if (type == kGLFilterYUV2RGB)
            gl_context->SetShaderParams(program, colourSpace->GetMatrix(),
                                        COLOUR_UNIFORM);

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

OpenGLFilterType OpenGLVideo::StringToFilter(const QString &filter)
{
    OpenGLFilterType ret = kGLFilterNone;

    if (filter.contains("master"))
        ret = kGLFilterYUV2RGB;
    else if (filter.contains("resize"))
        ret = kGLFilterResize;
    else if (filter.contains("bicubic"))
        ret = kGLFilterBicubic;

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
    }

    return "";
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
"DPH tmp.r, res.arbg, yuv[0];\n"
"DPH tmp.g, res.arbg, yuv[1];\n"
"DPH tmp.b, res.arbg, yuv[2];\n"
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
"MAD coord.xy, fragment.texcoord[0], {%6, %7}, {0.5, 0.5};\n"
"TEX parmx, coord.x, texture[1], 1D;\n"
"TEX parmy, coord.y, texture[1], 1D;\n"
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
                                      QString deint, FrameScanType field)
{
    QString ret =
        "!!ARBfp1.0\n"
        "OPTION ARB_precision_hint_fastest;\n";

    switch (name)
    {
        case kGLFilterYUV2RGB:
        {
            bool need_tex = true;
            bool packed = MYTHTV_UYVY == videoTextureType;
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

    float lineHeight = 1.0f;
    float colWidth   = 1.0f;
    float yselect    = 1.0f;
    QSize fb_size = GetTextureSize(video_disp_dim);

    if (!textureRects &&
       (inputTextureSize.height() > 0))
    {
        lineHeight /= inputTextureSize.height();
        colWidth   /= inputTextureSize.width();
        yselect    /= ((float)inputTextureSize.width() / 2.0f);
    }

    float maxheight  = (float)(min(inputTextureSize.height(), 1080) - 1) *
                       lineHeight;
    float fieldSize = 1.0f / (lineHeight * 2.0);

    string.replace("%2", QString::number(fieldSize, 'f', 8));
    string.replace("%3", QString::number(lineHeight, 'f', 8));
    string.replace("%4", QString::number(lineHeight * 2.0, 'f', 8));
    string.replace("%5", QString::number(colWidth, 'f', 8));
    string.replace("%6", QString::number((float)fb_size.width(), 'f', 1));
    string.replace("%7", QString::number((float)fb_size.height(), 'f', 1));
    string.replace("%8", QString::number(1.0f / yselect, 'f', 8));
    string.replace("%9", QString::number(maxheight, 'f', 8));

    string.replace("COLOUR_UNIFORM", COLOUR_UNIFORM);
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
"    gl_FragColor = vec4(yuva.arb, 1.0) * COLOUR_UNIFORM;\n"
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
"    gl_FragColor = vec4(yuva.arb, 1.0) * COLOUR_UNIFORM;\n"
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
"    gl_FragColor = vec4(yuva.arb, 1.0) * COLOUR_UNIFORM;\n"
"}\n"
};

static const QString LinearBlendShader[2] = {
"GLSL_DEFINES"
"uniform GLSL_SAMPLER s_texture0;\n"
"uniform mat4 COLOUR_UNIFORM;\n"
"varying vec2 v_texcoord0;\n"
"void main(void)\n"
"{\n"
"    vec2 line  = vec2(0.0, %3);\n"
"    vec2 line2 = vec2(v_texcoord0.x, clamp(v_texcoord0.y + %3, 0.0, %9));\n"
"    vec4 yuva  = GLSL_TEXTURE(s_texture0, v_texcoord0);\n"
"    vec4 above = GLSL_TEXTURE(s_texture0, line2);\n"
"    vec4 below = GLSL_TEXTURE(s_texture0, v_texcoord0 - line);\n"
"    if (fract(v_texcoord0.y * %2) >= 0.5)\n"
"        yuva = mix(above, below, 0.5);\n"
"SELECT_COLUMN"
"    gl_FragColor = vec4(yuva.arb, 1.0) * COLOUR_UNIFORM;\n"
"}\n",

"GLSL_DEFINES"
"uniform GLSL_SAMPLER s_texture0;\n"
"uniform mat4 COLOUR_UNIFORM;\n"
"varying vec2 v_texcoord0;\n"
"void main(void)\n"
"{\n"
"    vec2 line  = vec2(0.0, %3);\n"
"    vec4 yuva  = GLSL_TEXTURE(s_texture0, v_texcoord0);\n"
"    vec4 above = GLSL_TEXTURE(s_texture0, v_texcoord0 + line);\n"
"    vec4 below = GLSL_TEXTURE(s_texture0, v_texcoord0 - line);\n"
"    if (fract(v_texcoord0.y * %2) < 0.5)\n"
"        yuva = mix(above, below, 0.5);\n"
"SELECT_COLUMN"
"    gl_FragColor = vec4(yuva.arb, 1.0) * COLOUR_UNIFORM;\n"
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
"    vec4 yuva    = GLSL_TEXTURE(s_texture1, v_texcoord0);\n"
"    if (fract(v_texcoord0.y * %2) >= 0.5)\n"
"    {\n"
"        vec2 twoup   = v_texcoord0 - vec2(0.0, %4);\n"
"        vec2 twodown = v_texcoord0 + vec2(0.0, %4);\n"
"        vec2 onedown = vec2(v_texcoord0.x, clamp(v_texcoord0.y + %3, 0.0, %9));\n"
"        vec4 line0   = GLSL_TEXTURE(s_texture1, twoup);\n"
"        vec4 line1   = GLSL_TEXTURE(s_texture1, v_texcoord0 - vec2(0.0, %3));\n"
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
"    gl_FragColor = vec4(yuva.arb, 1.0) * COLOUR_UNIFORM;\n"
"}\n",

"GLSL_DEFINES"
"uniform GLSL_SAMPLER s_texture0;\n"
"uniform GLSL_SAMPLER s_texture1;\n"
"uniform mat4 COLOUR_UNIFORM;\n"
"varying vec2 v_texcoord0;\n"
"void main(void)\n"
"{\n"
"    vec4 yuva    = GLSL_TEXTURE(s_texture1, v_texcoord0);\n"
"    if (fract(v_texcoord0.y * %2) < 0.5)\n"
"    {\n"
"        vec2 twoup   = v_texcoord0 - vec2(0.0, %4);\n"
"        vec2 twodown = v_texcoord0 + vec2(0.0, %4);\n"
"        vec4 line0   = GLSL_TEXTURE(s_texture1, twoup);\n"
"        vec4 line1   = GLSL_TEXTURE(s_texture1, v_texcoord0 - vec2(0.0, %3));\n"
"        vec4 line3   = GLSL_TEXTURE(s_texture1, v_texcoord0 + vec2(0.0, %3));\n"
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
"    gl_FragColor = vec4(yuva.arb, 1.0) * COLOUR_UNIFORM;\n"
"}\n"
};

static const QString BicubicShader =
"GLSL_DEFINES"
"uniform sampler2D s_texture0;\n"
"uniform sampler1D s_texture1;\n"
"varying vec2 v_texcoord0;\n"
"void main(void)\n"
"{\n"
"    vec2 coord = (v_texcoord0 * vec2(%6, %7)) - vec2(0.5, 0.5);\n"
"    vec4 parmx = texture1D(s_texture1, coord.x);\n"
"    vec4 parmy = texture1D(s_texture1, coord.y);\n"
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

            fragment.replace("SELECT_COLUMN", MYTHTV_UYVY == videoTextureType ?
                                              SelectColumn : "");
            break;
        }
        case kGLFilterNone:
        case kGLFilterResize:
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
