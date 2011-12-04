#ifndef _VIDEOSURFACE_H
#define _VIDEOSURFACE_H

#include <QHash>
#include <QList>

#include "referencecounter.h"
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

class VideoSurface : public ReferenceCounter
{
  public:
    VideoSurface(OpenCLDevice *dev, uint32_t width, uint32_t height, uint id,
                 VdpVideoSurface vdpSurface);
    VideoSurface(OpenCLDevice *dev, VideoSurfaceType type, uint32_t width,
                 uint32_t height);
    ~VideoSurface();
    void Bind(void);
    void Dump(QString basename, int framenum, int limit = 0);

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

class VideoHistogram : public ReferenceCounter
{
  public:
    VideoHistogram(OpenCLDevice *dev, int binCount = 64, int offset = 0);
    ~VideoHistogram(void);
    void Dump(QString basename, int framenum, bool summaryOnly = true);

    OpenCLDevice *m_dev;
    int m_binCount;
    int m_offset;
    cl_mem m_buf;
};

class VideoAspect : public ReferenceCounter
{
  public:
    VideoAspect(OpenCLDevice *dev, VideoSurface *rgb);
    VideoAspect(uint32_t *topL, uint32_t *botR);
    ~VideoAspect(void) {};
    bool Compare(const uint32_t* topL, const uint32_t *botR) const;
    bool Compare(const VideoAspect* other) const;
    uint32_t Width(void) const { return m_xBR - m_xTL + 1; };
    uint32_t Height(void) const { return (m_yBR - m_yTL + 1) * 2; };
    float Ratio(uint32_t width = 0, uint32_t height = 0) const;
    float NearestRatio(uint32_t width = 0, uint32_t height = 0) const;

    uint32_t m_xTL;
    uint32_t m_yTL;
    uint32_t m_xBR;
    uint32_t m_yBR;
};

#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
