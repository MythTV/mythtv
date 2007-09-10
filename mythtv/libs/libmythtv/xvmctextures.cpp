#ifdef USING_XVMC_OPENGL

#include "XvMCSurfaceTypes.h"
#include "xvmctextures.h"
#include "osd.h"
#include "osdsurface.h"
#include "frame.h"

#define LOC QString("XvMCTex: ")
#define LOC_ERR QString("XvMCTex, Error: ")

static vector<GLuint> create_textures(
    Display     *XJ_disp,
    GLXWindow    glx_window,   GLXContext  glx_context,
    GLXPbuffer   glx_pbuffer,  const QSize &tex_size,   
    uint         num_of_textures);

XvMCTextures *XvMCTextures::Create(Display *XJ_disp, Window XJ_curwin,
                                   int XJ_screen_num,
                                   const QSize &video_dim,
                                   const QSize &window_size)
{
    XvMCTextures *tmp = new XvMCTextures();
    if (tmp && tmp->Init(XJ_disp, XJ_curwin, XJ_screen_num,
                         video_dim, window_size))
    {
        return tmp;
    }

    if (tmp)
        delete tmp;

    return NULL;
}

bool XvMCTextures::Init(Display *disp, Window XJ_curwin,
                        int XJ_screen_num,
                        const QSize &video_dim,
                        const QSize &window_size)
{
    VERBOSE(VB_IMPORTANT, LOC + "Init");

    if (!disp)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Init() no display!");
        return false;
    }

    XJ_disp = disp;

    if (!init_opengl())
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR + "Failed to initialize OpenGL support.");

        return false;
    }

    uint major = 0, minor = 0;
    if (!get_glx_version(XJ_disp, major, minor))
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR + "GLX extension not present.");

        return false;
    }

    if ((1 == major) && (minor < 3))
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR + QString(
                    "Need GLX 1.3 or better, have %1.%2")
                .arg(major).arg(minor));

        return false;
    }

    if (!glx_fbconfig)
    {
        glx_fbconfig = get_fbuffer_cfg(
            XJ_disp, XJ_screen_num, get_attr_cfg(kRenderRGBA));
    }

    if (glx_fbconfig)
        glx_pbuffer = get_pbuffer(XJ_disp, glx_fbconfig, video_dim);

    if (!glx_context)
        X11S(glx_context = glXCreateNewContext(XJ_disp, glx_fbconfig,
                                               GLX_RGBA_TYPE, NULL, 1));

    XVisualInfo *vis_info;
    vis_info = glXGetVisualFromFBConfig(XJ_disp, glx_fbconfig);
    gl_window = get_gl_window(XJ_disp, XJ_curwin, vis_info,
                              window_size, true);

    glx_window = get_glx_window(XJ_disp, glx_fbconfig, gl_window, glx_context,
                                glx_pbuffer, window_size);

    glXMakeContextCurrent(XJ_disp, glx_window, glx_pbuffer, glx_context);
    glCheck();
    glXMakeContextCurrent(XJ_disp, None, None, NULL);

    gl_vid_textures = create_textures(
        XJ_disp, glx_window, glx_context, glx_pbuffer, video_dim, 1);
    gl_vid_tex_size.resize(gl_vid_textures.size());

    gl_osd_textures = create_textures(
        XJ_disp, glx_window, glx_context, glx_pbuffer, video_dim, 1);
    gl_osd_tex_size.resize(gl_vid_textures.size());

    XSync(XJ_disp, 0);

    gl_display_size = window_size;
    gl_video_size   = video_dim;

    glXMakeContextCurrent(XJ_disp, glx_window, glx_pbuffer, glx_context);
    glCheck();
    glXMakeContextCurrent(XJ_disp, None, None, NULL);

    VERBOSE(VB_IMPORTANT, LOC +
            QString("InitXvMCGL: video_size: %1x%2  vis_size: %3x%4")
            .arg(gl_video_size.width()).arg(gl_video_size.height())
            .arg(gl_display_size.width()).arg(gl_display_size.height()));

    VERBOSE(VB_IMPORTANT, LOC <<endl
            <<"glx_fbconfig: "<<glx_fbconfig<<endl
            <<"gl_window:    "<<gl_window<<endl
            <<"glx_window:   "<<glx_window<<endl
            <<"gl_vid_tex:   "<<gl_vid_textures[0]<<endl
            <<"gl_osd_tex:   "<<gl_osd_textures[0]<<endl);

    return true;
}

