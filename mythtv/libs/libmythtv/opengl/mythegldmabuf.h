#ifndef MYTHEGLDMABUF_H
#define MYTHEGLDMABUF_H

// MythTV
#include "mythframe.h"
#include "videoouttypes.h"

// Std
#include <vector>
using namespace std;

class MythRenderOpenGL;
class MythVideoTexture;
struct AVDRMFrameDescriptor;

class MythEGLDMABUF
{
  public:
    explicit MythEGLDMABUF(MythRenderOpenGL *Context);
   ~MythEGLDMABUF() = default;
    static bool HaveDMABuf(MythRenderOpenGL *Context);
    vector<MythVideoTexture*> CreateTextures(AVDRMFrameDescriptor* Desc,
                                             MythRenderOpenGL *Context,
                                             VideoFrame *Frame,
                                             bool UseSeparate,
                                             FrameScanType Scan = kScan_Progressive);
    static void               ClearDMATextures(MythRenderOpenGL *Context,
                                               vector<MythVideoTexture*>& Textures) ;

  private:
    vector<MythVideoTexture*> CreateComposed(AVDRMFrameDescriptor* Desc,
                                             MythRenderOpenGL *Context,
                                             VideoFrame *Frame,
                                             FrameScanType Scan) const;
    vector<MythVideoTexture*> CreateSeparate(AVDRMFrameDescriptor* Desc,
                                             MythRenderOpenGL *Context,
                                             VideoFrame *Frame) const;
    vector<MythVideoTexture*> CreateSeparate2(AVDRMFrameDescriptor* Desc,
                                             MythRenderOpenGL *Context,
                                             VideoFrame *Frame) const;
    bool m_useModifiers { false };
};

#endif // MYTHEGLDMABUF_H
