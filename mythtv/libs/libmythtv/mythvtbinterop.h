#ifndef MYTHVTBINTEROP_H
#define MYTHVTBINTEROP_H

// MythTV
#include "mythopenglinterop.h"

// OSX
#include <CoreVideo/CoreVideo.h>

class MythVTBInterop : public MythOpenGLInterop
{
  public:
    static MythVTBInterop* Create(MythRenderOpenGL *Context, MythOpenGLInterop::Type Type);
    static Type GetInteropType(MythCodecID CodecId, MythRenderOpenGL *Context = nullptr);

    MythVTBInterop(MythRenderOpenGL *Context, MythOpenGLInterop::Type Type);
   ~MythVTBInterop() override;

    vector<MythVideoTexture*> Acquire(MythRenderOpenGL *Context,
                                      VideoColourSpace *ColourSpace,
                                      VideoFrame       *Frame,
                                      FrameScanType    Scan) override;
};

class MythVTBSurfaceInterop : public MythVTBInterop
{
  public:
    MythVTBSurfaceInterop(MythRenderOpenGL *Context);
   ~MythVTBSurfaceInterop() override;

    vector<MythVideoTexture*> Acquire(MythRenderOpenGL *Context,
                                      VideoColourSpace *ColourSpace,
                                      VideoFrame       *Frame,
                                      FrameScanType     Scan) override;

  private:

    void                      RotateReferenceFrames(IOSurfaceID Buffer);
    vector<MythVideoTexture*> GetReferenceFrames(void);
    QVector<IOSurfaceID>      m_referenceFrames { };
};

#endif
