#ifndef MYTHOPENGLINTEROP_H
#define MYTHOPENGLINTEROP_H

// Qt
#include <QHash>

// MythTV
#include "vaapicontext.h"
#include "videoouttypes.h"
#include "mythframe.h"

class MythRenderOpenGL;
class VideoColourSpace;

class MythOpenGLInterop
{
  public:
    static bool IsCodecSupported(MythCodecID CodecId);

    MythOpenGLInterop();
   ~MythOpenGLInterop();
    bool  CopySurfaceToTexture(MythRenderOpenGL *Context,
                               VideoColourSpace *ColourSpace,
                               VideoFrame *Frame, uint Texture,
                               uint TextureType, FrameScanType Scan);
    int   SetPictureAttribute(PictureAttribute Attribute, int NewValue);

  private:
    void  InitPictureAttributes(void);
    void* GetGLXSurface(uint Texture, uint TextureType, VideoFrame *Frame,
                        MythVAAPIDisplay *Display, VideoColourSpace *ColourSpace);
    void  DeleteGLXSurface(void);

    void               *m_glxSurface;
    GLuint              m_glxSurfaceTexture;
    GLenum              m_glxSurfaceTextureType;
    QSize               m_glxSurfaceSize;
    MythVAAPIDisplay   *m_glxSurfaceDisplay;
    VideoColourSpace   *m_colourSpace;
    VADisplayAttribute* m_pictureAttributes;
    int                 m_pictureAttributeCount;
    int                 m_hueBase;
    uint                m_vaapiColourSpace;
};

#endif // MYTHOPENGLINTEROP_H
