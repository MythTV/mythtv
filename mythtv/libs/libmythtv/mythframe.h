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
    // YV12 variants
    FMT_YUV420P10,
    FMT_YUV420P12,
    FMT_YUV420P16,
    // NV12 variants
    FMT_P010,
    FMT_P016,
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

static inline int format_is_hwyuv(VideoFrameType Type)
{
    return (Type == FMT_NVDEC) || (Type == FMT_VTB);
}

static inline int format_is_yuv(VideoFrameType Type)
{
    return (Type == FMT_YV12) || (Type == FMT_YUV422P) ||
           (Type == FMT_YUY2) || (Type == FMT_NV12) ||
           (Type == FMT_YUYVHQ) || (Type == FMT_YUV420P10) ||
           (Type == FMT_YUV420P12) || (Type == FMT_YUV420P16) ||
           (Type == FMT_P010) || (Type == FMT_P016);
}

static inline int format_is_420(VideoFrameType Type)
{
    return (Type == FMT_YV12) || (Type == FMT_YUV420P10) ||
           (Type == FMT_YUV420P12) || (Type == FMT_YUV420P16);
}

static inline int format_is_nv12(VideoFrameType Type)
{
    return (Type == FMT_NV12) || (Type == FMT_P010) || (Type == FMT_P016);
}

typedef enum MythDeintType
{
    DEINT_NONE   = 0x0000,
    DEINT_BASIC  = 0x0001,
    DEINT_MEDIUM = 0x0002,
    DEINT_HIGH   = 0x0004,
    DEINT_CPU    = 0x0010,
    DEINT_SHADER = 0x0020,
    DEINT_DRIVER = 0x0040,
    DEINT_ALL    = 0xFFFF
} MythDeintType;

inline MythDeintType operator| (MythDeintType a, MythDeintType b) { return static_cast<MythDeintType>(static_cast<int>(a) | static_cast<int>(b)); }
inline MythDeintType operator& (MythDeintType a, MythDeintType b) { return static_cast<MythDeintType>(static_cast<int>(a) & static_cast<int>(b)); }
inline MythDeintType operator~ (MythDeintType a) { return static_cast<MythDeintType>(~(static_cast<int>(a))); }

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
    int interlaced_frame; ///< 1 if interlaced.
    int top_field_first; ///< 1 if top field is first.
    int interlaced_reversed; /// 1 for user override of scan
    int repeat_pict;
    int forcekey; ///< hardware encoded .nuv
    int dummy;
    int pitches[3]; ///< Y, U, & V pitches
    int offsets[3]; ///< Y, U, & V offsets
    int pix_fmt;
    int sw_pix_fmt;
    int directrendering; ///< 1 if managed by FFmpeg
    int colorspace;
    int colorrange;
    int colorprimaries;
    int colortransfer;
    int chromalocation;
    MythDeintType deinterlace_single;
    MythDeintType deinterlace_double;
    MythDeintType deinterlace_allowed;
} VideoFrame;

#ifdef __cplusplus
}
#endif

int MTV_PUBLIC ColorDepth(int Format);

#ifdef __cplusplus

MythDeintType MTV_PUBLIC GetSingleRateOption(const VideoFrame* Frame, MythDeintType Type);
MythDeintType MTV_PUBLIC GetDoubleRateOption(const VideoFrame* Frame, MythDeintType Type);
MythDeintType MTV_PUBLIC GetDeinterlacer(MythDeintType Option);

enum class uswcState {
    Detect,
    Use_SSE,
    Use_SW
};

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

    uint8_t*  m_cache {nullptr};
    int       m_size  {0};
    uswcState m_uswc  {uswcState::Detect};
};

void MTV_PUBLIC framecopy(VideoFrame *dst, const VideoFrame *src,
                          bool useSSE = true);

