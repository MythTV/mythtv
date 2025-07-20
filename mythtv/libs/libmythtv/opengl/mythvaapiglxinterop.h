#ifndef MYTHVAAPIGLXNTEROP_H
#define MYTHVAAPIGLXNTEROP_H

// MythTV
#include "libmythui/opengl/mythrenderopengl.h"

#include "libmythtv/mythframe.h"
#include "libmythtv/mythplayerui.h"
#include "libmythtv/videoouttypes.h"

#include "mythvaapiinterop.h"

class MythVAAPIInteropGLX : public MythVAAPIInterop
{
    Q_OBJECT

  public:
    MythVAAPIInteropGLX(MythPlayerUI *Player, MythRenderOpenGL* Context, InteropType Type);
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
    MythVAAPIInteropGLXCopy(MythPlayerUI* Player, MythRenderOpenGL* Context);
    ~MythVAAPIInteropGLXCopy() override;

    std::vector<MythVideoTextureOpenGL*>
    Acquire(MythRenderOpenGL* Context,
            MythVideoColourSpace* ColourSpace,
            MythVideoFrame* Frame, FrameScanType Scan) override;

  private:
    void* m_glxSurface { nullptr };
};

#define Cursor XCursor // Prevent conflicts with Qt6.
#define pointer Xpointer // Prevent conflicts with Qt6.
#if defined(_X11_XLIB_H_) && !defined(Bool)
#define Bool int
#endif
#include <GL/glx.h>
#include <GL/glxext.h>
#undef None            // X11/X.h defines this. Causes compile failure in Qt6.
#undef Cursor
#undef pointer
#undef Bool            // Interferes with cmake moc file compilation
using MYTH_GLXBINDTEXIMAGEEXT = void (*)(Display*, GLXDrawable, int, int*);
using MYTH_GLXRELEASETEXIMAGEEXT = void (*)(Display*, GLXDrawable, int);

class MythVAAPIInteropGLXPixmap : public MythVAAPIInteropGLX
{
  public:
    MythVAAPIInteropGLXPixmap(MythPlayerUI* Player, MythRenderOpenGL* Context);
    ~MythVAAPIInteropGLXPixmap() override;

    std::vector<MythVideoTextureOpenGL*>
    Acquire(MythRenderOpenGL* Context,
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
