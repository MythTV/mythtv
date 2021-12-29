#ifndef MYTHDRMPRIMEINTEROP_H
#define MYTHDRMPRIMEINTEROP_H

// MythTV
#include "mythegldmabuf.h"
#include "mythopenglinterop.h"

#ifdef USING_DRM_VIDEO
#include "drm/mythvideodrm.h"
#endif

struct AVDRMFrameDescriptor;

class MythDRMPRIMEInterop : public MythOpenGLInterop, public MythEGLDMABUF
{
  public:
    static void GetDRMTypes(MythRenderOpenGL* Render, MythInteropGPU::InteropMap& Types);
    static MythDRMPRIMEInterop* CreateDRM(MythRenderOpenGL* Context, MythPlayerUI* Player);
    void DeleteTextures(void) override;
    std::vector<MythVideoTextureOpenGL*>
    Acquire(MythRenderOpenGL *Context,
            MythVideoColourSpace *ColourSpace,
            MythVideoFrame *Frame, FrameScanType Scan) override;

  protected:
    MythDRMPRIMEInterop(MythRenderOpenGL* Context, MythPlayerUI* Player, InteropType Type);
   ~MythDRMPRIMEInterop() override;

  private:
    AVDRMFrameDescriptor* VerifyBuffer(MythRenderOpenGL *Context, MythVideoFrame *Frame);
    bool m_deinterlacing { false };
    bool m_composable    { true  };

#ifdef USING_DRM_VIDEO
    bool HandleDRMVideo(MythVideoColourSpace* ColourSpace, MythVideoFrame* Frame,
                        AVDRMFrameDescriptor* DRMDesc);
    MythVideoDRM* m_drm { nullptr };
    bool m_drmTriedAndFailed { false };
#endif
};

#endif
