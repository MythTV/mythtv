#ifndef _FRAME_H
#define _FRAME_H

#ifdef __cplusplus
#include <cstdint>
#include <cstring>
#else
#include <stdint.h>
#include <string.h>
#endif
#include "mythtvexp.h" // for MTV_PUBLIC

#ifdef __cplusplus
extern "C" {
#endif

typedef enum FrameType_
{
    FMT_NONE = -1,
    FMT_RGB24 = 0,
    FMT_YV12,
    FMT_RGB32,   ///< endian dependent format, ARGB or BGRA
    FMT_ARGB32,
    FMT_RGBA32,
    FMT_YUV422P,
    FMT_BGRA,
    FMT_YUY2,
    FMT_NV12,
    FMT_YUYVHQ, // temporary
    // these are all endian dependent and higher bit depth
    FMT_YUV420P10,
    FMT_YUV420P12,
    FMT_YUV420P16,
    // hardware formats
    FMT_VDPAU,
    FMT_VAAPI,
    FMT_DXVA2,
    FMT_OMXEGL,
    FMT_MEDIACODEC,
    FMT_VTB,
    FMT_NVDEC
} VideoFrameType;

const char* format_description(VideoFrameType Type);

static inline int format_is_hw(VideoFrameType Type)
{
    return (Type == FMT_VDPAU) || (Type == FMT_VAAPI) ||
           (Type == FMT_DXVA2) || (Type == FMT_OMXEGL) ||
           (Type == FMT_MEDIACODEC) || (Type == FMT_VTB) ||
           (Type == FMT_NVDEC);
}

static inline int format_is_yuv(VideoFrameType Type)
{
    return (Type == FMT_YV12) || (Type == FMT_YUV422P) ||
           (Type == FMT_YUY2) || (Type == FMT_NV12) ||
           (Type == FMT_YUYVHQ) || (Type == FMT_YUV420P10) ||
           (Type == FMT_YUV420P12) || (Type == FMT_YUV420P16);
}

typedef struct VideoFrame_
{
    VideoFrameType codec;
    unsigned char *buf;

    int width;
    int height;
    float aspect;
    double frame_rate;
    int bpp;
    int size;

    long long frameNumber;
    long long timecode;
    int64_t   disp_timecode;

    unsigned char *priv[4]; ///< random empty storage

    unsigned char *qscale_table;
    int            qstride;

    int interlaced_frame; ///< 1 if interlaced.
    int top_field_first; ///< 1 if top field is first.
    int repeat_pict;
    int forcekey; ///< hardware encoded .nuv
    int dummy;

    int pitches[3]; ///< Y, U, & V pitches
    int offsets[3]; ///< Y, U, & V offsets

    int pix_fmt;
    int directrendering; ///< 1 if managed by FFmpeg
    int colorspace;
} VideoFrame;

#ifdef __cplusplus
}
#endif

int MTV_PUBLIC ColorDepth(int Format);

#ifdef __cplusplus

class MTV_PUBLIC MythUSWCCopy
{
public:
    MythUSWCCopy(int width, bool nocache = false);
    virtual ~MythUSWCCopy();

    void copy(VideoFrame *dst, const VideoFrame *src);
    void reset(int width);
    void resetUSWCDetection(void);
    void setUSWC(bool uswc);

private:
    void allocateCache(int width);

    uint8_t* m_cache;
    int m_size;
    int m_uswc;
};

void MTV_PUBLIC framecopy(VideoFrame *dst, const VideoFrame *src,
                          bool useSSE = true);

static inline void init(VideoFrame *vf, VideoFrameType _codec,
                        unsigned char *_buf, int _width, int _height, int _size,
                        const int *p = nullptr,
                        const int *o = nullptr,
                        float _aspect = -1.0f, double _rate = -1.0f,
                        int _aligned = 64);
static inline void clear(VideoFrame *vf);
static inline bool compatible(const VideoFrame *a,
                              const VideoFrame *b);
static inline int  bitsperpixel(VideoFrameType type);

