#ifndef MYTHFRAME_H
#define MYTHFRAME_H

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

#include "mythtvexp.h" // for MTV_PUBLIC
#include "mythaverror.h"

extern "C" {
#include "libavcodec/avcodec.h"
}

#define MYTH_WIDTH_ALIGNMENT 64
#define MYTH_HEIGHT_ALIGNMENT 16
enum VideoFrameType
{
    FMT_NONE = -1,
    // YV12 and variants
    FMT_YV12 = 0,
    FMT_YUV420P9,
    FMT_YUV420P10,
    FMT_YUV420P12,
    FMT_YUV420P14,
    FMT_YUV420P16,
    // RGB variants
    FMT_RGB24,
    FMT_BGRA,
    FMT_RGB32,   ///< endian dependent format, ARGB or BGRA
    FMT_ARGB32,
    FMT_RGBA32,
    // YUV422P and variants
    FMT_YUV422P,
    FMT_YUV422P9,
    FMT_YUV422P10,
    FMT_YUV422P12,
    FMT_YUV422P14,
    FMT_YUV422P16,
    // YUV444P and variants
    FMT_YUV444P,
    FMT_YUV444P9,
    FMT_YUV444P10,
    FMT_YUV444P12,
    FMT_YUV444P14,
    FMT_YUV444P16,
    // Packed YUV
    FMT_YUY2,
    // NV12 and variants
    FMT_NV12,
    FMT_P010,
    FMT_P016,
    // hardware formats
    FMT_VDPAU,
    FMT_VAAPI,
    FMT_DXVA2,
    FMT_MMAL,
    FMT_MEDIACODEC,
    FMT_VTB,
    FMT_NVDEC,
    FMT_DRMPRIME
};

const char* format_description(VideoFrameType Type);

static inline bool format_is_hw(VideoFrameType Type)
{
    return (Type == FMT_VDPAU) || (Type == FMT_VAAPI) ||
           (Type == FMT_DXVA2) || (Type == FMT_MMAL) ||
           (Type == FMT_MEDIACODEC) || (Type == FMT_VTB) ||
           (Type == FMT_NVDEC) || (Type == FMT_DRMPRIME);
}

static inline bool format_is_hwframes(VideoFrameType Type)
{
    return (Type == FMT_VDPAU) || (Type == FMT_VAAPI) || (Type == FMT_NVDEC);
}

static inline bool format_is_420(VideoFrameType Type)
{
    return (Type == FMT_YV12) || (Type == FMT_YUV420P9) || (Type == FMT_YUV420P10) ||
           (Type == FMT_YUV420P12) || (Type == FMT_YUV420P14) || (Type == FMT_YUV420P16);
}

static inline bool format_is_422(VideoFrameType Type)
{
    return (Type == FMT_YUV422P)   || (Type == FMT_YUV422P9) || (Type == FMT_YUV422P10) ||
           (Type == FMT_YUV422P12) || (Type == FMT_YUV422P14) || (Type == FMT_YUV422P16);
}

static inline bool format_is_444(VideoFrameType Type)
{
    return (Type == FMT_YUV444P)   || (Type == FMT_YUV444P9) || (Type == FMT_YUV444P10) ||
           (Type == FMT_YUV444P12) || (Type == FMT_YUV444P14) || (Type == FMT_YUV444P16);

}

static inline bool format_is_nv12(VideoFrameType Type)
{
    return (Type == FMT_NV12) || (Type == FMT_P010) || (Type == FMT_P016);
}

static inline bool format_is_packed(VideoFrameType Type)
{
    return Type == FMT_YUY2;
}

static inline bool format_is_yuv(VideoFrameType Type)
{
    return format_is_420(Type)  || format_is_422(Type) || format_is_444(Type) ||
           format_is_nv12(Type) || format_is_packed(Type);
}

enum MythDeintType
{
    DEINT_NONE   = 0x0000,
    DEINT_BASIC  = 0x0001,
    DEINT_MEDIUM = 0x0002,
    DEINT_HIGH   = 0x0004,
    DEINT_CPU    = 0x0010,
    DEINT_SHADER = 0x0020,
    DEINT_DRIVER = 0x0040,
    DEINT_ALL    = 0x0077
};

