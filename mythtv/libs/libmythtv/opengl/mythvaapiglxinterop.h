#ifndef MYTHVAAPIGLXNTEROP_H
#define MYTHVAAPIGLXNTEROP_H

// MythTV
#include "mythvaapiinterop.h"

class MythVAAPIInteropGLX : public MythVAAPIInterop
{
    Q_OBJECT

  public:
    MythVAAPIInteropGLX(MythRenderOpenGL* Context, Type InteropType);
    ~MythVAAPIInteropGLX() override;

  public slots:
    int  SetPictureAttribute(PictureAttribute Attribute, int Value);

  protected:
    uint GetFlagsForFrame(MythVideoFrame* Frame, FrameScanType Scan);
    void InitPictureAttributes(MythVideoColourSpace* ColourSpace);

  protected:
    VADisplayAttribute* m_vaapiPictureAttributes     { nullptr };
    int                 m_vaapiPictureAttributeCount { 0 };
    uint                m_vaapiColourSpace           { 0 };
    MythDeintType       m_basicDeinterlacer          { DEINT_NONE };
};

class MythVAAPIInteropGLXCopy : public MythVAAPIInteropGLX
{
  public:
    explicit MythVAAPIInteropGLXCopy(MythRenderOpenGL* Context);
    ~MythVAAPIInteropGLXCopy() override;
    vector<MythVideoTextureOpenGL*> Acquire(MythRenderOpenGL* Context,
                                            MythVideoColourSpace* ColourSpace,
                                            MythVideoFrame* Frame, FrameScanType Scan) override;

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
    explicit MythVAAPIInteropGLXPixmap(MythRenderOpenGL* Context);
    ~MythVAAPIInteropGLXPixmap() override;
    vector<MythVideoTextureOpenGL*> Acquire(MythRenderOpenGL* Context,
                                            MythVideoColourSpace* ColourSpace,
                                            MythVideoFrame* Frame, FrameScanType Scan) override;
    static bool IsSupported(MythRenderOpenGL* Context);

  private:
    bool        InitPixmaps();

    // Texture from Pixmap
    Pixmap                     m_pixmap                { 0 };
    GLXPixmap                  m_glxPixmap             { 0 };
    MYTH_GLXBINDTEXIMAGEEXT    m_glxBindTexImageEXT    { nullptr };
    MYTH_GLXRELEASETEXIMAGEEXT m_glxReleaseTexImageEXT { nullptr };
};


#endif // MYTHVAAPIGLXNTEROP_H
