// MythTV headers
#include "mythcontext.h"
#include "tv.h"
#include "openglvideo.h"
#include "openglcontext.h"

// AVLib header
extern "C" {
#include "../libavcodec/avcodec.h"
}

// OpenGL headers
#define GL_GLEXT_PROTOTYPES
#define GLX_GLXEXT_PROTOTYPES
#define XMD_H 1
//#include <GL/glx.h>
#include <GL/gl.h>
#undef GLX_ARB_get_proc_address
//#include <GL/glxext.h>
//#include <GL/glext.h>
#include "util-opengl.h"

#define LOC QString("GLVid: ")
#define LOC_ERR QString("GLVid, Error: ")

class OpenGLFilter
{
    public:
        GLuint         fragmentProgram;
        uint           numInputs;
        bool           rotateFrameBuffers;
        vector<GLuint> frameBuffers;
        vector<GLuint> frameBufferTextures;
        DisplayBuffer  outputBuffer;
};

OpenGLVideo::OpenGLVideo() :
    gl_context(NULL),         videoSize(0,0),
    viewportSize(0,0),        masterViewportSize(0,0),
    visibleRect(0,0,0,0),     videoRect(0,0,0,0),
    frameRect(0,0,0,0),
    frameBufferRect(0,0,0,0), invertVideo(false),
    softwareDeinterlacer(QString::null),
    hardwareDeinterlacing(false),
    useColourControl(false),  viewportControl(false),
    frameBuffer(0),           frameBufferTexture(0),
    inputTextureSize(0,0),    currentFrameNum(0),
    inputUpdated(false),

    convertSize(0,0),         convertBuf(NULL),

    videoResize(false),       videoResizeRect(0,0,0,0)
{
}

OpenGLVideo::~OpenGLVideo()
{
    Teardown();
}

// locking ok
void OpenGLVideo::Teardown(void)
{
    ShutDownYUV2RGB();

    gl_context->MakeCurrent(true);

    if (frameBuffer)
        gl_context->DeleteFrameBuffer(frameBuffer);

    if (frameBufferTexture)
        gl_context->DeleteTexture(frameBufferTexture);

    for (uint i = 0; i < inputTextures.size(); i++)
        gl_context->DeleteTexture(inputTextures[i]);
    inputTextures.clear();

    if (!filters.empty())
    {
        glfilt_map_t::iterator it;
        for (it = filters.begin(); it != filters.end(); ++it)
        {
            if (it->second->fragmentProgram)
                gl_context->DeleteFragmentProgram(it->second->fragmentProgram);
            vector<GLuint> temp = it->second->frameBuffers;
            for (uint i = 0; i < temp.size(); i++)
                gl_context->DeleteFrameBuffer(temp[i]);
            temp = it->second->frameBufferTextures;
            for (uint i = 0; i < temp.size(); i++)
                gl_context->DeleteTexture((temp[i]));
        }
    }
    filters.clear();

    gl_context->MakeCurrent(false);
}

// locking ok
bool OpenGLVideo::Init(OpenGLContext *glcontext, bool colour_control,
                       bool onscreen, QSize video_size, QRect visible_rect,
                       QRect video_rect, QRect frame_rect,
                       bool viewport_control, bool osd)
{
    gl_context            = glcontext;
    videoSize             = video_size;
    visibleRect           = visible_rect;
    videoRect             = video_rect;
    frameRect             = frame_rect;
    masterViewportSize    = QSize(1920, 1080);

    QSize rect            = GetTextureSize(videoSize);

    frameBufferRect       = QRect(QPoint(0,0), rect);
    invertVideo           = true;
    softwareDeinterlacer  = "";
    hardwareDeinterlacing = false;
    useColourControl      = colour_control;
    viewportControl       = viewport_control;
    inputTextureSize      = QSize(0,0);
    convertSize           = QSize(0,0);
    videoResize           = false;
    videoResizeRect       = QRect(0,0,0,0);
    frameBuffer           = 0;
    currentFrameNum       = -1;
    inputUpdated          = false;

    if (!onscreen)
    {
        QSize fb_size = GetTextureSize(visibleRect.size());
        if (!AddFrameBuffer(frameBuffer, frameBufferTexture, fb_size))
            return false;
    }

    SetViewPort(visibleRect.size());
    InitOpenGL();

    if (osd)
    {
        QSize osdsize = visibleRect.size();
        QSize half_size(osdsize.width() >> 1, osdsize.height() >>1);
        GLuint alphatex = CreateVideoTexture(osdsize, inputTextureSize);
        GLuint utex = CreateVideoTexture(half_size, inputTextureSize);
        GLuint vtex = CreateVideoTexture(half_size, inputTextureSize);
        GLuint ytex = CreateVideoTexture(osdsize, inputTextureSize);

        if ((alphatex && ytex && utex && vtex) && AddFilter(kGLFilterYUV2RGBA))
        {
            inputTextures.push_back(ytex);
            inputTextures.push_back(utex);
            inputTextures.push_back(vtex);
            inputTextures.push_back(alphatex);
            if (!AddFilter(kGLFilterResize))
            {
                Teardown();
                return false;
            }
        }
    }
    else
    {
        QSize half_size(videoSize.width() >> 1, videoSize.height() >>1);
        GLuint utex = CreateVideoTexture(half_size, inputTextureSize);
        GLuint vtex = CreateVideoTexture(half_size, inputTextureSize);
        GLuint ytex = CreateVideoTexture(videoSize, inputTextureSize);;

        if ((ytex && utex && vtex) && AddFilter(kGLFilterYUV2RGB))
        {
            inputTextures.push_back(ytex);
            inputTextures.push_back(utex);
            inputTextures.push_back(vtex);
        }
    }

    if (filters.empty())
    {
        if (osd)
        {
            Teardown();
            return false;
        }

        VERBOSE(VB_PLAYBACK, LOC +
                "OpenGL colour conversion failed.\n\t\t\t"
                "Falling back to software conversion.\n\t\t\t"
                "Any opengl filters will also be disabled.");

        GLuint rgb24tex = CreateVideoTexture(videoSize, inputTextureSize);

        if (rgb24tex && AddFilter(kGLFilterResize))
        {
            inputTextures.push_back(rgb24tex);
        }
        else
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Fatal error");
            Teardown();
            return false;
        }
    }

    return true;
}

