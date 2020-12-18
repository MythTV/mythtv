#ifndef MYTHEGLDMABUF_H
#define MYTHEGLDMABUF_H

// MythTV
#include "mythframe.h"
#include "videoouttypes.h"

// Std
#include <vector>

class MythRenderOpenGL;
class MythVideoTextureOpenGL;
struct AVDRMFrameDescriptor;

class MythEGLDMABUF
{
  public:
    explicit MythEGLDMABUF(MythRenderOpenGL *Context);
   ~MythEGLDMABUF() = default;
    static bool HaveDMABuf(MythRenderOpenGL *Context);
    std::vector<MythVideoTextureOpenGL*> CreateTextures(AVDRMFrameDescriptor* Desc,
                                                        MythRenderOpenGL *Context,
                                                        MythVideoFrame *Frame,
                                                        bool UseSeparate,
                                                        FrameScanType Scan = kScan_Progressive);
    static void               ClearDMATextures(MythRenderOpenGL *Context,
                                               std::vector<MythVideoTextureOpenGL*>& Textures) ;

  private:
    std::vector<MythVideoTextureOpenGL*> CreateComposed(AVDRMFrameDescriptor* Desc,
                                                        MythRenderOpenGL *Context,
                                                        MythVideoFrame *Frame,
                                                        FrameScanType Scan) const;
    std::vector<MythVideoTextureOpenGL*> CreateSeparate(AVDRMFrameDescriptor* Desc,
                                                        MythRenderOpenGL *Context,
                                                        MythVideoFrame *Frame) const;
    std::vector<MythVideoTextureOpenGL*> CreateSeparate2(AVDRMFrameDescriptor* Desc,
                                                         MythRenderOpenGL *Context,
                                                         MythVideoFrame *Frame) const;
    bool m_useModifiers { false };
};

#endif
