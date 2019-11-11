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
    virtual ~MythDRMPRIMEInterop() override;

    void DeleteTextures(void) override;
    vector<MythVideoTexture*> Acquire(MythRenderOpenGL *Context,
                                      VideoColourSpace *ColourSpace,
                                      VideoFrame *Frame, FrameScanType Scan) override;

  private:
    vector<MythVideoTexture*> AcquireYUV(AVDRMFrameDescriptor* Desc,
                                         MythRenderOpenGL *Context,
                                         VideoColourSpace *ColourSpace,
                                         VideoFrame *Frame, FrameScanType Scan);
    AVDRMFrameDescriptor*     VerifyBuffer(MythRenderOpenGL *Context, VideoFrame *Frame);
    bool m_openGLES       { true  };
    bool m_fullYUVSupport { false };
};

#endif // MYTHDRMPRIMEINTEROP_H