OpenGLFilterType OpenGLVideo::GetDeintFilter(void) const
{
    if (filters.count(kGLFilterKernelDeint))
        return kGLFilterKernelDeint;
    if (filters.count(kGLFilterLinearBlendDeint))
        return kGLFilterLinearBlendDeint;
    if (filters.count(kGLFilterOneFieldDeint))
        return kGLFilterOneFieldDeint;
    if (filters.count(kGLFilterBobDeintDFR))
        return kGLFilterBobDeintDFR;
    if (filters.count(kGLFilterOneFieldDeintDFR))
        return kGLFilterOneFieldDeintDFR;
    if (filters.count(kGLFilterLinearBlendDeintDFR))
        return kGLFilterLinearBlendDeintDFR;
    if (filters.count(kGLFilterKernelDeintDFR))
        return kGLFilterKernelDeintDFR;
    if (filters.count(kGLFilterFieldOrderDFR))
        return kGLFilterFieldOrderDFR;

    return kGLFilterNone;
}

bool OpenGLVideo::OptimiseFilters(void)
{
    // if video height does not match display rect height, add resize stage
    // to preserve field information N.B. assumes interlaced
    // if video rectangle is smaller than display rectangle, add resize stage
    // to improve performance

    bool needResize =  ((videoSize.height() != videoRect.height()) ||
                        (videoSize.width()  <  videoRect.width()));
    if (needResize && !filters.count(kGLFilterResize) &&
        !(AddFilter(kGLFilterResize)))
    {
        return false;
    }

    glfilt_map_t::reverse_iterator it;

    // add/remove required frame buffer objects
    // and link filters
    uint buffers_needed = 1;
    bool last_filter    = true;
    bool needtorotate   = false;
    for (it = filters.rbegin(); it != filters.rend(); it++)
    {
        it->second->outputBuffer = kFrameBufferObject;
        it->second->rotateFrameBuffers = needtorotate;
        if (!last_filter)
        {
            uint buffers_have = it->second->frameBuffers.size();
            int buffers_diff = buffers_needed - buffers_have;
            if (buffers_diff > 0)
            {
                uint tmp_buf, tmp_tex;
                QSize fb_size = GetTextureSize(videoSize);
                for (int i = 0; i < buffers_diff; i++)
                {
                    if (!AddFrameBuffer(tmp_buf, tmp_tex, fb_size))
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
            last_filter = false;
        }

        buffers_needed = it->second->numInputs;
        needtorotate = (it->first == kGLFilterKernelDeint ||
                        it->first == kGLFilterLinearBlendDeint ||
                        it->first == kGLFilterOneFieldDeintDFR ||
                        it->first == kGLFilterLinearBlendDeintDFR ||
                        it->first == kGLFilterKernelDeintDFR ||
                        it->first == kGLFilterFieldOrderDFR);

    }

    bool deinterlacing = hardwareDeinterlacing;
    hardwareDeinterlacing = true;

    SetDeinterlacing(false);
    if (deinterlacing)
        SetDeinterlacing(deinterlacing);

    return true;
}

// locking ok
void OpenGLVideo::SetFiltering(void)
{
    // filter settings included for performance only
    // no (obvious) quality improvement over GL_LINEAR throughout
    if (filters.empty())
        return;

    if (filters.size() == 1)
    {
        SetTextureFilters(&inputTextures, GL_LINEAR);
        return;
    }

    SetTextureFilters(&inputTextures, GL_NEAREST);
    vector<GLuint> textures;
    glfilt_map_t::iterator it;
    for (it = filters.begin(); it != filters.end(); it++)
        SetTextureFilters(&(it->second->frameBufferTextures), GL_NEAREST);

    // resize or last active (ie don't need resize) need GL_LINEAR
    glfilt_map_t::reverse_iterator rit;
    bool next = false;
    bool resize = filters.count(kGLFilterResize);
    for (rit = filters.rbegin(); rit != filters.rend(); rit++)
    {
        if (next && (rit->second->outputBuffer != kNoBuffer))
        {
            SetTextureFilters(&(rit->second->frameBufferTextures), GL_LINEAR);
            return;
        }

        if (resize)
        {
            next |= ((rit->first == kGLFilterResize) ||
                     (rit->second->outputBuffer == kDefaultBuffer));
        }
    }

    SetTextureFilters(&inputTextures, GL_LINEAR);
}

// locking ok
bool OpenGLVideo::ReInit(OpenGLContext *glcontext, bool colour_control,
                         bool onscreen, QSize video_size, QRect visible_rect,
                         QRect video_rect, QRect frame_rect,
                         bool viewport_control, bool osd)
{
    VERBOSE(VB_PLAYBACK, LOC + "Reinit");

    gl_context->MakeCurrent(true);

    QString harddeint   = GetDeinterlacer(); // only adds back deinterlacer
    QString softdeint   = softwareDeinterlacer;
    bool    interlacing = hardwareDeinterlacing;
    bool    resize      = videoResize;
    QRect   resize_rect = videoResizeRect;

    Teardown();

    bool success = Init(glcontext, colour_control, onscreen, video_size,
                        visible_rect, video_rect, frame_rect,
                        viewport_control, osd);

    if (harddeint != "")
        success &= AddDeinterlacer(harddeint);

    softwareDeinterlacer = softdeint;
    SetDeinterlacing(interlacing);

    if (resize)
        SetVideoResize(resize_rect);

    return success;
}

// locking ok
bool OpenGLVideo::AddFilter(OpenGLFilterType filter)
{
    if (filters.count(filter))
        return true;

    VERBOSE(VB_PLAYBACK, LOC + QString("Creating %1 filter.")
            .arg(FilterToString(filter)));

    gl_context->MakeCurrent(true);

    OpenGLFilter *temp = new OpenGLFilter();

    temp->numInputs = 1;

    if ((filter == kGLFilterLinearBlendDeint) ||
        (filter == kGLFilterKernelDeint) ||
        (filter == kGLFilterFieldOrderDFR))
    {
        temp->numInputs = 2;
    }
    else if ((filter == kGLFilterYUV2RGB) ||
             (filter == kGLFilterOneFieldDeintDFR) ||
             (filter == kGLFilterKernelDeintDFR) ||
             (filter == kGLFilterLinearBlendDeintDFR))
    {
        temp->numInputs = 3;
    }
    else if ((filter == kGLFilterYUV2RGBA))
    {
        temp->numInputs = 4;
    }

    GLuint program = 0;
    if (filter != kGLFilterNone && filter != kGLFilterResize)
    {
        program = AddFragmentProgram(filter);
        if (!program)
            return false;
    }

    temp->fragmentProgram    = program;
    temp->outputBuffer       = kDefaultBuffer;
    temp->rotateFrameBuffers = false;

    temp->frameBuffers.clear();
    temp->frameBufferTextures.clear();

    filters[filter] = temp;

    if (OptimiseFilters())
        return true;

    RemoveFilter(filter);

    return false;
}

// locking ok
bool OpenGLVideo::RemoveFilter(OpenGLFilterType filter)
{
    if (!filters.count(filter))
        return true;

    VERBOSE(VB_PLAYBACK, QString("Removing %1 filter")
            .arg(FilterToString(filter)));

    gl_context->MakeCurrent(true);

    gl_context->DeleteFragmentProgram(filters[filter]->fragmentProgram);

    vector<GLuint> temp;
    vector<GLuint>::iterator it;

    temp = filters[filter]->frameBuffers;
    for (it = temp.begin(); it != temp.end(); it++)
        gl_context->DeleteFrameBuffer(*it);

    temp = filters[filter]->frameBufferTextures;
    for (it = temp.begin(); it != temp.end(); it++)
        gl_context->DeleteTexture((*(it)));

    filters.erase(filter);

    gl_context->MakeCurrent(false);

    return true;
}

// locking ok
bool OpenGLVideo::AddDeinterlacer(const QString &filter)
{
    QString current_deinterlacer = GetDeinterlacer();

    if (current_deinterlacer == filter)
        return true;

    if (!current_deinterlacer.isEmpty())
        RemoveFilter(current_deinterlacer);

    return AddFilter(filter);
}

// locking ok
uint OpenGLVideo::AddFragmentProgram(OpenGLFilterType name)
{
    if (!gl_context->IsFeatureSupported(kGLExtFragProg))
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR + "Fragment programs not supported");
        return 0;
    }

    QString program = GetProgramString(name);
    QString texType = (gl_context->IsFeatureSupported(kGLExtRect)) ? "RECT" : "2D";
    program.replace("%1", texType);

    uint ret;
    if (gl_context->CreateFragmentProgram(program, ret))
    {
        VERBOSE(VB_PLAYBACK, LOC + QString("Created fragment program %1.")
                .arg(FilterToString(name)));

        return ret;
    }

    return 0;
}

