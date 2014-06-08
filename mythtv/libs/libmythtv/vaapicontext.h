#ifndef VAAPICONTEXT_H
#define VAAPICONTEXT_H

extern "C" {
#include "libavcodec/vaapi.h"
}
#include "va/va_version.h"
#if VA_CHECK_VERSION(0,34,0)
#include "va/va_compat.h"
#endif
#include "va/va_x11.h"
#include "va/va_glx.h"
#include "videocolourspace.h"

struct vaapi_surface
{
    VASurfaceID m_id;
};

class VAAPIDisplay;
class OpenGLVideo;
class MythRenderOpenGL;

enum VAAPIDisplayType
{
    kVADisplayX11,
    kVADisplayGLX,
};

class VAAPIContext
{
  public:
    static bool IsFormatAccelerated(QSize size, MythCodecID codec,
                                    PixelFormat &pix_fmt);
    VAAPIContext(VAAPIDisplayType display_type, MythCodecID codec);
   ~VAAPIContext();

    bool  CreateDisplay(QSize size, bool noreuse = false, 
                        MythRenderOpenGL *render = NULL);
    bool  CreateBuffers(void);
    void* GetVideoSurface(int i);
    uint8_t* GetSurfaceIDPointer(void* buf);

    int   GetNumBuffers(void)        const { return m_numSurfaces; }
    PixelFormat GetPixelFormat(void) const { return m_pix_fmt;     }

    // X11 display
    bool  CopySurfaceToFrame(VideoFrame *frame, const void *buf);
    bool  InitImage(const void *buf);
    // GLX display
    bool  CopySurfaceToTexture(const void* buf, uint texture,
                               uint texture_type, FrameScanType scan);
    void* GetGLXSurface(uint texture, uint texture_type);
    void  ClearGLXSurfaces(void);

    bool InitDisplay(void);
    bool InitProfiles(void);
    bool InitBuffers(void);
    bool InitContext(void);
    void InitPictureAttributes(VideoColourSpace &colourspace);
    int  SetPictureAttribute(PictureAttribute attribute, int newValue);

    VAAPIDisplayType m_dispType;
    vaapi_context  m_ctx;
    MythCodecID    m_codec;
    QSize          m_size;
    VAAPIDisplay  *m_display;
    VAProfile      m_vaProfile;
    VAEntrypoint   m_vaEntrypoint;
    PixelFormat    m_pix_fmt;
    int            m_numSurfaces;
    VASurfaceID   *m_surfaces;
    vaapi_surface *m_surfaceData;
    QHash<uint, void*> m_glxSurfaces;
    VADisplayAttribute* m_pictureAttributes;
    int            m_pictureAttributeCount;
    int            m_hueBase;
    VAImage        m_image;
};

#endif // VAAPICONTEXT_H