static inline void init(VideoFrame *vf, VideoFrameType _codec,
                        unsigned char *_buf, int _width, int _height, int _size,
                        const int *p = nullptr,
                        const int *o = nullptr,
                        float _aspect = -1.0F, double _rate = -1.0F,
                        int _aligned = 64);
static inline void clear(VideoFrame *vf);
static inline int  bitsperpixel(VideoFrameType type);
static inline int  pitch_for_plane(VideoFrameType Type, int Width, uint Plane);
static inline int  height_for_plane(VideoFrameType Type, int Height, uint Plane);

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
    vf->interlaced_frame = 1;
    vf->interlaced_reversed = 0;
    vf->top_field_first  = 1;
    vf->repeat_pict      = 0;
    vf->forcekey         = 0;
    vf->dummy            = 0;
    vf->pix_fmt          = 0;
    vf->sw_pix_fmt       = -1; // AV_PIX_FMT_NONE
    vf->directrendering  = 1;
    vf->colorspace       = 1; // BT.709
    vf->colorrange       = 1; // normal/mpeg
    vf->colorprimaries   = 1; // BT.709
    vf->colortransfer    = 1; // BT.709
    vf->chromalocation   = 1; // default 4:2:0
    vf->deinterlace_single = DEINT_NONE;
    vf->deinterlace_double = DEINT_NONE;
    vf->deinterlace_allowed = DEINT_NONE;

    memset(vf->priv, 0, 4 * sizeof(unsigned char *));

    int width_aligned;
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
        for (int i = 0; i < 3; ++i)
            vf->pitches[i] = pitch_for_plane(_codec, width_aligned, i);
    }

    if (o)
    {
        memcpy(vf->offsets, o, 3 * sizeof(int));
    }
    else
    {
        vf->offsets[0] = 0;
        if (FMT_YV12 == _codec)
        {
            vf->offsets[1] = width_aligned * _height;
            vf->offsets[2] = vf->offsets[1] + ((width_aligned + 1) >> 1) * ((_height+1) >> 1);
        }
        else if (FMT_YUV420P10 == _codec || FMT_YUV420P12 == _codec ||
                 FMT_YUV420P16 == _codec)
        {
            vf->offsets[1] = (width_aligned << 1) * _height;
            vf->offsets[2] = vf->offsets[1] + (width_aligned * (_height >> 1));
        }
        else if (FMT_YUV422P == _codec)
        {
            vf->offsets[1] = width_aligned * _height;
            vf->offsets[2] = vf->offsets[1] + ((width_aligned + 1) >> 1) * _height;
        }
        else if (FMT_NV12 == _codec)
        {
            vf->offsets[1] = width_aligned * _height;
            vf->offsets[2] = 0;
        }
        else if (FMT_P010 == _codec || FMT_P016 == _codec)
        {
            vf->offsets[1] = (width_aligned << 1) * _height;
            vf->offsets[2] = 0;
        }
        else
        {
            vf->offsets[1] = vf->offsets[2] = 0;
        }
    }
}

static inline int pitch_for_plane(VideoFrameType Type, int Width, uint Plane)
{
    switch (Type)
    {
        case FMT_YV12:
        case FMT_YUV422P:
            if (Plane == 0) return Width;
            if (Plane < 3)  return (Width + 1) >> 1;
            break;
        case FMT_YUV420P10:
        case FMT_YUV420P12:
        case FMT_YUV420P16:
            if (Plane == 0) return Width << 1;
            if (Plane < 3)  return Width;
            break;
        case FMT_NV12:
            if (Plane < 2) return Width;
            break;
        case FMT_P010:
        case FMT_P016:
            if (Plane < 2) return Width << 1;
            break;
        case FMT_YUY2:
        case FMT_YUYVHQ:
        case FMT_RGB24:
        case FMT_ARGB32:
        case FMT_RGBA32:
        case FMT_BGRA:
        case FMT_RGB32:
            if (Plane == 0) return (bitsperpixel(Type) * Width) >> 3;
            break;
        default: break; // None and hardware formats
    }
    return 0;
}

