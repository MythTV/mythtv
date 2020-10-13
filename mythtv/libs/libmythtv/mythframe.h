#ifndef MYTHFRAME_H
#define MYTHFRAME_H

// MythTV
#include "mythtvexp.h"
#include "mythaverror.h"

// Std
#include <array>
#include <vector>

// FFmpeg
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

class MythVideoFrame;
MythDeintType MTV_PUBLIC GetSingleRateOption(const MythVideoFrame* Frame, MythDeintType Type, MythDeintType Override = DEINT_NONE);
MythDeintType MTV_PUBLIC GetDoubleRateOption(const MythVideoFrame* Frame, MythDeintType Type, MythDeintType Override = DEINT_NONE);
MythDeintType MTV_PUBLIC GetDeinterlacer(MythDeintType Option);

using VideoFrameTypes = std::vector<VideoFrameType>;
using FramePitches = std::array<int,3>;
using FrameOffsets = std::array<int,3>;

class MTV_PUBLIC MythVideoFrame
{
  public:
    MythVideoFrame() = default;
    MythVideoFrame(VideoFrameType Type, uint8_t* Buffer, size_t BufferSize,
                   int Width, int Height, int Alignment = MYTH_WIDTH_ALIGNMENT);
   ~MythVideoFrame();
    void Init(VideoFrameType Type, uint8_t* Buffer, size_t BufferSize,
              int Width, int Height, int Alignment = MYTH_WIDTH_ALIGNMENT);

    void ClearMetadata();
    void ClearBufferToBlank();
    bool CopyFrame(MythVideoFrame* From);

    VideoFrameType codec               { FMT_NONE };
    uint8_t*       buf                 { nullptr  };
    int            width               { 0 };
    int            height              { 0 };
    int            bpp                 { 0 };
    size_t         size                { 0 };

    // everthing below is considered metadata by ClearMetadata
    float          aspect              { -1.0F };
    double         frame_rate          { -1.0 };
    long long      frameNumber         { 0 };
    long long      frameCounter        { 0 }; ///< raw frame counter/ticker for discontinuity checks
    long long      timecode            { 0 };
    int64_t        disp_timecode       { 0 };
    std::array<uint8_t*,4> priv        { nullptr }; ///< random empty storage
    int            interlaced_frame    { 0    }; ///< 1 if interlaced. 0 if not interlaced. -1 if unknown.
    bool           top_field_first     { true }; ///< true if top field is first.
    bool           interlaced_reversed { false }; /// true for user override of scan
    bool           new_gop             { false }; /// used to unlock the scan type
    bool           repeat_pict         { false };
    bool           forcekey            { false }; ///< hardware encoded .nuv
    bool           dummy               { false };
    bool           pause_frame         { false };
    FramePitches   pitches             { 0 }; ///< Y, U, & V pitches
    FrameOffsets   offsets             { 0 }; ///< Y, U, & V offsets
    int            pix_fmt             { 0 };
    int            sw_pix_fmt          { 0 };
    bool           directrendering     { true }; ///< true if managed by FFmpeg
    int            colorspace          { 1 };
    int            colorrange          { 1 };
    int            colorprimaries      { 1 };
    int            colortransfer       { 1 };
    int            chromalocation      { 1 };
    bool           colorshifted        { false }; ///< false for software decoded 10/12/16bit frames. true for hardware decoders.
    bool           already_deinterlaced { false }; ///< temporary? TODO move scan detection/tracking into decoder
    int            rotation            { 0 };
    uint           stereo3D            { 0 };
    MythDeintType  deinterlace_single  { DEINT_NONE };
    MythDeintType  deinterlace_double  { DEINT_NONE };
    MythDeintType  deinterlace_allowed { DEINT_NONE };
    MythDeintType  deinterlace_inuse   { DEINT_NONE };
    bool           deinterlace_inuse2x { false };