inline MythDeintType operator| (MythDeintType a, MythDeintType b) { return static_cast<MythDeintType>(static_cast<int>(a) | static_cast<int>(b)); }
inline MythDeintType operator& (MythDeintType a, MythDeintType b) { return static_cast<MythDeintType>(static_cast<int>(a) & static_cast<int>(b)); }
inline MythDeintType operator~ (MythDeintType a) { return static_cast<MythDeintType>(~(static_cast<int>(a))); }

using FramePitches = std::array<int,3>;
using FrameOffsets = std::array<int,3>;

struct VideoFrame
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
    long long frameCounter; ///< raw frame counter/ticker for discontinuity checks
    long long timecode;
    int64_t   disp_timecode;
    std::array<uint8_t*,4> priv; ///< random empty storage
    int interlaced_frame; ///< 1 if interlaced. 0 if not interlaced. -1 if unknown.
    bool top_field_first; ///< true if top field is first.
    bool interlaced_reversed; /// true for user override of scan
    bool new_gop; /// used to unlock the scan type
    bool repeat_pict;
    bool forcekey; ///< hardware encoded .nuv
    bool dummy;
    bool pause_frame;
    FramePitches pitches; ///< Y, U, & V pitches
    FrameOffsets offsets; ///< Y, U, & V offsets
    int pix_fmt;
    int sw_pix_fmt;
    bool directrendering; ///< true if managed by FFmpeg
    int colorspace;
    int colorrange;
    int colorprimaries;
    int colortransfer;
    int chromalocation;
    bool colorshifted; ///< false for software decoded 10/12/16bit frames. true for hardware decoders.
    bool already_deinterlaced; ///< temporary? TODO move scan detection/tracking into decoder
    int rotation;
    uint stereo3D;
    MythDeintType deinterlace_single;
    MythDeintType deinterlace_double;
    MythDeintType deinterlace_allowed;
    MythDeintType deinterlace_inuse;
    bool          deinterlace_inuse2x;
};

using VideoFrameTypeVec = std::vector<VideoFrameType>;

int MTV_PUBLIC ColorDepth(int Format);

MythDeintType MTV_PUBLIC GetSingleRateOption(const VideoFrame* Frame, MythDeintType Type, MythDeintType Override = DEINT_NONE);
MythDeintType MTV_PUBLIC GetDoubleRateOption(const VideoFrame* Frame, MythDeintType Type, MythDeintType Override = DEINT_NONE);
MythDeintType MTV_PUBLIC GetDeinterlacer(MythDeintType Option);

enum class uswcState {
    Detect,
    Use_SSE,
    Use_SW
};

class MTV_PUBLIC MythUSWCCopy
{
public:
    explicit MythUSWCCopy(int width, bool nocache = false);
    virtual ~MythUSWCCopy();

    void copy(VideoFrame *dst, const VideoFrame *src);
    void reset(int width);
    void resetUSWCDetection(void);
    void setUSWC(bool uswc);

private:
    // This function is needed on ARCH_X86
    // cppcheck-suppress unusedPrivateFunction
    void allocateCache(int width);

    uint8_t*  m_cache {nullptr};
    int       m_size  {0};
    uswcState m_uswc  {uswcState::Detect};
};

void MTV_PUBLIC framecopy(VideoFrame *dst, const VideoFrame *src,
                          bool useSSE = true);

static inline int  bitsperpixel(VideoFrameType type);
static inline int  pitch_for_plane(VideoFrameType Type, int Width, uint Plane);
static inline int  height_for_plane(VideoFrameType Type, int Height, uint Plane);

using mavtriplet = std::array<int,3>;
using mavtripletptr = mavtriplet::const_pointer;

