#ifndef MYTHDRMPRIMEINTEROP_H
#define MYTHDRMPRIMEINTEROP_H

// MythTV
#include "mythopenglinterop.h"

struct AVDRMFrameDescriptor;

class MythDRMPRIMEInterop : public MythOpenGLInterop
{
  public:
    static MythDRMPRIMEInterop* Create(MythRenderOpenGL *Context, Type InteropType);
    static Type GetInteropType(MythCodecID CodecId, MythRenderOpenGL *Context = nullptr);

    MythDRMPRIMEInterop(MythRenderOpenGL *Context);
    virtual ~MythDRMPRIMEInterop() override = default;

    virtual vector<MythVideoTexture*> Acquire(MythRenderOpenGL *Context,
                                              VideoColourSpace *ColourSpace,
                                              VideoFrame *Frame, FrameScanType Scan) override;

  private:
    AVDRMFrameDescriptor* VerifyBuffer(MythRenderOpenGL *Context, VideoFrame *Frame);
};

#endif // MYTHDRMPRIMEINTEROP_H
