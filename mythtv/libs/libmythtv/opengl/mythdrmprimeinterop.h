#ifndef MYTHDRMPRIMEINTEROP_H
#define MYTHDRMPRIMEINTEROP_H

// MythTV
#include "mythegldmabuf.h"
#include "mythopenglinterop.h"

struct AVDRMFrameDescriptor;

class MythDRMPRIMEInterop : public MythOpenGLInterop, public MythEGLDMABUF
{
  public:
    static MythDRMPRIMEInterop* Create(MythRenderOpenGL *Context, Type InteropType);
    static Type GetInteropType(MythCodecID CodecId, MythRenderOpenGL *Context = nullptr);

    MythDRMPRIMEInterop(MythRenderOpenGL *Context);
    virtual ~MythDRMPRIMEInterop() override;

    void DeleteTextures(void) override;
    vector<MythVideoTexture*> Acquire(MythRenderOpenGL *Context,
                                      VideoColourSpace *ColourSpace,
                                      VideoFrame *Frame, FrameScanType Scan) override;

  private:
    AVDRMFrameDescriptor*     VerifyBuffer(MythRenderOpenGL *Context, VideoFrame *Frame);
};

#endif // MYTHDRMPRIMEINTEROP_H
