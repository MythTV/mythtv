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
    VADisplayAttribute *m_vaapiPictureAttributes     { nullptr };
    int                 m_vaapiPictureAttributeCount { 0 };
    int                 m_vaapiHueBase               { 0 };
    uint                m_vaapiColourSpace           { 0 };
    MythDeintType       m_basicDeinterlacer          { DEINT_NONE };
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
    void* m_glxSurface { nullptr };
};

#include "GL/glx.h"
#include "GL/glxext.h"
using MYTH_GLXBINDTEXIMAGEEXT = void (*)(Display*, GLXDrawable, int, int*);
using MYTH_GLXRELEASETEXIMAGEEXT = void (*)(Display*, GLXDrawable, int);

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
    Pixmap                     m_pixmap                { 0 };
    GLXPixmap                  m_glxPixmap             { 0 };
    MYTH_GLXBINDTEXIMAGEEXT    m_glxBindTexImageEXT    { nullptr };
    MYTH_GLXRELEASETEXIMAGEEXT m_glxReleaseTexImageEXT { nullptr };
};


#endif // MYTHVAAPIGLXNTEROP_H