static inline int height_for_plane(VideoFrameType Type, int Height, uint Plane)
{
    switch (Type)
    {
        case FMT_YV12:
        case FMT_YUV422P:
        case FMT_YUV420P10:
        case FMT_YUV420P12:
        case FMT_YUV420P16:
            if (Plane == 0) return Height;
            if (Plane < 3)  return Height >> 1;
            break;
        case FMT_NV12:
        case FMT_P010:
        case FMT_P016:
            if (Plane < 2) return Height;
            break;
        case FMT_YUY2:
        case FMT_YUYVHQ:
        case FMT_RGB24:
        case FMT_ARGB32:
        case FMT_RGBA32:
        case FMT_BGRA:
        case FMT_RGB32:
            if (Plane == 0) return Height;
            break;
        default: break; // None and hardware formats
    }
    return 0;
}
static inline void clear(VideoFrame *vf)
{
    if (!vf || (vf && !vf->buf))
        return;

    int uv_height = (vf->height+1) >> 1;
    int uv = (2 ^ (ColorDepth(vf->codec) - 1)) - 1;
    if (FMT_YV12 == vf->codec)
    {
        memset(vf->buf + vf->offsets[0],         0, vf->pitches[0] * vf->height);
        memset(vf->buf + vf->offsets[1], uv & 0xff, vf->pitches[1] * uv_height);
        memset(vf->buf + vf->offsets[2], uv & 0xff, vf->pitches[2] * uv_height);
    }
    else if (FMT_NV12 == vf->codec)
    {
        memset(vf->buf + vf->offsets[0],         0, vf->pitches[0] * vf->height);
        memset(vf->buf + vf->offsets[1], uv & 0xff, vf->pitches[1] * uv_height);
    }
    else if (FMT_YUV420P10 == vf->codec || FMT_YUV420P12 == vf->codec ||
             FMT_YUV420P16 == vf->codec)
    {
        memset(vf->buf + vf->offsets[0], 0, vf->pitches[0] * vf->height);
        if (vf->pitches[1] == vf->pitches[2])
        {
            unsigned char uv1 = (uv & 0xff00) >> 8;
            unsigned char uv2 = (uv & 0x00ff);
            unsigned char* buf1 = vf->buf + vf->offsets[1];
            unsigned char* buf2 = vf->buf + vf->offsets[2];
            for (int row = 0; row < uv_height; ++row)
            {
                for (int col = 0; col < vf->pitches[1]; col += 2)
                {
                    buf1[col]     = buf2[col]     = uv1;
                    buf1[col + 1] = buf2[col + 1] = uv2;
                }
                buf1 += vf->pitches[1];
                buf2 += vf->pitches[2];
            }
        }
    }
    else if (FMT_P010 == vf->codec || FMT_P016 == vf->codec)
    {
        memset(vf->buf + vf->offsets[0], 0, vf->pitches[0] * vf->height);
        unsigned char uv1 = (uv & 0xff00) >> 8;
        unsigned char uv2 = (uv & 0x00ff);
        unsigned char* buf = vf->buf + vf->offsets[1];
        for (int row = 0; row < uv_height; ++row)
        {
            for (int col = 0; col < vf->pitches[1]; col += 4)
            {
                buf[col]     = buf[col + 2] = uv1;
                buf[col + 1] = buf[col + 3] = uv2;
            }
            buf += vf->pitches[1];
        }
    }
}

static inline void copyplane(uint8_t* dst, int dst_pitch,
                             const uint8_t* src, int src_pitch,
                             int width, int height)
{
    for (int y = 0; y < height; y++)
    {
        memcpy(dst, src, static_cast<size_t>(width));
        src += src_pitch;
        dst += dst_pitch;
    }
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
        case FMT_P010:
        case FMT_P016:
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
        case FMT_P010:
        case FMT_P016:
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
        pitch = src->width;

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