static inline void init(VideoFrame *vf, VideoFrameType _codec,
                        unsigned char *_buf, int _width, int _height,
                        int _size,
                        const FramePitches p,
                        const FrameOffsets o,
                        bool arrays_valid = true,
                        float _aspect = -1.0F, double _rate = -1.0F,
                        int _aligned = MYTH_WIDTH_ALIGNMENT)
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
    vf->frameCounter = 0;
    vf->timecode     = 0;
    vf->interlaced_frame = 1;
    vf->interlaced_reversed = false;
    vf->new_gop          = false;
    vf->top_field_first  = true;
    vf->repeat_pict      = false;
    vf->forcekey         = false;
    vf->dummy            = false;
    vf->pause_frame      = false;
    vf->pix_fmt          = 0;
    vf->sw_pix_fmt       = -1; // AV_PIX_FMT_NONE
    vf->directrendering  = true;
    vf->colorspace       = 1; // BT.709
    vf->colorrange       = 1; // normal/mpeg
    vf->colorprimaries   = 1; // BT.709
    vf->colortransfer    = 1; // BT.709
    vf->chromalocation   = 1; // default 4:2:0
    vf->colorshifted     = false;
    vf->already_deinterlaced = false;
    vf->rotation         = 0;
    vf->stereo3D         = 0;
    vf->deinterlace_single = DEINT_NONE;
    vf->deinterlace_double = DEINT_NONE;
    vf->deinterlace_allowed = DEINT_NONE;
    vf->deinterlace_inuse = DEINT_NONE;
    vf->deinterlace_inuse2x = false;
    vf->priv.fill(nullptr);

    int width_aligned = 0;
    int height_aligned = (_height + MYTH_HEIGHT_ALIGNMENT - 1) & ~(MYTH_HEIGHT_ALIGNMENT -1);
    if (!_aligned)
        width_aligned = _width;
    else
        width_aligned = (_width + _aligned - 1) & ~(_aligned - 1);

    if (arrays_valid)
    {
        vf->pitches = p;
    }
    else
    {
        for (uint i = 0; i < 3; ++i)
            vf->pitches[i] = pitch_for_plane(_codec, width_aligned, i);
    }

    if (arrays_valid)
    {
        vf->offsets = o;
    }
    else
    {
        vf->offsets[0] = 0;
        if (FMT_YV12 == _codec)
        {
            vf->offsets[1] = width_aligned * height_aligned;
            vf->offsets[2] = vf->offsets[1] + ((width_aligned + 1) >> 1) * ((height_aligned+1) >> 1);
        }
        else if (format_is_420(_codec))
        {
            vf->offsets[1] = (width_aligned << 1) * height_aligned;
            vf->offsets[2] = vf->offsets[1] + (width_aligned * (height_aligned >> 1));
        }
        else if (FMT_YUV422P == _codec)
        {
            vf->offsets[1] = width_aligned * height_aligned;
            vf->offsets[2] = vf->offsets[1] + ((width_aligned + 1) >> 1) * height_aligned;
        }
        else if (format_is_422(_codec))
        {
            vf->offsets[1] = (width_aligned << 1) * height_aligned;
            vf->offsets[2] = vf->offsets[1] + (width_aligned * height_aligned);
        }
        else if (FMT_YUV444P == _codec)
        {
            vf->offsets[1] = width_aligned * height_aligned;
            vf->offsets[2] = vf->offsets[1] + (width_aligned * height_aligned);
        }
        else if (format_is_444(_codec))
        {
            vf->offsets[1] = (width_aligned << 1) * height_aligned;
            vf->offsets[2] = vf->offsets[1] + ((width_aligned << 1) * height_aligned);
        }
        else if (FMT_NV12 == _codec)
        {
            vf->offsets[1] = width_aligned * height_aligned;
            vf->offsets[2] = 0;
        }
        else if (format_is_nv12(_codec))
        {
            vf->offsets[1] = (width_aligned << 1) * height_aligned;
            vf->offsets[2] = 0;
        }
        else
        {
            vf->offsets[1] = vf->offsets[2] = 0;
        }
    }
}

