#ifndef MYTHMMALINTEROP_H
#define MYTHMMALINTEROP_H

// MythTV
#include "mythopenglinterop.h"

// MMAL
extern "C" {
#include <interface/mmal/mmal.h>
#include <interface/mmal/mmal_buffer.h>
}

class MythMMALInterop : public MythOpenGLInterop
{
  public:
    static MythMMALInterop* Create(MythRenderOpenGL *Context, Type InteropType);
    static Type GetInteropType(MythCodecID CodecId, MythRenderOpenGL *Context = nullptr);

    MythMMALInterop(MythRenderOpenGL *Context);
    virtual ~MythMMALInterop() override;

    virtual vector<MythVideoTexture*> Acquire(MythRenderOpenGL *Context,
                                              VideoColourSpace *ColourSpace,
                                              VideoFrame *Frame, FrameScanType Scan) override;

  private:
    MMAL_BUFFER_HEADER_T* VerifyBuffer(MythRenderOpenGL *Context, VideoFrame *Frame);
};

#endif // MYTHMMALINTEROP_H
