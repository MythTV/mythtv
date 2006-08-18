#ifndef _FRAME_H
#define _FRAME_H 

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum FrameType_
{
    FMT_NONE = -1,
    FMT_RGB24 = 0,
    FMT_YV12,
    FMT_XVMC_IDCT_MPEG2,
    FMT_XVMC_MOCO_MPEG2,
    FMT_VIA_HWSLICE,
    FMT_IA44,
    FMT_AI44,
    FMT_ARGB32,
    FMT_YUV422P
} VideoFrameType;

typedef struct VideoFrame_
{
    VideoFrameType codec;
    unsigned char *buf;

    int width;
    int height;
    int bpp;
    int size;

    long long frameNumber;
    long long timecode;

    unsigned char *priv[4]; // random empty storage

    unsigned char *qscale_table;
    int            qstride;

    int interlaced_frame; // 1 if interlaced.
    int top_field_first; // 1 if top field is first.
    int repeat_pict;
    int forcekey; // hardware encoded .nuv

    int pitches[3]; // Y, U, & V pitches
    int offsets[3]; // Y, U, & V offsets
} VideoFrame;

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
static inline void init(
    VideoFrame *vf,
    VideoFrameType _codec, unsigned char *_buf,
    int _width, int _height, int _bpp, int _size,
    const int *p = 0, const int *o = 0) __attribute__ ((unused));

static inline void init(
    VideoFrame *vf,
    VideoFrameType _codec, unsigned char *_buf,
    int _width, int _height, int _bpp, int _size,
    const int *p, const int *o)
{
    vf->codec  = _codec;
    vf->buf    = _buf;
    vf->width  = _width;
    vf->height = _height;

    vf->bpp          = _bpp;
    vf->size         = _size;
    vf->frameNumber  = 0;
    vf->timecode     = 0;

    vf->qscale_table = 0;
    vf->qstride      = 0;

    vf->interlaced_frame = 1;
    vf->top_field_first  = 1;
    vf->repeat_pict      = 0;
    vf->forcekey         = 0;

    bzero(vf->priv, 4 * sizeof(unsigned char *));

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
            vf->pitches[0] = (_width * _bpp) >> 3;
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

#endif /* __cplusplus */

#endif