// locking ok
bool OpenGLVideo::AddFrameBuffer(uint &framebuffer,
                                 uint &texture, QSize size)
{
    if (!gl_context->IsFeatureSupported(kGLExtFBufObj))
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR + "Framebuffer binding not supported.");
        return false;
    }

    texture = gl_context->CreateTexture();

    bool ok = gl_context->CreateFrameBuffer(framebuffer, texture, size);

    if (!ok)
        gl_context->DeleteTexture(texture);

    return ok;
}

// locking ok
void OpenGLVideo::SetViewPort(const QSize &viewPortSize)
{
    uint w = max(viewPortSize.width(),  videoSize.width());
    uint h = max(viewPortSize.height(), videoSize.height());

    viewportSize = QSize(w, h);

    if (!viewportControl)
        return;

    VERBOSE(VB_PLAYBACK, LOC + QString("Viewport: %1x%2")
            .arg(w).arg(h));

    SetViewPortPrivate(viewportSize);
}

void OpenGLVideo::SetViewPortPrivate(const QSize &viewPortSize)
{
    glViewport(0, 0, viewPortSize.width(), viewPortSize.height());
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, viewPortSize.width() - 1,
            0, viewPortSize.height() - 1, 1, -1); // aargh...
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

// locking ok
void OpenGLVideo::InitOpenGL(void)
{
    gl_context->MakeCurrent(true);
    glDisable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // for gl osd
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    gl_context->EnableTextures();;
    glShadeModel(GL_FLAT);
    glDisable(GL_POLYGON_SMOOTH);
    glDisable(GL_LINE_SMOOTH);
    glDisable(GL_POINT_SMOOTH);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glFlush();
    gl_context->MakeCurrent(false);
}

// locking ok
uint OpenGLVideo::CreateVideoTexture(QSize size, QSize &tex_size)
{
    uint tmp_tex = gl_context->CreateTexture();

    QSize temp = GetTextureSize(size);

    if ((temp.width()  > (int)gl_context->GetMaxTexSize()) ||
        (temp.height() > (int)gl_context->GetMaxTexSize()) ||
        !gl_context->SetupTexture(temp, tmp_tex, GL_LINEAR))
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR + "Could not create texture.");
        gl_context->DeleteTexture(tmp_tex);
        return 0;
    }

    tex_size = temp;

    VERBOSE(VB_PLAYBACK, LOC + QString("Created main input texture %1x%2")
            .arg(temp.width()).arg(temp.height()));

    return tmp_tex;
}