static inline void init(VideoFrame *vf, VideoFrameType _codec,
                        unsigned char *_buf, int _width, int _height,
                        int _size, const int *p, const int *o,
                        float _aspect, double _rate, int _aligned)
{
    vf->bpp    = bitsperpixel(_codec);
    vf->codec  = _codec;
    vf->buf    = _buf;
    vf->width  = _width;
    vf->height = _height;
    vf->aspect = _aspect;
    vf->frame_rate = _rate;

    vf->size         = _size;
    vf->frameNumber  = 0;
    vf->timecode     = 0;

    vf->qscale_table = nullptr;
    vf->qstride      = 0;

    vf->interlaced_frame = 1;
    vf->top_field_first  = 1;
    vf->repeat_pict      = 0;
    vf->forcekey         = 0;
    vf->dummy            = 0;
    vf->pix_fmt          = 0;
    vf->directrendering  = 1;
    vf->colorspace       = 1; // BT.709

    memset(vf->priv, 0, 4 * sizeof(unsigned char *));

    uint width_aligned;
    if (!_aligned)
    {
        width_aligned = _width;
    }
    else
    {
        width_aligned = (_width + _aligned - 1) & ~(_aligned - 1);
    }

    if (p)
    {
        memcpy(vf->pitches, p, 3 * sizeof(int));
    }
    else
    {
        if (FMT_YV12 == _codec || FMT_YUV422P == _codec)
        {
            vf->pitches[0] = width_aligned;
            vf->pitches[1] = vf->pitches[2] = (width_aligned+1) >> 1;
        }
        else if (FMT_NV12 == _codec)
        {
            vf->pitches[0] = width_aligned;
            vf->pitches[1] = width_aligned;
            vf->pitches[2] = 0;
        }
        else
        {
            vf->pitches[0] = (width_aligned * vf->bpp) >> 3;
            vf->pitches[1] = vf->pitches[2] = 0;
        }
    }

    if (o)
    {
        memcpy(vf->offsets, o, 3 * sizeof(int));
    }
    else
    {
        if (FMT_YV12 == _codec)
        {
            vf->offsets[0] = 0;
            vf->offsets[1] = width_aligned * _height;
            vf->offsets[2] =
                vf->offsets[1] + ((width_aligned+1) >> 1) * ((_height+1) >> 1);
        }
        else if (FMT_YUV422P == _codec)
        {
            vf->offsets[0] = 0;
            vf->offsets[1] = width_aligned * _height;
            vf->offsets[2] =
                vf->offsets[1] + ((width_aligned+1) >> 1) * _height;
        }
        else if (FMT_NV12 == _codec)
        {
            vf->offsets[0] = 0;
            vf->offsets[1] = width_aligned * _height;
            vf->offsets[2] = 0;
        }
        else
        {
            vf->offsets[0] = vf->offsets[1] = vf->offsets[2] = 0;
        }
    }
}

static inline void clear(VideoFrame *vf)
{
    if (!vf)
        return;

    int uv_height = (vf->height+1) >> 1;
    if (FMT_YV12 == vf->codec)
    {
        memset(vf->buf + vf->offsets[0],   0, vf->pitches[0] * vf->height);
        memset(vf->buf + vf->offsets[1], 127, vf->pitches[1] * uv_height);
        memset(vf->buf + vf->offsets[2], 127, vf->pitches[2] * uv_height);
    }
    else if (FMT_NV12 == vf->codec)
    {
        memset(vf->buf + vf->offsets[0],   0, vf->pitches[0] * vf->height);
        memset(vf->buf + vf->offsets[1], 127, vf->pitches[1] * uv_height);
    }
}

static inline bool compatible(const VideoFrame *a, const VideoFrame *b)
{
    if (a && b && a->codec == b->codec &&
        (a->codec == FMT_YV12 || a->codec == FMT_NV12))
    {
        return (a->codec      == b->codec)      &&
               (a->width      == b->width)      &&
               (a->height     == b->height)     &&
               (a->size       == b->size);
    }

    return a && b &&
        (a->codec      == b->codec)      &&
        (a->width      == b->width)      &&
        (a->height     == b->height)     &&
        (a->size       == b->size)       &&
        (a->offsets[0] == b->offsets[0]) &&
        (a->offsets[1] == b->offsets[1]) &&
        (a->offsets[2] == b->offsets[2]) &&
        (a->pitches[0] == b->pitches[0]) &&
        (a->pitches[1] == b->pitches[1]) &&
        (a->pitches[2] == b->pitches[2]);
}

/**
 * copy: copy one frame into another
 * copy only works with the following assumptions:
 * frames are of the same resolution
 * destination frame is in YV12 format
 * source frame is either YV12 or NV12 format
 */
static inline void copy(VideoFrame *dst, const VideoFrame *src)
{
    framecopy(dst, src, true);
}

