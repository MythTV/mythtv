#ifndef _OPENGL_CONTEXT_H_
#define _OPENGL_CONTEXT_H_

// Std C++ headers
#include <vector>
using namespace std;

// Qt headers
#include <qstring.h>
#include <qrect.h>

#ifdef USING_X11
// MythTV headers
#include "mythxdisplay.h"
#define GLX_GLXEXT_PROTOTYPES
#include <GL/glx.h>
#endif

#ifdef Q_WS_MACX
#include "util-osx.h"
#import <agl.h>
#include "mythxdisplay.h"  // For GetNumberXineramaScreens()
#endif

#ifdef USING_MINGW
#include <windows.h>
#endif

#include "frame.h"
#include "videooutbase.h"

#ifndef GL_BGRA
#define GL_BGRA                           0x80E1
#endif
#ifndef GL_UNSIGNED_BYTE
#define GL_UNSIGNED_BYTE                  0x1401
#endif
#ifndef GL_RGBA8
#define GL_RGBA8                          0x8058
#endif
#ifndef GL_LINEAR
#define GL_LINEAR                         0x2601
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE                  0x812F
#endif

class OpenGLVideo;
class PrivateContext;

typedef enum
{
    kGLExtRect     = 0x0001,
    kGLExtFragProg = 0x0002,
    kGLExtFBufObj  = 0x0004,
    kGLXPBuffer    = 0x0008,
    kGLExtPBufObj  = 0x0010,
    kGLNVFence     = 0x0020,
    kGLAppleFence  = 0x0040,
    kGLFinish      = 0x0080,
    kGLGLXSwap     = 0x0100,
    kGLGLXVSync    = 0x0200,
    kGLWGLSwap     = 0x0400,
    kGLAGLSwap     = 0x0800,
    kGLYCbCrTex    = 0x1000,
    kGLMaxFeat     = 0x2000,
} GLFeatures;

class OpenGLContext;

class OpenGLContextLocker
{
    public:
        OpenGLContextLocker(OpenGLContext *ctx);
        ~OpenGLContextLocker();

    private:
        OpenGLContext *m_ctx;
};

class OpenGLContext
{
  public:
    static OpenGLContext *Create(QMutex *lock);

    OpenGLContext(QMutex *lock);
    virtual ~OpenGLContext();
    virtual bool Create(WId window, const QRect &display_visible,
                        bool colour_control = false) = 0;
    virtual void Show(void) = 0;
    virtual void MapWindow(void) = 0;
    virtual void Hide(void) = 0;
    virtual void UnmapWindow(void) = 0;
    /// \brief Swap the front and back OpenGL framebuffers.
    virtual void SwapBuffers(void) = 0;
    /// \brief Return current display information
    virtual DisplayInfo GetDisplayInfo(void) = 0;
    /// \brief Set the number of screen refreshes between buffer swaps.
    virtual void SetSwapInterval(int interval) = 0;
    /// \brief Return the total number of screens available.
    virtual int  GetNumberOfScreens(void) { return 1; }
    virtual void MoveResizeWindow(QRect rect) { }
    virtual void EmbedInWidget(int x, int y, int w, int h) { }
    virtual void StopEmbedding(void) { }

    /// \brief Return the size of the OpenGL window.
    void GetWindowRect(QRect &rect) { rect = m_window_rect; }
    bool MakeCurrent(bool current);
    void Flush(bool use_fence);
    void SetViewPort(const QSize &size);
    void UpdateTexture(uint tex, const unsigned char *buf,
                       const int *offsets,
                       const int *pitches,
                       VideoFrameType fmt,
                       bool convert,
                       bool interlaced = FALSE,
                       const unsigned char* alpha = NULL);
    uint CreateTexture(QSize tot_size, QSize vid_size,
                       bool use_pbo, uint type,
                       uint data_type = GL_UNSIGNED_BYTE,
                       uint data_fmt = GL_BGRA,
                       uint internal_fmt = GL_RGBA8,
                       uint filter = GL_LINEAR,
                       uint wrap = GL_CLAMP_TO_EDGE);
    void SetTextureFilters(uint tex, uint filt, uint wrap);
    void DeleteTexture(uint tex);
    void GetTextureType(uint &current, bool &rect);
    void EnableTextures(uint type, uint tex_type = 0);
    void DisableTextures(void);

    bool CreateFragmentProgram(const QString &program, uint &prog);
    void DeleteFragmentProgram(uint prog);
    void EnableFragmentProgram(uint fp);
    void InitFragmentParams(uint fp, float a, float b, float c, float d);

    bool CreateFrameBuffer(uint &fb, uint tex);
    void DeleteFrameBuffer(uint fb);
    void BindFramebuffer(uint fb);
    uint GetFeatures(void) { return m_ext_supported; }
    void SetFeatures(uint features) { m_ext_used = features; }

