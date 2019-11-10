#ifndef MYTHVAAPIGLXNTEROP_H
#define MYTHVAAPIGLXNTEROP_H

// MythTV
#include "mythvaapiinterop.h"

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


#endif // MYTHVAAPIGLXNTEROP_H