static inline void init(VideoFrame *vf, VideoFrameType _codec,
                        unsigned char *_buf, int _width, int _height, int _size,
                        float _aspect = -1.0F, double _rate = -1.0F,
                        int _aligned = MYTH_WIDTH_ALIGNMENT)
{
    init(vf, _codec, _buf, _width, _height, _size, {}, {}, false,
         _aspect, _rate, _aligned);
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
        case FMT_YUV420P9:
        case FMT_YUV420P10:
        case FMT_YUV420P12:
        case FMT_YUV420P14:
        case FMT_YUV420P16:
        case FMT_YUV422P9:
        case FMT_YUV422P10:
        case FMT_YUV422P12:
        case FMT_YUV422P14:
        case FMT_YUV422P16:
            if (Plane == 0) return Width << 1;
            if (Plane < 3)  return Width;
            break;
        case FMT_YUV444P:
        case FMT_YUV444P9:
        case FMT_YUV444P10:
        case FMT_YUV444P12:
        case FMT_YUV444P14:
        case FMT_YUV444P16:
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

static inline int width_for_plane(VideoFrameType Type, int Width, uint Plane)
{
    switch (Type)
    {
        case FMT_YV12:
        case FMT_YUV420P9:
        case FMT_YUV420P10:
        case FMT_YUV420P12:
        case FMT_YUV420P14:
        case FMT_YUV420P16:
        case FMT_YUV422P:
        case FMT_YUV422P9:
        case FMT_YUV422P10:
        case FMT_YUV422P12:
        case FMT_YUV422P14:
        case FMT_YUV422P16:
            if (Plane == 0) return Width;
            if (Plane < 3)  return (Width + 1) >> 1;
            break;
        case FMT_YUV444P:
        case FMT_YUV444P9:
        case FMT_YUV444P10:
        case FMT_YUV444P12:
        case FMT_YUV444P14:
        case FMT_YUV444P16:
            if (Plane < 3)  return Width;
            break;
        case FMT_NV12:
        case FMT_P010:
        case FMT_P016:
            if (Plane < 2) return Width;
            break;
        case FMT_YUY2:
        case FMT_RGB24:
        case FMT_ARGB32:
        case FMT_RGBA32:
        case FMT_BGRA:
        case FMT_RGB32:
            if (Plane == 0) return Width;
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
        case FMT_YUV420P9:
        case FMT_YUV420P10:
        case FMT_YUV420P12:
        case FMT_YUV420P14:
        case FMT_YUV420P16:
            if (Plane == 0) return Height;
            if (Plane < 3)  return Height >> 1;
            break;
        case FMT_YUV422P:
        case FMT_YUV422P9:
        case FMT_YUV422P10:
        case FMT_YUV422P12:
        case FMT_YUV422P14:
        case FMT_YUV422P16:
        case FMT_YUV444P:
        case FMT_YUV444P9:
        case FMT_YUV444P10:
        case FMT_YUV444P12:
        case FMT_YUV444P14:
        case FMT_YUV444P16:
            if (Plane < 3)  return Height;
            break;
        case FMT_NV12:
        case FMT_P010:
        case FMT_P016:
            if (Plane == 0) return Height;
            if (Plane < 2) return Height >> 1;
            break;
        case FMT_YUY2:
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
static inline void clear_vf(VideoFrame *vf)
{
    if (!vf || !vf->buf)
        return;

    // luma (or RGBA)
    int uv_height = height_for_plane(vf->codec, vf->height, 1);
    int uv = (1 << (ColorDepth(vf->codec) - 1)) - 1;
    if (FMT_YV12 == vf->codec || FMT_YUV422P == vf->codec || FMT_YUV444P == vf->codec)
    {
        memset(vf->buf + vf->offsets[0], 0, static_cast<size_t>(vf->pitches[0] * vf->height));
        memset(vf->buf + vf->offsets[1], uv & 0xff, static_cast<size_t>(vf->pitches[1] * uv_height));
        memset(vf->buf + vf->offsets[2], uv & 0xff, static_cast<size_t>(vf->pitches[2] * uv_height));
    }
    else if ((format_is_420(vf->codec) || format_is_422(vf->codec) || format_is_444(vf->codec)) && (vf->pitches[1] == vf->pitches[2]))
    {
        memset(vf->buf + vf->offsets[0], 0, static_cast<size_t>(vf->pitches[0] * vf->height));
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
    else if (FMT_NV12 == vf->codec)
    {
        memset(vf->buf + vf->offsets[0], 0, static_cast<size_t>(vf->pitches[0] * vf->height));
        memset(vf->buf + vf->offsets[1], uv & 0xff, vf->pitches[1] * uv_height);
    }
    else if (format_is_nv12(vf->codec))
    {
        memset(vf->buf + vf->offsets[0], 0, static_cast<size_t>(vf->pitches[0] * vf->height));
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
    if ((dst_pitch == width) && (src_pitch == width))
    {
        memcpy(dst, src, static_cast<size_t>(width * height));
        return;
    }

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
        case FMT_YUV420P9:
        case FMT_YUV420P10:
        case FMT_YUV420P12:
        case FMT_YUV420P14:
        case FMT_YUV420P16:
        case FMT_YUV422P:
        case FMT_YUV422P9:
        case FMT_YUV422P10:
        case FMT_YUV422P12:
        case FMT_YUV422P14:
        case FMT_YUV422P16:
        case FMT_YUV444P:
        case FMT_YUV444P9:
        case FMT_YUV444P10:
        case FMT_YUV444P12:
        case FMT_YUV444P14:
        case FMT_YUV444P16: return 3;
        case FMT_P010:
        case FMT_P016:
        case FMT_NV12:      return 2;
        case FMT_YUY2:
        case FMT_BGRA:
        case FMT_ARGB32:
        case FMT_RGB24:
        case FMT_RGB32:
        case FMT_RGBA32:    return 1;
        case FMT_NONE:
        case FMT_VDPAU:
        case FMT_VAAPI:
        case FMT_DXVA2:
        case FMT_MEDIACODEC:
        case FMT_NVDEC:
        case FMT_MMAL:
        case FMT_DRMPRIME:
        case FMT_VTB:       return 0;
    }
    return 0;
}

static inline int bitsperpixel(VideoFrameType Type)
{
    switch (Type)
    {
        case FMT_BGRA:
        case FMT_RGBA32:
        case FMT_ARGB32:
        case FMT_RGB32:
        case FMT_YUV422P9:
        case FMT_YUV422P10:
        case FMT_YUV422P12:
        case FMT_YUV422P14:
        case FMT_YUV422P16: return 32;
        case FMT_RGB24:     return 24;
        case FMT_YUV422P:
        case FMT_YUY2:      return 16;
        case FMT_YV12:
        case FMT_NV12:      return 12;
        case FMT_YUV444P:
        case FMT_P010:
        case FMT_P016:
        case FMT_YUV420P9:
        case FMT_YUV420P10:
        case FMT_YUV420P12:
        case FMT_YUV420P14:
        case FMT_YUV420P16:  return 24;
        case FMT_YUV444P9:
        case FMT_YUV444P10:
        case FMT_YUV444P12:
        case FMT_YUV444P14:
        case FMT_YUV444P16:  return 48;
        case FMT_NONE:
        case FMT_VDPAU:
        case FMT_VAAPI:
        case FMT_DXVA2:
        case FMT_MMAL:
        case FMT_MEDIACODEC:
        case FMT_NVDEC:
        case FMT_DRMPRIME:
        case FMT_VTB: break;
    }
    return 8;
}

static inline size_t GetBufferSize(VideoFrameType Type, int Width, int Height,
                                   int Aligned = MYTH_WIDTH_ALIGNMENT)
{
    int  type_bpp = bitsperpixel(Type);
    int bpp = type_bpp / 4; /* bits per pixel div common factor */
    int bpb =  8 / 4; /* bits per byte div common factor */

    // make sure all our pitches are a multiple of 16 bytes
    // as U and V channels are half the size of Y channel
    // This allow for SSE/HW accelerated code on all planes
    // If the buffer sizes are not 32 bytes aligned, adjust.
    // old versions of MythTV allowed people to set invalid
    // dimensions for MPEG-4 capture, no need to segfault..
    int adj_w = Aligned ? ((Width  + Aligned - 1) & ~(Aligned - 1)) : Width;
    int adj_h = (Height + MYTH_HEIGHT_ALIGNMENT - 1) & ~(MYTH_HEIGHT_ALIGNMENT - 1);

    // Calculate rounding as necessary.
    int remainder = (adj_w * adj_h * bpp) % bpb;
    return static_cast<uint>((adj_w * adj_h * bpp) / bpb + (remainder ? 1 : 0));
}

static inline unsigned char* GetAlignedBuffer(size_t Size)
{
    return static_cast<unsigned char*>(av_malloc(Size + 64));
}

static inline unsigned char* GetAlignedBufferZero(size_t Size)
{
    return static_cast<unsigned char*>(av_mallocz(Size + 64));
}

static inline unsigned char* CreateBuffer(VideoFrameType Type, int Width, int Height)
{
    size_t size = GetBufferSize(Type, Width, Height);
    return GetAlignedBuffer(size);
}

static inline unsigned char* CreateBufferZero(VideoFrameType Type, int Width, int Height)
{
    size_t size = GetBufferSize(Type, Width, Height);
    return GetAlignedBufferZero(size);
}
#endif /* MYTHFRAME_H */
