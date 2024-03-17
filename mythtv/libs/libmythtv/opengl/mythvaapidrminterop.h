#ifndef MYTHVAAPIDRMINTEROP_H
#define MYTHVAAPIDRMINTEROP_H

// MythTV
#include "opengl/mythegldmabuf.h"
#include "opengl/mythvaapiinterop.h"

#ifdef USING_DRM_VIDEO
#include "drm/mythvideodrm.h"
#endif

class MythDRMPRIMEInterop;
struct AVDRMFrameDescriptor;

class MythVAAPIInteropDRM : public MythVAAPIInterop, public MythEGLDMABUF
{
  public:
    MythVAAPIInteropDRM(MythPlayerUI* Player, MythRenderOpenGL* Context, InteropType Type);
   ~MythVAAPIInteropDRM() override;
    std::vector<MythVideoTextureOpenGL*>
    Acquire(MythRenderOpenGL* Context,
            MythVideoColourSpace* ColourSpace,
            MythVideoFrame* Frame, FrameScanType Scan) override;
    static bool    IsSupported(MythRenderOpenGL* Context);
    void           DeleteTextures() override;

  protected:
    void           DestroyDeinterlacer() override;
    void           PostInitDeinterlacer() override;

  private:
    static VideoFrameType VATypeToMythType(uint32_t Fourcc);
    void           CleanupReferenceFrames();
    void           RotateReferenceFrames(AVBufferRef* Buffer);
    std::vector<MythVideoTextureOpenGL*> GetReferenceFrames();

  private:
    QFile                 m_drmFile;
    QVector<AVBufferRef*> m_referenceFrames;

    std::vector<MythVideoTextureOpenGL*>
    AcquireVAAPI(VASurfaceID Id, MythRenderOpenGL* Context,
                 MythVideoFrame* Frame);
    std::vector<MythVideoTextureOpenGL*>
    AcquirePrime(VASurfaceID Id, MythRenderOpenGL* Context,
                 MythVideoFrame* Frame);

    AVDRMFrameDescriptor*     GetDRMFrameDescriptor(VASurfaceID Id);
    void                      CleanupDRMPRIME();
    bool                      TestPrimeInterop();
    bool                      m_usePrime { false };
    QHash<unsigned long long, AVDRMFrameDescriptor*> m_drmFrames;

#ifdef USING_DRM_VIDEO
    bool HandleDRMVideo(MythVideoColourSpace* ColourSpace, VASurfaceID Id, MythVideoFrame* Frame);
    MythVideoDRM* m_drm { nullptr };
    bool          m_drmTriedAndFailed { false };
#endif
};

#endif
