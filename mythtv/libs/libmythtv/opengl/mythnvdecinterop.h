#ifndef MYTHNVDECINTEROP_H
#define MYTHNVDECINTEROP_H

// MythTV
#include "opengl/mythopenglinterop.h"

extern "C" {
#include <ffnvcodec/dynlink_cuda.h>
struct CudaFunctions;
}

class MythNVDECInterop : public MythOpenGLInterop
{
  public:
    static void GetNVDECTypes(MythRenderOpenGL* Render, MythInteropGPU::InteropMap& Types);
    static MythNVDECInterop* CreateNVDEC(MythPlayerUI* Player, MythRenderOpenGL* Context);
    static bool CreateCUDAContext(MythRenderOpenGL* GLContext, CudaFunctions*& CudaFuncs,
                                  CUcontext& CudaContext);
    static void CleanupContext(MythRenderOpenGL* GLContext, CudaFunctions*& CudaFuncs,
                               CUcontext& CudaContext);

    bool IsValid();
    CUcontext GetCUDAContext();

    std::vector<MythVideoTextureOpenGL*>
    Acquire(MythRenderOpenGL* Context, MythVideoColourSpace* ColourSpace,
            MythVideoFrame* Frame, FrameScanType Scan) override;

  protected:
    MythNVDECInterop(MythPlayerUI* Player, MythRenderOpenGL* Context);
   ~MythNVDECInterop() override;

  private:
    bool           InitialiseCuda();
    void           DeleteTextures() override;
    void           RotateReferenceFrames(CUdeviceptr Buffer);
    static bool    CreateCUDAPriv(MythRenderOpenGL* GLContext, CudaFunctions*& CudaFuncs,
                                  CUcontext& CudaContext, bool& Retry);
    CUcontext      m_cudaContext {};
    CudaFunctions* m_cudaFuncs { nullptr };
    QVector<CUdeviceptr> m_referenceFrames;
};

#endif
