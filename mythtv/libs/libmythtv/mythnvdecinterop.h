#ifndef MYTHNVDECINTEROP_H
#define MYTHNVDECINTEROP_H

// MythTV
#include "mythopenglinterop.h"

class MythNVDECInterop : public MythOpenGLInterop
{
  public:
    static MythNVDECInterop* Create(MythRenderOpenGL *Context);
    static Type GetInteropType(MythCodecID CodecId, MythRenderOpenGL *Context = nullptr);

    MythNVDECInterop(MythRenderOpenGL *Context);
   ~MythNVDECInterop() override;

    vector<MythVideoTexture*> Acquire(MythRenderOpenGL *Context, VideoColourSpace *ColourSpace,
                                      VideoFrame *Frame, FrameScanType Scan) override;
};

#endif // MYTHNVDECINTEROP_H
