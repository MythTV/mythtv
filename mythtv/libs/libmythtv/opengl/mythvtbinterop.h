#ifndef MYTHVTBINTEROP_H
#define MYTHVTBINTEROP_H

// MythTV
#include "opengl/mythopenglinterop.h"

// OSX
#include <CoreVideo/CoreVideo.h>

class MythVTBInterop : public MythOpenGLInterop
{
    friend class MythOpenGLInterop;

  public:
    static MythVTBInterop* Create(MythRenderOpenGL* Context, MythOpenGLInterop::Type Type);
    vector<MythVideoTextureOpenGL*> Acquire(MythRenderOpenGL*     Context,
                                            MythVideoColourSpace* ColourSpace,
                                            MythVideoFrame*       Frame,
                                            FrameScanType         Scan) override;

  protected:
    CVPixelBufferRef Verify(MythRenderOpenGL* Context, MythVideoColourSpace* ColourSpace,
                            MythVideoFrame* Frame);
    static Type GetInteropType(VideoFrameType Format);
    MythVTBInterop(MythRenderOpenGL *Context, MythOpenGLInterop::Type Type);
   ~MythVTBInterop() override;
};

class MythVTBSurfaceInterop : public MythVTBInterop
{
  public:
    explicit MythVTBSurfaceInterop(MythRenderOpenGL *Context);
   ~MythVTBSurfaceInterop() override;

    vector<MythVideoTextureOpenGL*> Acquire(MythRenderOpenGL*     Context,
                                            MythVideoColourSpace* ColourSpace,
                                            MythVideoFrame*       Frame,
                                            FrameScanType         Scan) override;

  private:

    void RotateReferenceFrames(IOSurfaceID Buffer);
    vector<MythVideoTextureOpenGL*> GetReferenceFrames(void);
    QVector<IOSurfaceID> m_referenceFrames { };
};

#endif
