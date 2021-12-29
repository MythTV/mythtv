#ifndef MYTHMMALINTEROP_H
#define MYTHMMALINTEROP_H

// MythTV
#include "opengl/mythopenglinterop.h"

// MMAL
extern "C" {
#include <interface/mmal/mmal.h>
#include <interface/mmal/mmal_buffer.h>
}

class MythMMALInterop : public MythOpenGLInterop
{
  public:
    static void GetMMALTypes(MythRenderOpenGL* Render, MythInteropGPU::InteropMap& Types);
    static MythMMALInterop* CreateMMAL(MythRenderOpenGL* Context);
    virtual std::vector<MythVideoTextureOpenGL*>
    Acquire(MythRenderOpenGL *Context,
            MythVideoColourSpace *ColourSpace,
            MythVideoFrame *Frame, FrameScanType Scan) override;

  protected:
    MythMMALInterop(MythRenderOpenGL *Context);
    virtual ~MythMMALInterop() override;

  private:
    MMAL_BUFFER_HEADER_T* VerifyBuffer(MythRenderOpenGL *Context, MythVideoFrame *Frame);
};

#endif // MYTHMMALINTEROP_H