static inline uint planes(VideoFrameType Type)
{
    switch (Type)
    {
        case FMT_YV12:
        case FMT_YUV422P:
        case FMT_YUV420P10:
        case FMT_YUV420P12:
        case FMT_YUV420P16: return 3;
        case FMT_NV12:      return 2;
        case FMT_YUY2:
        case FMT_YUYVHQ:
        case FMT_BGRA:
        case FMT_ARGB32:
        case FMT_RGB24:
        case FMT_RGB32:
        case FMT_RGBA32:    return 1;
        default: break;
    }
    return 0;
}

static inline int bitsperpixel(VideoFrameType type)
{
    int res = 8;
    switch (type)
    {
        case FMT_BGRA:
        case FMT_RGBA32:
        case FMT_ARGB32:
        case FMT_RGB32:
        case FMT_YUYVHQ:
            res = 32;
            break;
        case FMT_RGB24:
            res = 24;
            break;
        case FMT_YUV422P:
        case FMT_YUY2:
            res = 16;
            break;
        case FMT_YV12:
        case FMT_NV12:
            res = 12;
            break;
        case FMT_YUV420P10: // NB stored in 16bits
        case FMT_YUV420P12: // NB stored in 16bits
        case FMT_YUV420P16:
            res = 24;
            break;
        default:
            res = 8;
    }
    return res;
}

static inline uint buffersize(VideoFrameType type, int width, int height,
                              int _aligned = 64)
{
    int  type_bpp = bitsperpixel(type);
    uint bpp = type_bpp / 4; /* bits per pixel div common factor */
    uint bpb =  8 / 4; /* bits per byte div common factor */

    // make sure all our pitches are a multiple of 16 bytes
    // as U and V channels are half the size of Y channel
    // This allow for SSE/HW accelerated code on all planes
    // If the buffer sizes are not 32 bytes aligned, adjust.
    // old versions of MythTV allowed people to set invalid
    // dimensions for MPEG-4 capture, no need to segfault..
    uint adj_w;
    if (!_aligned)
    {
        adj_w = width;
    }
    else
    {
        adj_w = (width  + _aligned - 1) & ~(_aligned - 1);
    }
    // Calculate rounding as necessary.
    uint remainder = (adj_w * height * bpp) % bpb;
    return (adj_w * height * bpp) / bpb + (remainder ? 1 : 0);
}

static inline void copybuffer(VideoFrame *dst, uint8_t *buffer,
                              int pitch, VideoFrameType type = FMT_YV12)
{
    if (type == FMT_YV12)
    {
        VideoFrame framein;
        int chroma_pitch  = pitch >> 1;
        int chroma_height = dst->height >> 1;
        int offsets[3] =
            { 0,
              pitch * dst->height,
              pitch * dst->height + chroma_pitch * chroma_height };
        int pitches[3] = { pitch, chroma_pitch, chroma_pitch };

        init(&framein, type, buffer, dst->width, dst->height, dst->size,
             pitches, offsets);
        copy(dst, &framein);
    }
    else if (type == FMT_NV12)
    {
        VideoFrame framein;
        int offsets[3] = { 0, pitch * dst->height, 0 };
        int pitches[3] = { pitch, pitch, 0 };

        init(&framein, type, buffer, dst->width, dst->height, dst->size,
             pitches, offsets);
        copy(dst, &framein);
    }
}

static inline void copybuffer(uint8_t *dstbuffer, const VideoFrame *src,
                              int pitch = 0, VideoFrameType type = FMT_YV12)
{
    if (pitch == 0)
    {
        pitch = src->width;
    }

    if (type == FMT_YV12)
    {
        VideoFrame frameout;
        int chroma_pitch  = pitch >> 1;
        int chroma_height = src->height >> 1;
        int offsets[3] =
            { 0,
              pitch * src->height,
              pitch * src->height + chroma_pitch * chroma_height };
        int pitches[3] = { pitch, chroma_pitch, chroma_pitch };

        init(&frameout, type, dstbuffer, src->width, src->height, src->size,
             pitches, offsets);
        copy(&frameout, src);
    }
    else if (type == FMT_NV12)
    {
        VideoFrame frameout;
        int offsets[3] = { 0, pitch * src->height, 0 };
        int pitches[3] = { pitch, pitch, 0 };

        init(&frameout, type, dstbuffer, src->width, src->height, src->size,
             pitches, offsets);
        copy(&frameout, src);
    }
}
#endif /* __cplusplus */

#endif