void XvMCTextures::DeInit(void)
{
    VERBOSE(VB_IMPORTANT, LOC + "DeInit");
    if (!XJ_disp)
        return; // already called

    X11L;
    glXMakeContextCurrent(XJ_disp, None, None, NULL);
    glXDestroyContext(XJ_disp, glx_context); glx_context = 0;
    glXDestroyWindow( XJ_disp, glx_window);  glx_window  = 0;
    glXDestroyPbuffer(XJ_disp, glx_pbuffer); glx_pbuffer = 0;
    XDestroyWindow(   XJ_disp, gl_window);   gl_window   = 0;
    gl_vid_textures.clear();
    gl_osd_textures.clear();
    XJ_disp = NULL;
    X11U;
}

bool XvMCTextures::ProcessOSD(OSD *osd)
{
    OSDSurface *osdsurf = NULL;

    if (osd)
        osdsurf = osd->Display();

    if (!osdsurf || gl_osd_textures.empty() ||
        gl_osd_revision == osdsurf->GetRevision())
    {
        if (!osdsurf && gl_osd_visible)
        {
            gl_osd_visible = false;
            VERBOSE(VB_IMPORTANT, "OSD not visible");
            return true;
        }
        return false;
    }

    uint bpl = gl_video_size.width() << 2;
    uint sz  = bpl * gl_video_size.height();
    unsigned char *pixels = gl_osd_tmp_buf;
    if (sz > gl_osd_tmp_buf_size)
    {
        pixels = new unsigned char[sz + 256];
        if (gl_osd_tmp_buf)
            delete [] gl_osd_tmp_buf;
        gl_osd_tmp_buf = pixels;
        gl_osd_tmp_buf_size = sz;
    }
    bzero(pixels, sz * sizeof(unsigned char));

    osdsurf->BlendToARGB(pixels, bpl, gl_video_size.height(),
                         false/*blend_to_black*/, 0);

    for (uint i = 0; i < sz; i += 4)
    {
        unsigned char tmp[4] = 
            { pixels[i+0], pixels[i+1], pixels[i+2], pixels[i+3] };
            
        pixels[i+0] = tmp[2];  // red
        //pixels[i+1] = tmp[1];  // green
        pixels[i+2] = tmp[0];  // blue
        pixels[i+3] = tmp[3];  // alpha
    }

    X11L;
    GLuint rect_type = GL_TEXTURE_RECTANGLE_NV;

    glXMakeContextCurrent(XJ_disp, glx_window, glx_pbuffer, glx_context);

    glBindTexture(rect_type, gl_osd_textures[gl_osd_tex_index]);
    glTexParameterf(rect_type, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameterf(rect_type, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameterf(rect_type, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(rect_type, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glTexImage2D(rect_type, 0, GL_RGBA8,
                 gl_video_size.width(), gl_video_size.height(),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    glXMakeContextCurrent(XJ_disp, None, None, NULL);

    gl_osd_revision = osdsurf->GetRevision();
    gl_osd_tex_size[gl_osd_tex_index] = gl_video_size;

    X11U;

    if (!gl_osd_visible)
        VERBOSE(VB_IMPORTANT, "OSD visible");

    gl_osd_visible = true;

    return true;
}

void XvMCTextures::PrepareFrame(XvMCSurface *surf,
                                const QRect &video_rect, int scan)
{
    int field = (scan <= 0) ? 1 : scan;

    X11L;

    glXMakeContextCurrent(XJ_disp, glx_window, glx_pbuffer, glx_context);

    /////

    int h = gl_video_size.height();
    if (XVMC_FRAME_PICTURE != field)
        h = gl_video_size.height() / 2;

    uint tex_idx = gl_vid_tex_index;

    glBindTexture(GL_TEXTURE_RECTANGLE_NV,
                  gl_vid_textures[tex_idx]);

    XvMCCopySurfaceToGLXPbuffer(XJ_disp, surf, glx_pbuffer, 0, 0,
                                gl_video_size.width(), h,
                                0, 0, GL_FRONT_LEFT, field);

    glCopyTexSubImage2D(GL_TEXTURE_RECTANGLE_NV, 0, 0, 0, 0, 0,
                        gl_video_size.width(), h);

    gl_vid_tex_size[tex_idx] = QSize(gl_video_size.width(), h);

    /////

    last_video_rect = video_rect;

    /////

    glFlush();

    glXMakeContextCurrent(XJ_disp, None, None, NULL);

    X11U;
}

void XvMCTextures::CompositeFrameAndOSD(int scan)
{
    if (last_video_rect.width() <= 0)
        return;

    QRect video_rect = last_video_rect;

    int field = (scan <= 0) ? 1 : scan;

    float bob_delta = 0.0f;
    if (XVMC_FRAME_PICTURE != field)
    {
        bob_delta = -0.25f / gl_vid_tex_size[gl_vid_tex_index].height();
        if (XVMC_BOTTOM_FIELD == field)
            bob_delta = -bob_delta;
    }

    float xobeg = video_rect.left();
    float xoend = video_rect.right();
    float yobeg = video_rect.top();
    float yoend = video_rect.bottom();

    glClearColor (0.0, 0.0, 0.0, 0.5);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    {
        float xbeg = 0.0f;
        float xend = gl_vid_tex_size[gl_vid_tex_index].width();
        float ybeg = bob_delta;
        float yend = bob_delta + gl_vid_tex_size[gl_vid_tex_index].height();

        glBindTexture(GL_TEXTURE_RECTANGLE_NV,
                      gl_vid_textures[gl_vid_tex_index]);
        glBegin(GL_QUADS);
        glTexCoord2f(xbeg, ybeg); glVertex3f(xobeg, yobeg, +0.0f);
        glTexCoord2f(xbeg, yend); glVertex3f(xobeg, yoend, +0.0f);
        glTexCoord2f(xend, yend); glVertex3f(xoend, yoend, +0.0f);
        glTexCoord2f(xend, ybeg); glVertex3f(xoend, yobeg, +0.0f);
        glEnd();
    }

    if (gl_osd_visible)
    {
        float xbeg = 0.0f;
        float ybeg = 0.0f;
        float xend = gl_osd_tex_size[gl_osd_tex_index].width();
        float yend = gl_osd_tex_size[gl_osd_tex_index].height();

        glBindTexture(GL_TEXTURE_RECTANGLE_NV,
                      gl_osd_textures[gl_osd_tex_index]);

        glBegin(GL_QUADS);
        glTexCoord2f(xend, ybeg); glVertex3f(xoend, yoend, -1.0f);
        glTexCoord2f(xend, yend); glVertex3f(xoend, yobeg, -1.0f);
        glTexCoord2f(xbeg, yend); glVertex3f(xobeg, yobeg, -1.0f);
        glTexCoord2f(xbeg, ybeg); glVertex3f(xobeg, yoend, -1.0f);
        glEnd();
    }
}

void XvMCTextures::Show(int scan)
{
    X11L;

    glXMakeContextCurrent(XJ_disp, glx_window, glx_pbuffer, glx_context);

    CompositeFrameAndOSD(scan);

    glFinish();
    glXSwapBuffers(XJ_disp, glx_window);

    glXMakeContextCurrent(XJ_disp, None, None, NULL);

    X11U;
}

static vector<GLuint> create_textures(
    Display     *XJ_disp,
    GLXWindow    glx_window,   GLXContext  glx_context,
    GLXPbuffer   glx_pbuffer,  const QSize &tex_size,   
    uint         num_of_textures)
{
    GLuint rect_type = GL_TEXTURE_RECTANGLE_NV;
    vector<GLuint> tex;
    tex.resize(num_of_textures);

    X11L;

    glXMakeContextCurrent(XJ_disp, glx_window, glx_pbuffer, glx_context);

    glGenTextures(num_of_textures, &tex[0]);

    //glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    for (uint i = 0; i < num_of_textures; i++)
    {    
        glBindTexture(rect_type, tex[i]);
        glTexParameterf(rect_type, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameterf(rect_type, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glTexParameterf(rect_type, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameterf(rect_type, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        //glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
        glTexImage2D(rect_type, 0, GL_RGB8/*3*/,
                     tex_size.width(), tex_size.height(),
                     0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    }

    glXMakeContextCurrent(XJ_disp, None, None, NULL);

    X11U;

    return tex;
}

#endif // USING_XVMC_OPENGL
