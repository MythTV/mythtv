// -*- Mode: c++ -*-

#ifndef _XVMC_TEXTURES_H_
#define _XVMC_TEXTURES_H_

#include <qsize.h>

class OSD;

#ifndef USING_XVMC_OPENGL

#ifdef USING_XVMC
#include <X11/extensions/XvMC.h>
#else
struct XvMCSurface;
#endif

class XvMCTextures
{
  public:
    XvMCTextures() {}
    ~XvMCTextures() {}

    static XvMCTextures *Create(
        Display*, Window, int, const QSize&, const QSize&)
        { return NULL; }

    bool Init(Display*, Window, int, const QSize&, const QSize&)
        { return false; }

    void PrepareFrame(XvMCSurface*, const QRect&, int) {}
    void DeInit(void) {}
    void Show(void) {}
    bool ProcessOSD(OSD*) { return false; }
};

#else // ! USING_XVMC_OPENGL

#ifdef USING_OPENGL
#include <GL/glx.h>
#include <GL/glu.h>
#endif

#include "mythcontext.h"
#include "util-x11.h"

static inline int __glCheck__(const QString &loc, const char* fileName, int n)
{
    int error = glGetError();
    if (error)
    { 
        VERBOSE(VB_IMPORTANT, loc << gluErrorString(error) << " @ "
                << fileName << ", #" << n);
    }
    return error;
}

#define glCheck() __glCheck__(LOC, __FILE__, __LINE__)

class XvMCTextures
{
  public:
    XvMCTextures() :
        XJ_disp(NULL),
        gl_video_size(0,0),     gl_display_size(0,0),
        gl_vid_tex_index(0),    gl_osd_tex_index(0),
        gl_osd_revision(-1),
        gl_osd_tmp_buf(NULL),   gl_osd_tmp_buf_size(0),
        gl_osd_visible(false),
        gl_window(0),           glx_window(0),
        glx_pbuffer(0),         glx_context(0),
        glx_fbconfig(0) {}
    ~XvMCTextures()
    {
        DeInit();
        if (gl_osd_tmp_buf)
            delete [] gl_osd_tmp_buf;
    }

    static XvMCTextures *Create(Display *XJ_disp, Window XJ_curwin,
                                int XJ_screen_num,
                                const QSize &video_dim,
                                const QSize &window_size);

    bool Init(Display *XJ_disp, Window XJ_curwin, int XJ_screen_num,
              const QSize &video_dim, const QSize &window_size);

    void DeInit(void);

    void PrepareFrame(XvMCSurface *surf, const QRect &video_rect, int field);

    void Show(void);

    bool ProcessOSD(OSD *osd);

    Display             *XJ_disp;
    QSize                gl_video_size;
    QSize                gl_display_size;
    vector<GLuint>       gl_vid_textures;
    vector<QSize>        gl_vid_tex_size;
    uint                 gl_vid_tex_index;
    vector<GLuint>       gl_osd_textures;
    vector<QSize>        gl_osd_tex_size;
    uint                 gl_osd_tex_index;
    int                  gl_osd_revision;
    unsigned char       *gl_osd_tmp_buf;
    uint                 gl_osd_tmp_buf_size;
    bool                 gl_osd_visible;
    Window               gl_window;
    GLXWindow            glx_window;
    GLXPbuffer           glx_pbuffer;
    GLXContext           glx_context;
    GLXFBConfig          glx_fbconfig;
  private:
};

#endif // USING_XVMC_OPENGL

#endif // _XVMC_TEXTURES_H_
