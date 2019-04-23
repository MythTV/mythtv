#ifndef MYTHNVDECINTEROP_H
#define MYTHNVDECINTEROP_H

// MythTV
#include "mythopenglinterop.h"

// FFmpeg
extern "C" {
#include "compat/cuda/dynlink_loader.h"
#include "libavutil/hwcontext_cuda.h"
}

class MythNVDECInterop : public MythOpenGLInterop
{
  public:
    static MythNVDECInterop* Create(MythRenderOpenGL *Context);
    static Type GetInteropType(MythCodecID CodecId, MythRenderOpenGL *Context = nullptr);

   ~MythNVDECInterop() override;

    bool IsValid(void);
    CUcontext GetCUDAContext(void);
    vector<MythVideoTexture*> Acquire(MythRenderOpenGL *Context, VideoColourSpace *ColourSpace,
                                      VideoFrame *Frame, FrameScanType Scan) override;

  protected:
    MythNVDECInterop(MythRenderOpenGL *Context);

  private:
    bool           InitialiseCuda(void);

    CUcontext      m_cudaContext;
    CudaFunctions *m_cudaFuncs;
};

#endif // MYTHNVDECINTEROP_H
