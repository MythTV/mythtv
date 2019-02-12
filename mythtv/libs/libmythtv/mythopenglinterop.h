#ifndef MYTHOPENGLINTEROP_H
#define MYTHOPENGLINTEROP_H

// MythTV
#include "videoouttypes.h"
#include "mythcodecid.h"
#include "mythframe.h"

#ifdef USING_GLVAAPI
#include "va/va.h"
class MythVAAPIDisplay;
#endif

class MythGLTexture;
class MythRenderOpenGL;
class VideoColourSpace;

class MythOpenGLInterop
{
  public:
    static QStringList GetAllowedRenderers(MythCodecID CodecId);
    static bool IsCodecSupported(MythCodecID CodecId);

    MythOpenGLInterop();
   ~MythOpenGLInterop();
    MythGLTexture* GetTexture(MythRenderOpenGL *Context,
                              VideoColourSpace *ColourSpace,
                              VideoFrame *Frame, FrameScanType Scan);
    int   SetPictureAttribute(PictureAttribute Attribute, int NewValue);

  private:
    MythGLTexture      *m_glTexture;
    int                 m_glTextureFormat;
    VideoColourSpace   *m_colourSpace;

#ifdef USING_GLVAAPI
    MythGLTexture *GetVAAPITexture(MythRenderOpenGL *Context,
                                   VideoColourSpace *ColourSpace,
                                   VideoFrame *Frame, FrameScanType Scan);
    void           InitVAAPIPictureAttributes(void);
    int            SetVAAPIPictureAttribute(PictureAttribute Attribute, int NewValue);
    MythGLTexture* GetVAAPIGLXSurface(VideoFrame *Frame, MythVAAPIDisplay *Display,
                                      VideoColourSpace *ColourSpace);
    void           DeleteVAAPIGLXSurface(void);

    MythVAAPIDisplay   *m_vaapiDisplay;
    VADisplayAttribute* m_vaapiPictureAttributes;
    int                 m_vaapiPictureAttributeCount;
    int                 m_vaapiHueBase;
    uint                m_vaapiColourSpace;
#endif
};

#endif // MYTHOPENGLINTEROP_H
