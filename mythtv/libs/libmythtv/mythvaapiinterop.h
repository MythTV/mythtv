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

#ifndef VA_FOURCC_I420
#define VA_FOURCC_I420 0x30323449
#endif

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

    static bool SetupDeinterlacer (MythDeintType Deinterlacer, bool DoubleRate,
                                   AVBufferRef *FramesContext, int Width, int Height,
                                   AVFilterGraph *&Graph, AVFilterContext *&Source,
                                   AVFilterContext *&Sink);

  protected:
    void IniitaliseDisplay   (void);
    VASurfaceID Deinterlace  (VideoFrame *Frame, VASurfaceID Current, FrameScanType Scan);
    virtual void DestroyDeinterlacer (void);
    virtual void PostInitDeinterlacer(void) { }

  protected:
    VADisplay        m_vaDisplay         { nullptr };
    QString          m_vaVendor          { };

    MythDeintType    m_deinterlacer      { DEINT_NONE };
    bool             m_deinterlacer2x    { false      };
    AVBufferRef     *m_vppFramesContext  { nullptr };
    AVFilterContext *m_filterSink        { nullptr };
    AVFilterContext *m_filterSource      { nullptr };
    AVFilterGraph   *m_filterGraph       { nullptr };
    bool             m_filterError       { false   };
    int              m_filterWidth       { 0 };
    int              m_filterHeight      { 0 };
    VASurfaceID      m_lastFilteredFrame { 0 };
    long long        m_lastFilteredFrameCount { 0 };
};

class MythVAAPIInteropGLX : public MythVAAPIInterop
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
    VADisplayAttribute *m_vaapiPictureAttributes;
    int                 m_vaapiPictureAttributeCount;
    int                 m_vaapiHueBase;
    uint                m_vaapiColourSpace;
    MythDeintType       m_basicDeinterlacer;
};

class MythVAAPIInteropGLXCopy : public MythVAAPIInteropGLX
{
  public:
    MythVAAPIInteropGLXCopy(MythRenderOpenGL *Context);
    ~MythVAAPIInteropGLXCopy() override;
    vector<MythVideoTexture*> Acquire(MythRenderOpenGL *Context,
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
    vector<MythVideoTexture*> Acquire(MythRenderOpenGL *Context,
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
    vector<MythVideoTexture*> Acquire(MythRenderOpenGL *Context,
                                      VideoColourSpace *ColourSpace,
                                      VideoFrame *Frame,FrameScanType Scan) final override;
    static bool    IsSupported(MythRenderOpenGL *Context);
    void           DeleteTextures(void) override;

  protected:
    void           DestroyDeinterlacer(void) override;
    void           PostInitDeinterlacer(void) override;

  private:
    void           CreateDRMBuffers(VideoFrameType Format,
                                    vector<MythVideoTexture*> Textures,
                                    uintptr_t Handle, VAImage &Image);
    bool           InitEGL(void);
    VideoFrameType VATypeToMythType(uint32_t Fourcc);
    void           CleanupReferenceFrames(void);
    void           RotateReferenceFrames(AVBufferRef *Buffer);
    vector<MythVideoTexture*> GetReferenceFrames(void);

  private:
    QFile                m_drmFile;
    MYTH_EGLIMAGETARGET  m_eglImageTargetTexture2DOES;
    MYTH_EGLCREATEIMAGE  m_eglCreateImageKHR;
    MYTH_EGLDESTROYIMAGE m_eglDestroyImageKHR;
    QVector<AVBufferRef*> m_referenceFrames;
};

#endif // MYTHVAAPIINTEROP_H
