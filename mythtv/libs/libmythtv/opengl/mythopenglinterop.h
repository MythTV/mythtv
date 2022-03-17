#ifndef MYTHOPENGLINTEROP_H
#define MYTHOPENGLINTEROP_H

// Qt
#include <QObject>

// MythTV
#include "libmythbase/referencecounter.h"
#include "libmythui/opengl/mythrenderopengl.h"
#include "videoouttypes.h"
#include "mythframe.h"
#include "opengl/mythvideotextureopengl.h"
#include "mythinteropgpu.h"

class MythVideoColourSpace;

class MythOpenGLInterop : public MythInteropGPU
{
    Q_OBJECT

  public:
    static void GetTypes(MythRender* Render, MythInteropGPU::InteropMap& Types);
    static std::vector<MythVideoTextureOpenGL*>
    Retrieve(MythRenderOpenGL *Context,
             MythVideoColourSpace *ColourSpace,
             MythVideoFrame *Frame,
             FrameScanType Scan);

    ~MythOpenGLInterop() override;

    virtual std::vector<MythVideoTextureOpenGL*>
    Acquire(MythRenderOpenGL *Context,
            MythVideoColourSpace *ColourSpace,
            MythVideoFrame *Frame, FrameScanType Scan);

  protected:
    MythOpenGLInterop(MythRenderOpenGL *Context, InteropType Type, MythPlayerUI* Player = nullptr);
    virtual void DeleteTextures ();

  protected:
    MythRenderOpenGL* m_openglContext { nullptr };
    QHash<unsigned long long, std::vector<MythVideoTextureOpenGL*> > m_openglTextures;
};

#endif
