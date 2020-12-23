#ifndef MYTHOPENGLINTEROP_H
#define MYTHOPENGLINTEROP_H

// Qt
#include <QObject>

// MythTV
#include "opengl/mythrenderopengl.h"
#include "referencecounter.h"
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
    static vector<MythVideoTextureOpenGL*> Retrieve(MythRenderOpenGL *Context,
                                                    MythVideoColourSpace *ColourSpace,
                                                    MythVideoFrame *Frame,
                                                    FrameScanType Scan);

    ~MythOpenGLInterop() override;
    virtual vector<MythVideoTextureOpenGL*> Acquire(MythRenderOpenGL *Context,
                                                    MythVideoColourSpace *ColourSpace,
                                                    MythVideoFrame *Frame, FrameScanType Scan);

  protected:
    MythOpenGLInterop(MythRenderOpenGL *Context, InteropType Type, MythPlayerUI* Player = nullptr);
    virtual void DeleteTextures ();

  protected:
    MythRenderOpenGL* m_openglContext { nullptr };
    QHash<unsigned long long, vector<MythVideoTextureOpenGL*> > m_openglTextures;
};

#endif