    int SetPictureAttribute(PictureAttribute attributeType, int newValue);
    PictureAttributeSupported GetSupportedPictureAttributes(void) const;
    void SetColourParams(void);
    uint CreateHelperTexture(void);
    void ActiveTexture(uint active_tex);
    void SetFence(void);

  protected:
    /// \brief Platform dependant calls required to attach the context
    ///  to the current thread.
    virtual bool MakeContextCurrent(bool current) = 0;
    /// \brief Delete all resources associated with the windowing system.
    virtual void DeleteWindowResources(void) = 0;
    bool CreateCommon(bool colour_control, QRect display_visible);
    void DeleteOpenGLResources(void);
    void Init2DState(void);
    uint CreatePBO(uint tex);
    void DeleteTextures(void);
    void DeletePrograms(void);
    void DeleteFrameBuffers(void);
    uint GetBufferSize(QSize size, uint fmt, uint type);
    bool ClearTexture(uint tex);

    PrivateContext *m_priv;
    QString         m_extensions;
    uint            m_ext_supported;
    uint            m_ext_used;
    uint            m_max_tex_size;
    QSize           m_viewport;
    QMutex         *m_lock;
    int             m_lock_level;
    bool            m_colour_control;
    QRect           m_window_rect;

    float pictureAttribs[kPictureAttribute_MAX];
};

#ifdef USING_X11
class OpenGLContextGLX : public OpenGLContext
{
  public:
    OpenGLContextGLX(QMutex *lock);
    ~OpenGLContextGLX();

    bool Create(MythXDisplay *display, Window window, const QRect &display_visible,
                bool colour_control = false, bool map_window = true);
    static bool IsGLXSupported(MythXDisplay *display, uint major, uint minor);

    bool Create(WId window, const QRect &display_visible, bool colour_control = false);
    void Show(void);
    void MapWindow(void);
    void Hide(void);
    void UnmapWindow(void);
    bool MakeContextCurrent(bool current);
    void SwapBuffers(void);
    uint GetScreenNum(void)  const { return m_display->GetScreen(); }
    DisplayInfo GetDisplayInfo(void);
    void SetSwapInterval(int interval);
    int  GetNumberOfScreens(void) { return GetNumberXineramaScreens(); }
    void MoveResizeWindow(QRect rect);

  private:
    void DeleteWindowResources(void);
    bool IsGLXSupported(uint major, uint minor) const
    {
        return (m_major_ver > major) ||
            ((m_major_ver == major) && (m_minor_ver >= minor));
    }

    MythXDisplay *m_display;
    bool          m_created_display;
    uint          m_major_ver;
    uint          m_minor_ver;
    GLXFBConfig   m_glx_fbconfig;
    Window        m_gl_window;
    GLXWindow     m_glx_window;
    GLXContext    m_glx_context;
    XVisualInfo  *m_vis_info;
    int const    *m_attr_list;
};
#endif // USING_X11

#ifdef USING_MINGW
class OpenGLContextWGL : public OpenGLContext
{
  public:
    OpenGLContextWGL(QMutex *lock);
    ~OpenGLContextWGL();

    bool Create(WId window, const QRect &display_visible, bool colour_control = false);
    void Show(void) { }
    void MapWindow(void) { }
    void Hide(void) { }
    void UnmapWindow(void) { }
    bool MakeContextCurrent(bool current);
    void SwapBuffers(void);
    DisplayInfo GetDisplayInfo(void);
    void SetSwapInterval(int interval);

  private:
    void DeleteWindowResources(void);

    HDC   hDC;
    HGLRC hRC;
    HWND  hWnd;

};
#endif //USING_MINGW

#ifdef Q_WS_MACX
class OpenGLContextAGL : public OpenGLContext
{
  public:
    OpenGLContextAGL(QMutex *lock);
    ~OpenGLContextAGL();

    bool Create(WId window, const QRect &display_visible,
                bool colour_control = false);
    void Show(void) { }
    void MapWindow(void) { }
    void Hide(void) { }
    void UnmapWindow(void) { }
    bool MakeContextCurrent(bool current);
    void SwapBuffers(void);
    DisplayInfo GetDisplayInfo(void);
    void SetSwapInterval(int interval);
    int  GetNumberOfScreens(void) { return GetNumberXineramaScreens(); }
    void MoveResizeWindow(QRect rect);    
    void EmbedInWidget(int x, int y, int w, int h);
    void StopEmbedding(void);

  private:
    void DeleteWindowResources(void);

    WindowRef    m_window;
    AGLContext   m_context;
    CGDirectDisplayID  m_screen;
    CGrafPtr     m_port;
};
#endif //Q_WS_MACX
#endif // _OPENGL_CONTEXT_H_
