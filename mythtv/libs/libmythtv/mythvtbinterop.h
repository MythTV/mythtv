#ifndef MYTHVTBINTEROP_H
#define MYTHVTBINTEROP_H

// MythTV
#include "mythopenglinterop.h"

// OSX
#include <CoreVideo/CoreVideo.h>

class MythVTBInterop : public MythOpenGLInterop
{
  public:
    static MythVTBInterop* Create(MythRenderOpenGL *Context);
    static Type GetInteropType(MythCodecID CodecId, MythRenderOpenGL *Context = nullptr);

    MythVTBInterop(MythRenderOpenGL *Context);
   ~MythVTBInterop() override;

    vector<MythGLTexture*> Acquire(MythRenderOpenGL *Context,
                                   VideoColourSpace *ColourSpace,
                                   VideoFrame       *Frame,
                                   FrameScanType    Scan) override;
};


#endif
