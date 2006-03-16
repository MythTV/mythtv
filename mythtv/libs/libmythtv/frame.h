#ifndef _FRAME_H
#define _FRAME_H 

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

    int height;
    int width;
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
} VideoFrame;

#endif

