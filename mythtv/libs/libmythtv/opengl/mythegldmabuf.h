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
    MythEGLDMABUF(MythRenderOpenGL *Context);
   ~MythEGLDMABUF() = default;
    static bool HaveDMABuf(MythRenderOpenGL *Context);
    vector<MythVideoTexture*> CreateTextures(AVDRMFrameDescriptor* Desc,
                                             MythRenderOpenGL *Context,
                                             VideoFrame *Frame,
                                             FrameScanType Scan = kScan_Progressive);

  private:
    vector<MythVideoTexture*> CreateComposed(AVDRMFrameDescriptor* Desc,
                                             MythRenderOpenGL *Context,
                                             VideoFrame *Frame,
                                             FrameScanType Scan);
    vector<MythVideoTexture*> CreateSeparate(AVDRMFrameDescriptor* Desc,
                                             MythRenderOpenGL *Context,
                                             VideoFrame *Frame);
    bool m_useModifiers { false };
};

#endif // MYTHEGLDMABUF_H