// locking ok
QSize OpenGLVideo::GetTextureSize(const QSize &size)
{
    if (gl_context->IsFeatureSupported(kGLExtRect))
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

// locking ok
void OpenGLVideo::UpdateInputFrame(const VideoFrame *frame)
{
    if (frame->width  != videoSize.width()  ||
        frame->height != videoSize.height() ||
        frame->width  < 1 ||
        frame->height < 1)
    {
        ShutDownYUV2RGB();
        return;
    }

    if (filters.count(kGLFilterYUV2RGB) && (frame->codec == FMT_YV12))
    {
        UpdateInput(frame->buf, frame->offsets, 0, FMT_YV12, videoSize);
        return;
    }

    // software yuv2rgb
    if (convertSize != videoSize)
    {
        ShutDownYUV2RGB();

        VERBOSE(VB_PLAYBACK, LOC + "Init software conversion.");

        convertSize = videoSize;
        convertBuf = new unsigned char[
            (videoSize.width() * videoSize.height() * 3) + 128];
    }

    if (convertBuf)
    {
        AVPicture img_in, img_out;

        avpicture_fill(&img_out, (uint8_t *)convertBuf, PIX_FMT_RGB24,
                       convertSize.width(), convertSize.height());
        avpicture_fill(&img_in, (uint8_t *)frame->buf, PIX_FMT_YUV420P,
                       convertSize.width(), convertSize.height());
        img_convert(&img_out, PIX_FMT_RGB24,
                    &img_in,  PIX_FMT_YUV420P,
                    convertSize.width(), convertSize.height());

        int offset = 0;
        UpdateInput(convertBuf, &offset, 0, FMT_RGB24, convertSize);
    }
}

// locking ok
void OpenGLVideo::UpdateInput(const unsigned char *buf, const int *offsets,
                              uint texture_index, int format, QSize size)
{
    inputUpdated = false;

    if (texture_index >= inputTextures.size())
        return;

    copy_pixels_to_texture(
        buf + offsets[0], format, size,
        inputTextures[texture_index], gl_context->GetTextureType());

    if (FMT_YV12 == format)
    {
        QSize chroma_size(size.width() >> 1, size.height() >> 1);
        copy_pixels_to_texture(
            buf + offsets[1], format, chroma_size,
            inputTextures[texture_index + 1],
            gl_context->GetTextureType());
        copy_pixels_to_texture(
            buf + offsets[2], format, chroma_size,
            inputTextures[texture_index + 2],
            gl_context->GetTextureType());
    }

    inputUpdated = true;
}

// locking ok
void OpenGLVideo::ShutDownYUV2RGB(void)
{
    if (convertBuf)
    {
        delete convertBuf;
        convertBuf = NULL;
    }
    convertSize = QSize(0,0);
}

// locking ok
// TODO shouldn't this take a QSize, not QRect?
void OpenGLVideo::SetVideoResize(const QRect &rect)
{
    bool abort = ((rect.right()  > videoSize.width())  ||
                  (rect.bottom() > videoSize.height()) ||
                  (rect.width()  > videoSize.width())  ||
                  (rect.height() > videoSize.height()));

    // if resize == existing frame, no need to carry on

    abort |= !rect.left() && !rect.top() && (rect.size() == videoSize);

    if (!abort)
    {
        videoResize     = true;
        videoResizeRect = rect;
        return;
    }

    DisableVideoResize();
}

// locking ok
void OpenGLVideo::DisableVideoResize(void)
{
    videoResize     = false;
    videoResizeRect = QRect(0, 0, 0, 0);
}

void OpenGLVideo::CalculateResize(float &left,  float &top,
                                  float &right, float &bottom)
{
    // FIXME video aspect == display aspect

    if ((videoSize.height() <= 0) || (videoSize.width() <= 0))
        return;

    float height     = visibleRect.height();
    float new_top    = height - ((float)videoResizeRect.bottom() /
                                 (float)videoSize.height()) * height;
    float new_bottom = height - ((float)videoResizeRect.top() /
                                 (float)videoSize.height()) * height;

    left   = (((float) videoResizeRect.left() / (float) videoSize.width()) *
              visibleRect.width());
    right  = (((float) videoResizeRect.right() / (float) videoSize.width()) *
              visibleRect.width());

    top    = new_top;
    bottom = new_bottom;
}

// locking ok
void OpenGLVideo::SetDeinterlacing(bool deinterlacing)
{
    if (deinterlacing == hardwareDeinterlacing)
        return;

    VERBOSE(VB_PLAYBACK, LOC + QString("Turning %1 deinterlacing.")
            .arg(deinterlacing ? "on" : "off"));

    hardwareDeinterlacing = deinterlacing;

    glfilt_map_t::iterator it = filters.begin();
    for (; it != filters.end(); it++)
    {
        it->second->outputBuffer = kFrameBufferObject;

        if ((it->first >= kGLFilterLinearBlendDeint) &&
            (it->first <= kGLFilterOneFieldDeintDFR) &&
            !deinterlacing)
        {
            it->second->outputBuffer = kNoBuffer;
        }
    }

    glfilt_map_t::reverse_iterator rit = filters.rbegin();
    for (; rit != filters.rend(); rit++)
    {
        if (rit->second->outputBuffer == kFrameBufferObject)
        {
            rit->second->outputBuffer = kDefaultBuffer;
            break;
        }
    }

    gl_context->MakeCurrent(true);
    SetFiltering();
    gl_context->MakeCurrent(false);
}

// locking ok
void OpenGLVideo::PrepareFrame(FrameScanType scan, bool softwareDeinterlacing,
                               long long frame)
{
    if (inputTextures.empty() || filters.empty())
        return;

    vector<GLuint> inputs = inputTextures;
    QSize inputsize = inputTextureSize;
    uint  numfilters = filters.size();

    glfilt_map_t::iterator it;
    for (it = filters.begin(); it != filters.end(); it++)
    {
        if (it->second->rotateFrameBuffers &&
            !(it->first == kGLFilterYUV2RGB && scan == kScan_Intr2ndField))
        {
            Rotate(&(it->second->frameBufferTextures));
            Rotate(&(it->second->frameBuffers));
        }

        // skip disabled filters
        if (it->second->outputBuffer == kNoBuffer)
            continue;

        OpenGLFilterType type = it->first;
        OpenGLFilter *filter = it->second;

        // skip colour conversion for osd already in frame buffer
        if (!inputUpdated && type == kGLFilterYUV2RGBA)
        {
            inputs = filter->frameBufferTextures;
            inputsize = videoSize;
            continue;
        }

        // skip colour conversion for frames already in frame buffer
        if (!inputUpdated && (frame == currentFrameNum) &&
            (type == kGLFilterYUV2RGB) && (frame != 0) &&
            (!(softwareDeinterlacing && softwareDeinterlacer == "bobdeint")))
        {
            inputs = filter->frameBufferTextures;
            inputsize = videoSize;
            continue;
        }

        // texture coordinates
        float t_right = (float)videoSize.width();
        float t_bottom  = (float)videoSize.height();
        float t_top = 0.0f;
        float t_left = 0.0f;
        float trueheight = (float)videoSize.height();

        // only apply overscan on last filter
        if (filter->outputBuffer == kDefaultBuffer)
        {
            t_left   = (float)frameRect.left();
            t_right  = (float)frameRect.width() + t_left;
            t_top    = (float)frameRect.top();
            t_bottom = (float)frameRect.height() + t_top;
        }

        if (!gl_context->IsFeatureSupported(kGLExtRect) &&
            (inputsize.width() > 0) && (inputsize.height() > 0))
        {
            t_right  /= inputsize.width();
            t_left   /= inputsize.width();
            t_bottom /= inputsize.height();
            t_top    /= inputsize.height();
            trueheight /= inputsize.height();
        }

        float line_height = (trueheight / (float)videoSize.height());
        float bob = line_height / 2.0f;

        if (type == kGLFilterBobDeintDFR)
        {
            if (scan == kScan_Interlaced)
            {
                t_bottom += bob;
                t_top += bob;
            }
            if (scan == kScan_Intr2ndField)
            {
                t_bottom -= bob;
                t_top -= bob;
            }
        }

        if (softwareDeinterlacer == "bobdeint" &&
            softwareDeinterlacing && (type == kGLFilterYUV2RGB ||
            (type == kGLFilterResize && numfilters == 1)))
        {
            bob = line_height / 4.0f;
            if (scan == kScan_Interlaced)
            {
                t_top /= 2;
                t_bottom /= 2;
                t_bottom += bob;
                t_top    += bob;
            }
            if (scan == kScan_Intr2ndField)
            {
                t_top = (trueheight / 2) + (t_top / 2);
                t_bottom = (trueheight / 2) + (t_bottom / 2);
                t_bottom -= bob;
                t_top    -= bob;
            }
        }

        float t_right_uv = t_right;
        float t_top_uv   = t_top;
        float t_bottom_uv = t_bottom;
        float t_left_uv  = t_left;

        if (gl_context->IsFeatureSupported(kGLExtRect))
        {
            t_right_uv  /= 2;
            t_top_uv    /= 2;
            t_bottom_uv /= 2;
            t_left_uv   /= 2;
        }

        // vertex coordinates
        QRect display = (filter->frameBuffers.empty() || 
                        filter->outputBuffer == kDefaultBuffer) ?
            videoRect : frameBufferRect;

        float vleft  = display.left();
        float vright = display.right();
        float vtop   = display.top();
        float vbot   = display.bottom();

        // resize for interactive tv
        if (videoResize && filter->outputBuffer == kDefaultBuffer)
            CalculateResize(vleft, vtop, vright, vbot);

        if (invertVideo &&
            ((type == kGLFilterYUV2RGB) || (type == kGLFilterYUV2RGBA)) ||
            ((type == kGLFilterResize) && (numfilters == 1)))
        {
            float temp = vtop;
            vtop = vbot;
            vbot = temp;
        }

        // bind correct frame buffer (default onscreen) and set viewport
        switch (filter->outputBuffer)
        {
            case kDefaultBuffer:
                if (frameBuffer)
                    gl_context->BindFramebuffer(frameBuffer);

                // clear the buffer
                if (viewportControl)
                {
                    glClear(GL_COLOR_BUFFER_BIT);
                    SetViewPortPrivate(visibleRect.size());
                }
                else
                {
                    SetViewPortPrivate(masterViewportSize);
                }

                break;

            case kFrameBufferObject:
                if (!filter->frameBuffers.empty())
                {
                    gl_context->BindFramebuffer(filter->frameBuffers[0]);
                    SetViewPortPrivate(frameBufferRect.size());
                }
                break;

            case kNoBuffer:
                continue;
        }

        // bind correct textures
        for (uint i = 0; i < inputs.size(); i++)
        {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(gl_context->GetTextureType(), inputs[i]);
        }

        // enable fragment program and set any environment variables
        if ((type != kGLFilterNone) && (type != kGLFilterResize))
        {
            glEnable(GL_FRAGMENT_PROGRAM_ARB);
            gl_context->BindFragmentProgram(filter->fragmentProgram);
            float field = -line_height;

            switch (type)
            {
                case kGLFilterYUV2RGB:
                case kGLFilterYUV2RGBA:
                    if (useColourControl)
                    {
                        gl_context->InitFragmentParams(
                            0,
                            pictureAttribs[kPictureAttribute_Brightness],
                            pictureAttribs[kPictureAttribute_Contrast],
                            pictureAttribs[kPictureAttribute_Colour],
                            0.0f);
                    }
                    break;

                case kGLFilterBobDeintDFR:
                case kGLFilterOneFieldDeintDFR:
                case kGLFilterKernelDeintDFR:
                case kGLFilterFieldOrderDFR:
                case kGLFilterLinearBlendDeintDFR:
                    if (scan == kScan_Intr2ndField)
                        field *= -1;

                case kGLFilterOneFieldDeint:
                case kGLFilterKernelDeint:
                case kGLFilterLinearBlendDeint:
                    gl_context->InitFragmentParams(
                        0, line_height * 2.0f, field, 0.0f, 0.0f);
                    break;

                case kGLFilterNone:
                case kGLFilterResize:
                    break;
            }
        }

        // enable blending for osd
        if (type == kGLFilterResize && filters.count(kGLFilterYUV2RGBA))
            glEnable(GL_BLEND);

        // draw quad
        glBegin(GL_QUADS);
        glTexCoord2f(t_left, t_top);
        if (type == kGLFilterYUV2RGB || type == kGLFilterYUV2RGBA)
        {
            glMultiTexCoord2f(GL_TEXTURE1, t_left_uv, t_top_uv);
            glMultiTexCoord2f(GL_TEXTURE2, t_left_uv, t_top_uv);
            if (type == kGLFilterYUV2RGBA)
                glMultiTexCoord2f(GL_TEXTURE3, t_left_uv, t_top_uv);
        }
        glVertex2f(vleft,  vtop);

        glTexCoord2f(t_right, t_top);
        if (type == kGLFilterYUV2RGB || type == kGLFilterYUV2RGBA)
        {
            glMultiTexCoord2f(GL_TEXTURE1, t_right_uv, t_top_uv);
            glMultiTexCoord2f(GL_TEXTURE2, t_right_uv, t_top_uv);
            if (type == kGLFilterYUV2RGBA)
                glMultiTexCoord2f(GL_TEXTURE3, t_right, t_top);
        }
        glVertex2f(vright, vtop);

        glTexCoord2f(t_right, t_bottom);
        if (type == kGLFilterYUV2RGB || type == kGLFilterYUV2RGBA)
        {
            glMultiTexCoord2f(GL_TEXTURE1, t_right_uv, t_bottom_uv);
            glMultiTexCoord2f(GL_TEXTURE2, t_right_uv, t_bottom_uv);
            if (type == kGLFilterYUV2RGBA)
                glMultiTexCoord2f(GL_TEXTURE3, t_right, t_bottom);
        }
        glVertex2f(vright, vbot);

        glTexCoord2f(t_left, t_bottom);
        if (type == kGLFilterYUV2RGB || type == kGLFilterYUV2RGBA)
        {
            glMultiTexCoord2f(GL_TEXTURE1, t_left_uv, t_bottom_uv);
            glMultiTexCoord2f(GL_TEXTURE2, t_left_uv, t_bottom_uv);
            if (type == kGLFilterYUV2RGBA)
                glMultiTexCoord2f(GL_TEXTURE3, t_left_uv, t_bottom);
        }
        glVertex2f(vleft,  vbot);
        glEnd();

        // disable blending
        if (type == kGLFilterResize && filters.count(kGLFilterYUV2RGBA))
            glDisable(GL_BLEND);

        // disable fragment program
        if (!(type == kGLFilterNone || type == kGLFilterResize))
        {
            gl_context->BindFragmentProgram(0);
            glDisable(GL_FRAGMENT_PROGRAM_ARB);
        }

        // switch back to default framebuffer
        if (filter->outputBuffer != kDefaultBuffer || frameBuffer)
            gl_context->BindFramebuffer(0);

        inputs = filter->frameBufferTextures;
        inputsize = videoSize;
    }

    currentFrameNum = frame;
    inputUpdated = false;
}

void OpenGLVideo::Rotate(vector<GLuint> *target)
{
    if (target->size() < 2)
        return;

    GLuint tmp = (*target)[target->size() - 1];
    for (uint i = target->size() - 1; i > 0;  i--)
        (*target)[i] = (*target)[i - 1];

    (*target)[0] = tmp;
}

// locking ok
int OpenGLVideo::SetPictureAttribute(
    PictureAttribute attribute, int newValue)
{
    if (!useColourControl)
        return -1;

    int ret = -1;
    switch (attribute)
    {
        case kPictureAttribute_Brightness:
            ret = newValue;
            pictureAttribs[attribute] = (newValue * 0.02f) - 0.5f;
            break;
        case kPictureAttribute_Contrast:
        case kPictureAttribute_Colour:
            ret = newValue;
            pictureAttribs[attribute] = (newValue * 0.02f);
            break;
        case kPictureAttribute_Hue: // not supported yet...
            break;
        default:
            break;
    }

    return ret;
}

PictureAttributeSupported 
OpenGLVideo::GetSupportedPictureAttributes(void) const
{
    return (!useColourControl) ?
        kPictureAttributeSupported_None :
        (PictureAttributeSupported) 
        (kPictureAttributeSupported_Brightness |
         kPictureAttributeSupported_Contrast |
         kPictureAttributeSupported_Colour);
}

// locking ok
void OpenGLVideo::SetTextureFilters(vector<GLuint> *textures, int filt)
{
    if (textures->empty())
        return;

    for (uint i = 0; i < textures->size(); i++)
        gl_context->SetupTextureFilters((*textures)[i], filt);
}

// locking ok
OpenGLFilterType OpenGLVideo::StringToFilter(const QString &filter)
{
    OpenGLFilterType ret = kGLFilterNone;

    if (filter.contains("master"))
        ret = kGLFilterYUV2RGB;
    else if (filter.contains("osd"))
        ret = kGLFilterYUV2RGBA;
    else if (filter.contains("openglkerneldeint"))
        ret = kGLFilterKernelDeint;
    else if (filter.contains("opengllinearblend"))
        ret = kGLFilterLinearBlendDeint;
    else if (filter.contains("openglonefield"))
        ret = kGLFilterOneFieldDeint;
    else if (filter.contains("openglbobdeint"))
        ret = kGLFilterBobDeintDFR;
    else if (filter.contains("opengldoubleratelinearblend"))
        ret = kGLFilterLinearBlendDeintDFR;
    else if (filter.contains("opengldoubleratekerneldeint"))
        ret = kGLFilterKernelDeintDFR;
    else if (filter.contains("opengldoublerateonefield"))
        ret = kGLFilterOneFieldDeintDFR;
    else if (filter.contains("opengldoubleratefieldorder"))
        ret = kGLFilterFieldOrderDFR;
    else if (filter.contains("resize"))
        ret = kGLFilterResize;

    return ret;
}

// locking ok
QString OpenGLVideo::FilterToString(OpenGLFilterType filt)
{
    switch (filt)
    {
        case kGLFilterNone:
            break;
        case kGLFilterYUV2RGB:
            return "master";
        case kGLFilterYUV2RGBA:
            return "osd";
        case kGLFilterKernelDeint:
            return "openglkerneldeint";
        case kGLFilterLinearBlendDeint:
            return "opengllinearblend";
        case kGLFilterOneFieldDeint:
            return "openglonefield";
        case kGLFilterBobDeintDFR:
            return "openglbobdeint";
        case kGLFilterLinearBlendDeintDFR:
            return "opengldoubleratelinearblend";
        case kGLFilterKernelDeintDFR:
            return "opengldoubleratekerneldeint";
        case kGLFilterOneFieldDeintDFR:
            return "opengldoublerateonefield";
        case kGLFilterFieldOrderDFR:
            return "opengldoubleratefieldorder";
        case kGLFilterResize:
            return "resize";
    }

    return "";
}

static const QString yuv2rgb1a =
"ATTRIB ytex  = fragment.texcoord[0];"
"ATTRIB uvtex = fragment.texcoord[1];"
"TEMP res, tmp;";

static const QString yuv2rgb1b =
"TEMP alpha;"
"TEX alpha, ytex, texture[3], %1;";

static const QString yuv2rgb1c =
"TEX res,   ytex,  texture[0], %1;"
"TEX tmp.x, uvtex, texture[1], %1;"
"TEX tmp.y, uvtex, texture[2], %1;";

static const QString yuv2rgb2 =
"PARAM  adj  = program.env[0];"
"SUB res, res, 0.5;"
"MAD res, res, adj.yyyy, adj.xxxx;"
"SUB tmp, tmp, { 0.5, 0.5 };"
"MAD tmp, adj.zzzz, tmp, 0.5;";

static const QString yuv2rgb3 =
"MAD res, res, 1.164, -0.063;"
"SUB tmp, tmp, { 0.5, 0.5 };"
"MAD res, { 0, -.392, 2.017 }, tmp.xxxw, res;";

static const QString yuv2rgb4 =
"MAD result.color, { 1.596, -.813, 0, 0 }, tmp.yyyw, res;";

static const QString yuv2rgb5 =
"MAD result.color, { 0, -.813, 1.596, 0 }, tmp.yyyw, res.bgra;";

static const QString yuv2rgb6 =
"MOV result.color.a, alpha.a;";

// locking ok
QString OpenGLVideo::GetProgramString(OpenGLFilterType name)
{
    QString ret =
        "!!ARBfp1.0\n"
        "OPTION ARB_precision_hint_fastest;";

    switch (name)
    {
        case kGLFilterYUV2RGB:
            ret = ret + yuv2rgb1a + yuv2rgb1c;
            if (useColourControl)
                ret += yuv2rgb2;
            ret += yuv2rgb3;
            ret += frameBuffer ? yuv2rgb5 : yuv2rgb4;
            break;

        case kGLFilterYUV2RGBA:
            ret = ret + yuv2rgb1a + yuv2rgb1b + yuv2rgb1c;
            if (useColourControl)
                ret += yuv2rgb2;
            ret = ret + yuv2rgb3 + yuv2rgb4 + yuv2rgb6;
            break;

        case kGLFilterKernelDeint:
            ret +=
                "ATTRIB tex = fragment.texcoord[0];"
                "PARAM  off = program.env[0];"
                "TEMP sam, pos, cum, cur, field, mov;"
                "RCP field, off.x;"
                "MUL field, tex.yyyy, field;"
                "FRC field, field;"
                "SUB field, field, 0.5;"
                "TEX sam, tex, texture[1], %1;"
                "TEX cur, tex, texture[0], %1;"
                "SUB mov, cur, sam;"
                "MUL cum, sam, 0.125;"
                "MAD cum, cur, 0.125, cum;"
                "ABS mov, mov;"
                "SUB mov, mov, 0.12;"
                "ADD pos, tex, off.wyww;"
                "TEX sam, pos, texture[0], %1;"
                "MAD cum, sam, 0.5, cum;"
                "SUB pos, tex, off.wyww;"
                "TEX sam, pos, texture[0], %1;"
                "MAD cum, sam, 0.5, cum;"
                "MAD pos, off.wyww, 2.0, tex;"
                "TEX sam, pos, texture[0], %1;"
                "MAD cum, sam, -0.0625, cum;"
                "TEX sam, pos, texture[1], %1;"
                "MAD cum, sam, -0.0625, cum;"
                "MAD pos, off.wyww, -2.0, tex;"
                "TEX sam, pos, texture[0], %1;"
                "MAD cum, sam, -0.0625, cum;"
                "TEX sam, pos, texture[1], %1;"
                "MAD cum, sam, -0.0625, cum;"
                "CMP cum, mov, cur, cum;"
                "CMP result.color, field, cum, cur;";
            break;

        case kGLFilterLinearBlendDeintDFR:
            ret +=
                "ATTRIB tex = fragment.texcoord[0];"
                "PARAM  off  = program.env[0];"
                "TEMP field, top, bot, current, previous, next, other, mov;"
                "TEX next, tex, texture[0], %1;"
                "TEX current, tex, texture[1], %1;"
                "TEX previous, tex, texture[2], %1;"
                "ADD top, tex, off.wyww;"
                "TEX other, top, texture[1], %1;"
                "SUB top, tex, off.wyww;"
                "TEX bot, top, texture[1], %1;"
                "LRP other, 0.5, other, bot;"
                "RCP field, off.x;"
                "MUL field, tex.yyyy, field;"
                "FRC field, field;"
                "SUB field, field, 0.5;"
                "SUB top, current, next;"
                "SUB bot, current, previous;"
                "CMP mov, field, bot, top;"
                "ABS mov, mov;"
                "SUB mov, mov, 0.12;"
                "CMP other, mov, current, other;"
                "CMP top, field, other, current;"
                "CMP bot, field, current, other;"
                "CMP result.color, off.y, top, bot;";
            break;

        case kGLFilterOneFieldDeintDFR:
            ret +=
                "ATTRIB tex = fragment.texcoord[0];"
                "PARAM  off  = program.env[0];"
                "TEMP field, top, bot, current, previous, next, other, mov;"
                "TEX next, tex, texture[0], %1;"
                "TEX current, tex, texture[1], %1;"
                "TEX previous, tex, texture[2], %1;"
                "ADD top, tex, off.wyww;"
                "TEX other, top, texture[1], %1;"
                "RCP field, off.x;"
                "MUL field, tex.yyyy, field;"
                "FRC field, field;"
                "SUB field, field, 0.5;"
                "SUB top, current, next;"
                "SUB bot, current, previous;"
                "CMP mov, field, bot, top;"
                "ABS mov, mov;"
                "SUB mov, mov, 0.12;"
                "CMP other, mov, current, other;"
                "CMP top, field, other, current;"
                "CMP bot, field, current, other;"
                "CMP result.color, off.y, top, bot;";
            break;

        case kGLFilterKernelDeintDFR:
            ret +=
                "ATTRIB tex = fragment.texcoord[0];"
                "PARAM  off = program.env[0];"
                "TEMP sam, pos, bot, top, cur, pre, nex, field, mov;"
                "RCP field, off.x;"
                "MUL field, tex.yyyy, field;"
                "FRC field, field;"
                "SUB field, field, 0.5;"
                "TEX pre, tex, texture[2], %1;" // -1,0
                "TEX cur, tex, texture[1], %1;" //  0,0
                "TEX nex, tex, texture[0], %1;" // +1,0
                "SUB top, nex, cur;"
                "SUB bot, pre, cur;"
                "CMP mov, field, bot, top;"
                "ABS mov, mov;"
                "SUB mov, mov, 0.12;"
                "MUL bot, pre, 0.125;"          // BOT -1,0
                "MAD bot, cur, 0.125, bot;"     // BOT +1,0
                "MUL top, cur, 0.125;"          // TOP -1,0
                "MAD top, nex, 0.125, top;"     // TOP +1,0
                "ADD pos, tex, off.wyww;"
                "TEX sam, pos, texture[1], %1;" // 0,+1
                "MAD bot, sam, 0.5, bot;"       // BOT 0,+1
                "MAD top, sam, 0.5, top;"       // TOP 0,+1
                "SUB pos, tex, off.wyww;"
                "TEX sam, pos, texture[1], %1;" // 0,-1
                "MAD bot, sam, 0.5, bot;"       // BOT 0,-1
                "MAD top, sam, 0.5, top;"       // TOP 0,-1
                "MAD pos, off.wyww, 2.0, tex;"
                "TEX sam, pos, texture[1], %1;" // 0,+2
                "MAD bot, sam, -0.0625, bot;"   // BOT +1,+2
                "MAD top, sam, -0.0625, top;"   // TOP -1,+2
                "TEX sam, pos, texture[2], %1;" // -1,+2
                "MAD bot, sam, -0.0625, bot;"   // BOT -1,+2
                "TEX sam, pos, texture[0], %1;" // +1,+2
                "MAD top, sam, -0.0625, top;"   // TOP +1,+2
                "MAD pos, off.wyww, -2.0, tex;"
                "TEX sam, pos, texture[1], %1;" // +1,-2
                "MAD bot, sam, -0.0625, bot;"   // BOT +1,-2
                "MAD top, sam, -0.0625, top;"   // TOP -1,-2
                "TEX sam, pos, texture[2], %1;" // -1, -2 row
                "MAD bot, sam, -0.0625, bot;"   // BOT -1,-2
                "TEX sam, pos, texture[0], %1;" // +1,-2
                "MAD top, sam, -0.0625, top;"   // TOP +1,-2
                "CMP top, mov, cur, top;"
                "CMP bot, mov, cur, bot;"
                "CMP top, field, top, cur;"
                "CMP bot, field, cur, bot;"
                "CMP result.color, off.y, top, bot;";
            break;

        case kGLFilterBobDeintDFR:
        case kGLFilterOneFieldDeint:
            ret +=
                "ATTRIB tex = fragment.texcoord[0];"
                "PARAM  off = program.env[0];"
                "TEMP field, top, bottom, current, other;"
                "TEX current, tex, texture[0], %1;"
                "RCP field, off.x;"
                "MUL field, tex.yyyy, field;"
                "FRC field, field;"
                "SUB field, field, 0.5;"
                "ADD top, tex, off.wyww;"
                "TEX other, top, texture[0], %1;"
                "CMP top, field, other, current;"
                "CMP bottom, field, current, other;"
                "CMP result.color, off.y, top, bottom;";
            break;

        case kGLFilterLinearBlendDeint:
            ret +=
                "ATTRIB tex = fragment.texcoord[0];"
                "PARAM  off  = program.env[0];"
                "TEMP mov, field, cur, pre, pos;"
                "RCP field, off.x;"
                "MUL field, tex.yyyy, field;"
                "FRC field, field;"
                "SUB field, field, 0.5;"
                "TEX cur, tex, texture[0], %1;"
                "TEX pre, tex, texture[1], %1;"
                "SUB mov, cur, pre;"
                "ABS mov, mov;"
                "SUB mov, mov, 0.12;"
                "ADD pos, tex, off.wyww;"
                "TEX pre, pos, texture[0], %1;"
                "SUB pos, tex, off.wyww;"
                "TEX pos, pos, texture[0], %1;"
                "LRP pre, 0.5, pos, pre;"
                "CMP pre, field, pre, cur;"
                "CMP result.color, mov, cur, pre;";
            break;

        case kGLFilterFieldOrderDFR:
            ret +=
                "ATTRIB tex = fragment.texcoord[0];"
                "PARAM  off  = program.env[0];"
                "TEMP field, cur, pre, bot;"
                "TEX cur, tex, texture[0], %1;"
                "TEX pre, tex, texture[1], %1;"
                "RCP field, off.x;"
                "MUL field, tex.yyyy, field;"
                "FRC field, field;"
                "SUB field, field, 0.5;"
                "CMP bot, off.y, pre, cur;"
                "CMP result.color, field, bot, cur;";

            break;

        case kGLFilterNone:
        case kGLFilterResize:
            break;

        default:
            VERBOSE(VB_PLAYBACK, LOC_ERR + "Unknown fragment program.");
            break;
    }

    return ret + "END";
}