    static void       CopyPlane(uint8_t* To, int ToPitch,
                                const uint8_t* From, int FromPitch,
                                int PlaneWidth, int PlaneHeight);
    static QString    FormatDescription(VideoFrameType Type);
    static uint8_t*   GetAlignedBuffer(size_t Size);
    static uint8_t*   CreateBuffer(VideoFrameType Type, int Width, int Height);
    static size_t     GetBufferSize(VideoFrameType Type, int Width, int Height, int Aligned = MYTH_WIDTH_ALIGNMENT);

    static inline int BitsPerPixel(VideoFrameType Type)
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

    static inline uint GetNumPlanes(VideoFrameType Type)
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

    static inline int GetHeightForPlane(VideoFrameType Type, int Height, uint Plane)
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

    static inline int GetPitchForPlane(VideoFrameType Type, int Width, uint Plane)
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
                if (Plane == 0) return (MythVideoFrame::BitsPerPixel(Type) * Width) >> 3;
                break;
            default: break; // None and hardware formats
        }
        return 0;
    }

    static inline int GetWidthForPlan(VideoFrameType Type, int Width, uint Plane)
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

    static inline int ColorDepth(int Format)
    {
        switch (Format)
        {
            case FMT_YUV420P9:
            case FMT_YUV422P9:
            case FMT_YUV444P9:  return 9;
            case FMT_P010:
            case FMT_YUV420P10:
            case FMT_YUV422P10:
            case FMT_YUV444P10: return 10;
            case FMT_YUV420P12:
            case FMT_YUV422P12:
            case FMT_YUV444P12: return 12;
            case FMT_YUV420P14:
            case FMT_YUV422P14:
            case FMT_YUV444P14: return 14;
            case FMT_P016:
            case FMT_YUV420P16:
            case FMT_YUV422P16:
            case FMT_YUV444P16: return 16;
            default: break;
        }
        return 8;
    }

    static inline bool HardwareFormat(VideoFrameType Type)
    {
        return (Type == FMT_VDPAU) || (Type == FMT_VAAPI) ||
               (Type == FMT_DXVA2) || (Type == FMT_MMAL) ||
               (Type == FMT_MEDIACODEC) || (Type == FMT_VTB) ||
               (Type == FMT_NVDEC) || (Type == FMT_DRMPRIME);
    }

    static inline bool HardwareFramesFormat(VideoFrameType Type)
    {
        return (Type == FMT_VDPAU) || (Type == FMT_VAAPI) || (Type == FMT_NVDEC);
    }

    static inline bool FormatIs420(VideoFrameType Type)
    {
        return (Type == FMT_YV12) || (Type == FMT_YUV420P9) || (Type == FMT_YUV420P10) ||
               (Type == FMT_YUV420P12) || (Type == FMT_YUV420P14) || (Type == FMT_YUV420P16);
    }

    static inline bool FormatIs422(VideoFrameType Type)
    {
        return (Type == FMT_YUV422P)   || (Type == FMT_YUV422P9) || (Type == FMT_YUV422P10) ||
               (Type == FMT_YUV422P12) || (Type == FMT_YUV422P14) || (Type == FMT_YUV422P16);
    }

    static inline bool FormatIs444(VideoFrameType Type)
    {
        return (Type == FMT_YUV444P)   || (Type == FMT_YUV444P9) || (Type == FMT_YUV444P10) ||
               (Type == FMT_YUV444P12) || (Type == FMT_YUV444P14) || (Type == FMT_YUV444P16);
    }

    static inline bool FormatIsNV12(VideoFrameType Type)
    {
        return (Type == FMT_NV12) || (Type == FMT_P010) || (Type == FMT_P016);
    }

    static inline bool PackedFormat(VideoFrameType Type)
    {
        return Type == FMT_YUY2;
    }

    static inline bool YUVFormat(VideoFrameType Type)
    {
        return FormatIs420(Type)  || FormatIs422(Type) || FormatIs444(Type) ||
               FormatIsNV12(Type) || PackedFormat(Type);
    }

  private:
    //Q_DISABLE_COPY(MythVideoFrame)
};

#endif
