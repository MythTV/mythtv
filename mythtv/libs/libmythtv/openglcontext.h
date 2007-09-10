#ifndef _OPENGL_CONTEXT_H_
#define _OPENGL_CONTEXT_H_

// Std C++ headers
#include <vector>
using namespace std;

// Qt headers
#include <qstring.h>
#include <qrect.h>

// MythTV headers
#include "util-x11.h"

class OpenGLVideo;
class PrivateContext;

typedef enum
{
    kGLExtRect     = 0x01,
    kGLExtFragProg = 0x02,
    kGLExtFBufObj  = 0x04,
    kGLXPBuffer    = 0x08,
} GLFeatures;

#ifdef USING_OPENGL

class OpenGLContext
{
  public:
    OpenGLContext();
    ~OpenGLContext();

    bool Create(Display *display, Window window, uint screen_num,
                const QSize &display_visible_size, bool visible);
    void Hide(void);
    void Show(void);

    bool MakeCurrent(bool current);
    void SwapBuffers(void);
    void Flush(void);

    uint GetMaxTexSize(void) const { return m_max_tex_size; }
    uint GetScreenNum(void)  const { return m_screen_num;   }

    uint CreateTexture(void);
    bool SetupTexture(const QSize &size, uint tex, int filt);
    void SetupTextureFilters(uint tex, int filt);
    void DeleteTexture(uint tex);
    int  GetTextureType(void) const;
    void EnableTextures(void);

    bool CreateFragmentProgram(const QString &program, uint &prog);
    void DeleteFragmentProgram(uint prog);
    void BindFragmentProgram(uint fp);
    void InitFragmentParams(uint fp, float a, float b, float c, float d);

    bool CreateFrameBuffer(uint &fb, uint tex, const QSize &size);
    void DeleteFrameBuffer(uint fb);
    void BindFramebuffer(uint fb);

    bool IsFeatureSupported(GLFeatures feature) const
        { return m_ext_supported & feature; }

    static bool IsGLXSupported(Display *display, uint major, uint minor);

  private:
    bool IsGLXSupported(uint major, uint minor) const
    {
        return (m_major_ver > major) ||
            ((m_major_ver == major) && (m_minor_ver >= minor));
    }

    void DeleteTextures(void);
    void DeletePrograms(void);
    void DeleteFrameBuffers(void);

    PrivateContext *m_priv;

    Display        *m_display;
    uint            m_screen_num;
    uint            m_major_ver;
    uint            m_minor_ver;
    QString         m_extensions;
    uint            m_ext_supported;
    bool            m_visible;
    uint            m_max_tex_size;
};

#else // if !USING_OPENGL

class OpenGLContext
{
  public:
    OpenGLContext() { }
    ~OpenGLContext() { }

    bool Create(Display*, Window, uint, const QSize&, bool) { return false; }

    bool MakeCurrent(bool) { return false; }
    void SwapBuffers(void) { }
    void Flush(void) { }

    uint GetMaxTexSize(void) const { return 0; }
    uint GetScreenNum(void)  const { return 0; }

    uint CreateTexture(void) { return 0; }
    bool SetupTexture(const QSize&, uint, int) { return false; }
    void SetupTextureFilters(uint, int) { }
    void DeleteTexture(uint) { }
    int  GetTextureType(void) const { return 0; }
    void EnableTextures(void) { }

    bool CreateFragmentProgram(const QString&, uint&) { return false; }
    void DeleteFragmentProgram(uint) { }
    void BindFragmentProgram(uint) { }
    void InitFragmentParams(uint, float, float, float, float) { }

    bool CreateFrameBuffer(uint&, uint, const QSize&) { return false; }
    void DeleteFrameBuffer(uint);
    void BindFramebuffer(uint);

    bool IsFeatureSupported(GLFeatures) const { return false; }

    static bool IsGLXSupported(Display*, uint, uint) { return false; }
};

#endif //!USING_OPENGL

#endif // _OPENGL_CONTEXT_H_
