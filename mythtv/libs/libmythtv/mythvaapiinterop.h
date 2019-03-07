#ifndef MYTHVAAPIINTEROP_H
#define MYTHVAAPIINTEROP_H

// Qt
#include <QFile>

// MythTV
#include "mythopenglinterop.h"

// VAAPI
#include "va/va.h"
#include "va/va_version.h"
#if VA_CHECK_VERSION(0,34,0)
#include "va/va_compat.h"
#endif
#include "va/va_x11.h"
#include "va/va_glx.h"
#include "va/va_drm.h"
#include "va/va_drmcommon.h"

class MythVAAPIInterop : public MythOpenGLInterop
{
  public:
    static MythVAAPIInterop* Create(MythRenderOpenGL *Context, Type InteropType);
    static Type GetInteropType(MythCodecID CodecId, MythRenderOpenGL *Context = nullptr);

    MythVAAPIInterop(MythRenderOpenGL *Context, Type InteropType);
    virtual ~MythVAAPIInterop() override;
    VASurfaceID VerifySurface(MythRenderOpenGL *Context, VideoFrame *Frame);
    VADisplay   GetDisplay   (void);
    QString     GetVendor    (void);

  protected:
    void IniitaliseDisplay   (void);

  protected:
    VADisplay           m_vaDisplay;
    QString             m_vaVendor;
};

class MythVAAPIInteropGLX : public QObject, public MythVAAPIInterop
{
    Q_OBJECT

  public:
    MythVAAPIInteropGLX(MythRenderOpenGL *Context, Type InteropType);
    virtual ~MythVAAPIInteropGLX() override;

  public slots:
    int  SetPictureAttribute(PictureAttribute Attribute, int Value);

  protected:
    uint GetFlagsForFrame(VideoFrame *Frame, FrameScanType Scan);
    void InitPictureAttributes(VideoColourSpace *ColourSpace);

  protected:
    VASurfaceID         m_lastSurface;
    VADisplayAttribute *m_vaapiPictureAttributes;
    int                 m_vaapiPictureAttributeCount;
    int                 m_vaapiHueBase;
    uint                m_vaapiColourSpace;
};

class MythVAAPIInteropGLXCopy : public MythVAAPIInteropGLX
{
  public:
    MythVAAPIInteropGLXCopy(MythRenderOpenGL *Context);
    ~MythVAAPIInteropGLXCopy() override;
    vector<MythGLTexture*> Acquire(MythRenderOpenGL *Context,
                                   VideoColourSpace *ColourSpace,
                                   VideoFrame *Frame, FrameScanType Scan) final override;

  private:
    void* m_glxSurface;
};

#include "va/va_x11.h"
#include "GL/glx.h"
#include "GL/glxext.h"
typedef void ( * MYTH_GLXBINDTEXIMAGEEXT)(Display*, GLXDrawable, int, int*);
typedef void ( * MYTH_GLXRELEASETEXIMAGEEXT)(Display*, GLXDrawable, int);

class MythVAAPIInteropGLXPixmap : public MythVAAPIInteropGLX
{
  public:
    MythVAAPIInteropGLXPixmap(MythRenderOpenGL *Context);
    ~MythVAAPIInteropGLXPixmap() override;
    vector<MythGLTexture*> Acquire(MythRenderOpenGL *Context,
                                   VideoColourSpace *ColourSpace,
                                   VideoFrame *Frame, FrameScanType Scan) final override;
    static bool IsSupported(MythRenderOpenGL *Context);

  private:
    bool        InitPixmaps(void);

    // Texture from Pixmap
    Pixmap                     m_pixmap;
    GLXPixmap                  m_glxPixmap;
    MYTH_GLXBINDTEXIMAGEEXT    m_glxBindTexImageEXT;
    MYTH_GLXRELEASETEXIMAGEEXT m_glxReleaseTexImageEXT;
};

#include <EGL/egl.h>
#include <EGL/eglext.h>
typedef void  ( * MYTH_EGLIMAGETARGET)  (GLenum, EGLImage);
typedef EGLImageKHR ( * MYTH_EGLCREATEIMAGE)  (EGLDisplay, EGLContext, EGLenum, EGLClientBuffer, const EGLint *);
typedef void  ( * MYTH_EGLDESTROYIMAGE) (EGLDisplay, EGLImage);

class MythVAAPIInteropDRM : public MythVAAPIInterop
{
  public:
    MythVAAPIInteropDRM(MythRenderOpenGL *Context);
    ~MythVAAPIInteropDRM() override;
    vector<MythGLTexture*> Acquire(MythRenderOpenGL *Context,
                                   VideoColourSpace *ColourSpace,
                                   VideoFrame *Frame,FrameScanType Scan) final override;
    static bool    IsSupported(MythRenderOpenGL *Context);

  private:
    MythGLTexture* GetDRMTexture(uint Plane, uint TotalPlanes,
                                 QSize Size, uintptr_t Handle, VAImage &Image);
    bool           InitEGL(void);

  private:
    QFile                m_drmFile;
    MYTH_EGLIMAGETARGET  m_eglImageTargetTexture2DOES;
    MYTH_EGLCREATEIMAGE  m_eglCreateImageKHR;
    MYTH_EGLDESTROYIMAGE m_eglDestroyImageKHR;
};

#endif // MYTHVAAPIINTEROP_H
