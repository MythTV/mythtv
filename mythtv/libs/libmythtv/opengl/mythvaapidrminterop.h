#ifndef MYTHVAAPIDRMINTEROP_H
#define MYTHVAAPIDRMINTEROP_H

// MythTV
#include "opengl/mythegldmabuf.h"
#include "opengl/mythvaapiinterop.h"

class MythDRMPRIMEInterop;
struct AVDRMFrameDescriptor;

class MythVAAPIInteropDRM : public MythVAAPIInterop, public MythEGLDMABUF
{
  public:
    explicit MythVAAPIInteropDRM(MythRenderOpenGL* Context);
    ~MythVAAPIInteropDRM() override;
    vector<MythVideoTextureOpenGL*> Acquire(MythRenderOpenGL* Context,
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
    vector<MythVideoTextureOpenGL*> GetReferenceFrames();

  private:
    QFile                 m_drmFile         { };
    QVector<AVBufferRef*> m_referenceFrames { };

    vector<MythVideoTextureOpenGL*> AcquireVAAPI(VASurfaceID Id, MythRenderOpenGL* Context,
                                                 MythVideoFrame* Frame);
    vector<MythVideoTextureOpenGL*> AcquirePrime(VASurfaceID Id, MythRenderOpenGL* Context,
                                                 MythVideoFrame* Frame);
    void                      CleanupDRMPRIME();
    bool                      TestPrimeInterop();
    bool                      m_usePrime { false };
    QHash<unsigned long long, AVDRMFrameDescriptor*> m_drmFrames { };
};

#endif
