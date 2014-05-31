#ifndef _FRAME_H
#define _FRAME_H

#include <string.h>
#include <stdint.h>
#include "fourcc.h"
#include "mythtvexp.h" // for MUNUSED

#ifdef __cplusplus
extern "C" {
#endif

typedef enum FrameType_
{
    FMT_NONE = -1,
    FMT_RGB24 = 0,
    FMT_YV12,
    FMT_IA44,
    FMT_AI44,
    FMT_ARGB32,
    FMT_RGBA32,
    FMT_YUV422P,
    FMT_BGRA,
    FMT_VDPAU,
    FMT_VAAPI,
    FMT_YUY2,
    FMT_DXVA2,
} VideoFrameType;

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

    unsigned char *priv[4]; // random empty storage

    unsigned char *qscale_table;
    int            qstride;

    int interlaced_frame; // 1 if interlaced.
    int top_field_first; // 1 if top field is first.
    int repeat_pict;
    int forcekey; // hardware encoded .nuv
    int dummy;

    int pitches[3]; // Y, U, & V pitches
    int offsets[3]; // Y, U, & V offsets

    int pix_fmt;
    int directrendering; // 1 if managed by FFmpeg
} VideoFrame;

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
static inline void init(VideoFrame *vf, VideoFrameType _codec,
                        unsigned char *_buf, int _width, int _height, int _size,
                        const int *p = 0,
                        const int *o = 0,
                        float _aspect = -1.0f, double _rate = -1.0f) MUNUSED;
static inline void clear(VideoFrame *vf);
static inline bool compatible(const VideoFrame *a,
                              const VideoFrame *b) MUNUSED;
static inline int  bitsperpixel(VideoFrameType type);

static inline void init(VideoFrame *vf, VideoFrameType _codec,
                        unsigned char *_buf, int _width, int _height,
                        int _size, const int *p, const int *o,
                        float _aspect, double _rate)
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

    vf->qscale_table = 0;
    vf->qstride      = 0;

    vf->interlaced_frame = 1;
    vf->top_field_first  = 1;
    vf->repeat_pict      = 0;
    vf->forcekey         = 0;
    vf->dummy            = 0;
    vf->pix_fmt          = 0;
    vf->directrendering  = 1;

    memset(vf->priv, 0, 4 * sizeof(unsigned char *));

    if (p)
    {
        memcpy(vf->pitches, p, 3 * sizeof(int));
    }
    else
    {
        if (FMT_YV12 == _codec || FMT_YUV422P == _codec)
        {
            vf->pitches[0] = _width;
            vf->pitches[1] = vf->pitches[2] = _width >> 1;
        }
        else
        {
            vf->pitches[0] = (_width * vf->bpp) >> 3;
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
            vf->offsets[1] = _width * _height;
            vf->offsets[2] = vf->offsets[1] + (vf->offsets[1] >> 2);
        }
        else if (FMT_YUV422P == _codec)
        {
            vf->offsets[0] = 0;
            vf->offsets[1] = _width * _height;
            vf->offsets[2] = vf->offsets[1] + (vf->offsets[1] >> 1);
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

    if (FMT_YV12 == vf->codec)
    {
        int uv_height = vf->height >> 1;
        memset(vf->buf + vf->offsets[0],   0, vf->pitches[0] * vf->height);
        memset(vf->buf + vf->offsets[1], 127, vf->pitches[1] * uv_height);
        memset(vf->buf + vf->offsets[2], 127, vf->pitches[2] * uv_height);
    }
}

static inline bool compatible(const VideoFrame *a, const VideoFrame *b)
{
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

static inline void copy(VideoFrame *dst, const VideoFrame *src)
{
    VideoFrameType codec = dst->codec;
    if (dst->codec != src->codec)
        return;

    dst->interlaced_frame = src->interlaced_frame;
    dst->repeat_pict      = src->repeat_pict;
    dst->top_field_first  = src->top_field_first;

    if (FMT_YV12 == codec)
    {
        int height0 = (dst->height < src->height) ? dst->height : src->height;
        int height1 = height0 >> 1;
        int height2 = height0 >> 1;
        int pitch0  = ((dst->pitches[0] < src->pitches[0]) ?
                       dst->pitches[0] : src->pitches[0]);
        int pitch1  = ((dst->pitches[1] < src->pitches[1]) ?
                       dst->pitches[1] : src->pitches[1]);
        int pitch2  = ((dst->pitches[2] < src->pitches[2]) ?
                       dst->pitches[2] : src->pitches[2]);

        memcpy(dst->buf + dst->offsets[0],
               src->buf + src->offsets[0], pitch0 * height0);
        memcpy(dst->buf + dst->offsets[1],
               src->buf + src->offsets[1], pitch1 * height1);
        memcpy(dst->buf + dst->offsets[2],
               src->buf + src->offsets[2], pitch2 * height2);
    }
}

static inline int bitsperpixel(VideoFrameType type)
{
    int res = 8;
    switch (type)
    {
        case FMT_BGRA:
        case FMT_RGBA32:
        case FMT_ARGB32:
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
            res = 12;
            break;
        case FMT_IA44:
        case FMT_AI44:
        default:
            res = 8;
    }
    return res;
}

static inline uint buffersize(VideoFrameType type, int width, int height)
{
    int  type_bpp = bitsperpixel(type);
    uint bpp = type_bpp / 4; /* bits per pixel div common factor */
    uint bpb =  8 / 4; /* bits per byte div common factor */

    // If the buffer sizes are not a multple of 16, adjust.
    // old versions of MythTV allowed people to set invalid
    // dimensions for MPEG-4 capture, no need to segfault..
    uint adj_w = (width  + 15) & ~0xF;
    uint adj_h = (height + 15) & ~0xF;
    return (adj_w * adj_h * bpp + 4/* to round up */) / bpb;
}

#endif /* __cplusplus */

#endif

