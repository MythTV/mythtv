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
    friend class MythOpenGLInterop;

  public:
    static MythNVDECInterop* Create(MythRenderOpenGL *Context);
    static bool CreateCUDAContext(MythRenderOpenGL *GLContext, CudaFunctions *&CudaFuncs,
                                  CUcontext &CudaContext);
    static void CleanupContext(MythRenderOpenGL *GLContext, CudaFunctions *&CudaFuncs,
                               CUcontext &CudaContext);

    bool IsValid(void);
    CUcontext GetCUDAContext(void);
    vector<MythVideoTexture*> Acquire(MythRenderOpenGL *Context, VideoColourSpace *ColourSpace,
                                      VideoFrame *Frame, FrameScanType Scan) override;

  protected:
    static Type GetInteropType(VideoFrameType Format);
    MythNVDECInterop(MythRenderOpenGL *Context);
   ~MythNVDECInterop() override;

  private:
    bool           InitialiseCuda(void);
    void           DeleteTextures(void) override;
    void           RotateReferenceFrames(CUdeviceptr Buffer);

    CUcontext      m_cudaContext;
    CudaFunctions *m_cudaFuncs { nullptr };
    QVector<CUdeviceptr> m_referenceFrames;
};

#endif // MYTHNVDECINTEROP_H
