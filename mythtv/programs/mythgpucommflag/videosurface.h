#ifndef _VIDEOSURFACE_H
#define _VIDEOSURFACE_H

#include <QHash>
#include <QList>

#include "openclinterface.h"

#include <dlfcn.h>
#include "openglsupport.h"

#include <CL/opencl.h>

extern "C" {
#include "libavcodec/vdpau.h" 
#include "libavcodec/avcodec.h"
}

typedef enum {
    kSurfaceUninit,
    kSurfaceVDPAURender,
    kSurfaceFFMpegRender,
    kSurfaceWavelet,
    kSurfaceYUV,
    kSurfaceYUV2,
    kSurfaceRGB,
    kSurfaceLogoROI,
    kSurfaceLogoRGB
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
    void Dump(QString basename, int framenum);

    VideoSurfaceType    m_type;
    uint                m_id;
    OpenCLDevice       *m_dev;
    uint32_t            m_width;
    uint32_t            m_height;
    uint32_t            m_realWidth;
    uint32_t            m_realHeight;
    PixelFormat         m_pixFmt;

    // VDPAU Decoding
    VdpVideoSurface     m_vdpSurface;
    vdpau_render_state  m_render;
    GLvdpauSurfaceNV    m_glSurface;
    GLuint              m_glVDPAUTex[4];
    GLuint              m_glOpenCLTex[4];
    GLuint              m_glFBO[4];

    // Software Decoding
    unsigned char      *m_buf;
    int                 m_offsets[3];
    int                 m_pitches[3];

    // Common
    cl_mem             *m_clBuffer;
    int                 m_bufCount;
    bool                m_valid;
};

typedef QHash<uint, VideoSurface *> VideoSurfaceHash;
typedef QList<uint> VideoSurfaceList;


#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
