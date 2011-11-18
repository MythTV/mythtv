#ifndef _VIDEOSURFACE_H
#define _VIDEOSURFACE_H

#include "openclinterface.h"

#include <dlfcn.h>
#include "openglsupport.h"

#ifdef MAX
#undef MAX
#endif
#ifdef MIN
#undef MIN
#endif
#include <oclUtils.h>

extern "C" {
#include "libavcodec/vdpau.h" 
#include "libavcodec/avcodec.h"
}

typedef enum {
    kSurfaceUninit,
    kSurfaceVDPAURender,
    kSurfaceWavelet,
    kSurfaceYUV,
    kSurfaceRGB
} VideoSurfaceType;

class VideoSurface
{
  public:
    VideoSurface(OpenCLDevice *dev, uint32_t width, uint32_t height, uint id,
                 VdpVideoSurface vdpSurface);
    VideoSurface(OpenCLDevice *dev, VideoSurfaceType type, uint32_t width,
                 uint32_t height);
    ~VideoSurface();
    void Bind(void);
    void Dump(QString basename);

    VideoSurfaceType    m_type;
    uint                m_id;
    OpenCLDevice       *m_dev;
    uint32_t            m_width;
    uint32_t            m_height;
    PixelFormat         m_pixFmt;
    VdpVideoSurface     m_vdpSurface;
    vdpau_render_state  m_render;
    GLvdpauSurfaceNV    m_glSurface;
    GLuint              m_glVDPAUTex[4];
    GLuint              m_glOpenCLTex[4];
    GLuint              m_glFBO[4];
    cl_mem              m_clBuffer[4];
    bool                m_valid;
};

#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
