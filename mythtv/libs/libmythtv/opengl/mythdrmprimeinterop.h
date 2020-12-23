#ifndef MYTHDRMPRIMEINTEROP_H
#define MYTHDRMPRIMEINTEROP_H

// MythTV
#include "mythegldmabuf.h"
#include "mythopenglinterop.h"

struct AVDRMFrameDescriptor;

class MythDRMPRIMEInterop : public MythOpenGLInterop, public MythEGLDMABUF
{
  public:
    static void GetDRMTypes(MythRenderOpenGL* Render, MythInteropGPU::InteropMap& Types);
    static MythDRMPRIMEInterop* CreateDRM(MythRenderOpenGL* Context, MythPlayerUI* Player = nullptr);
    void DeleteTextures(void) override;
    vector<MythVideoTextureOpenGL*> Acquire(MythRenderOpenGL *Context,
                                            MythVideoColourSpace *ColourSpace,
                                            MythVideoFrame *Frame, FrameScanType Scan) override;

  protected:
    MythDRMPRIMEInterop(MythRenderOpenGL* Context, MythPlayerUI* Player);
   ~MythDRMPRIMEInterop() override;

  private:
    AVDRMFrameDescriptor* VerifyBuffer(MythRenderOpenGL *Context, MythVideoFrame *Frame);
    bool m_deinterlacing { false };
    bool m_composable    { true  };
};

#endif
