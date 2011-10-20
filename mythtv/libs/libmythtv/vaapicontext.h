#ifndef VAAPICONTEXT_H
#define VAAPICONTEXT_H

extern "C" {
#include "libavcodec/vaapi.h"
}
#include "va/va_glx.h"
#include "videocolourspace.h"

struct vaapi_surface
{
    VASurfaceID m_id;
};

class VAAPIDisplay;
class OpenGLVideo;

class VAAPIContext
{
  public:
    static bool IsFormatAccelerated(QSize size, MythCodecID codec,
                                    PixelFormat &pix_fmt);
    VAAPIContext(MythCodecID codec);
   ~VAAPIContext();

    bool  CreateDisplay(QSize size);
    bool  CreateBuffers(void);
    void* GetVideoSurface(int i);
    uint8_t* GetSurfaceIDPointer(void* buf);
    
    int   GetNumBuffers(void)        { return m_numSurfaces; }
    PixelFormat GetPixelFormat(void) { return m_pix_fmt;     }

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
};

#endif // VAAPICONTEXT_H
