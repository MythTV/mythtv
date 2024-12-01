#ifndef MYTHFRAME_H
#define MYTHFRAME_H

// Qt
#include <QRect>

// MythTV
#include "libmythbase/mythchrono.h"
#include "libmythtv/mythtvexp.h"

// Std
#include <array>
#include <vector>
#include <memory>

static constexpr uint8_t MYTH_WIDTH_ALIGNMENT  { 64 };
static constexpr uint8_t MYTH_HEIGHT_ALIGNMENT { 16 };

enum VideoFrameType : std::int8_t
{
    FMT_NONE = -1,
    // YV12 and variants
    FMT_YV12 = 0,
    FMT_YUV420P9  =  1,
    FMT_YUV420P10 =  2,
    FMT_YUV420P12 =  3,
    FMT_YUV420P14 =  4,
    FMT_YUV420P16 =  5,
    // RGB variants
    FMT_RGB24     =  6,
    FMT_BGRA      =  7,
    FMT_RGB32     =  8, ///< endian dependent format, ARGB or BGRA
    FMT_ARGB32    =  9,
    FMT_RGBA32    = 10,
    // YUV422P and variants
    FMT_YUV422P   = 11,
    FMT_YUV422P9  = 12,
    FMT_YUV422P10 = 13,
    FMT_YUV422P12 = 14,
    FMT_YUV422P14 = 15,
    FMT_YUV422P16 = 16,
    // YUV444P and variants
    FMT_YUV444P   = 17,
    FMT_YUV444P9  = 18,
    FMT_YUV444P10 = 19,
    FMT_YUV444P12 = 20,
    FMT_YUV444P14 = 21,
    FMT_YUV444P16 = 22,
    // Packed YUV
    FMT_YUY2      = 23,
    // NV12 and variants
    FMT_NV12      = 24,
    FMT_P010      = 25,
    FMT_P016      = 26,
    // hardware formats
    FMT_VDPAU    = 27,
    FMT_VAAPI    = 28,
    FMT_DXVA2    = 29,
    FMT_MMAL     = 30,
    FMT_MEDIACODEC = 31,
    FMT_VTB      = 32,
    FMT_NVDEC    = 33,
    FMT_DRMPRIME = 34
};

enum MythDeintType : std::uint8_t
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

using VideoFrameTypes = std::vector<VideoFrameType>;
using FramePitches = std::array<int,3>;
using FrameOffsets = std::array<int,3>;
using MythHDRVideoPtr = std::shared_ptr<class MythHDRVideoMetadata>;

class MTV_PUBLIC MythVideoFrame
{
  public:
    static inline const VideoFrameTypes kDefaultRenderFormats { FMT_YV12 };

    MythVideoFrame() = default;
    MythVideoFrame(VideoFrameType Type, int Width, int Height, const VideoFrameTypes* RenderFormats = nullptr);
    MythVideoFrame(VideoFrameType Type, uint8_t* Buffer, size_t BufferSize,
                   int Width, int Height, const VideoFrameTypes* RenderFormats = nullptr,
                   int Alignment = MYTH_WIDTH_ALIGNMENT);
   ~MythVideoFrame();

    void Init(VideoFrameType Type, int Width, int Height, const VideoFrameTypes* RenderFormats = nullptr);
    void Init(VideoFrameType Type, uint8_t* Buffer, size_t BufferSize,
              int Width, int Height, const VideoFrameTypes* RenderFormats = nullptr, int Alignment = MYTH_WIDTH_ALIGNMENT);
    void ClearMetadata();
    void ClearBufferToBlank();
    bool CopyFrame(MythVideoFrame* From);
    MythDeintType GetSingleRateOption(MythDeintType Type, MythDeintType Override = DEINT_NONE) const;
    MythDeintType GetDoubleRateOption(MythDeintType Type, MythDeintType Override = DEINT_NONE) const;

    static void     CopyPlane(uint8_t* To, int ToPitch, const uint8_t* From, int FromPitch,
                              int PlaneWidth, int PlaneHeight);
    static QString  FormatDescription(VideoFrameType Type);
    static uint8_t* GetAlignedBuffer(size_t Size);
    static uint8_t* CreateBuffer(VideoFrameType Type, int Width, int Height);
    static size_t   GetBufferSize(VideoFrameType Type, int Width, int Height, int Aligned = MYTH_WIDTH_ALIGNMENT);
    static QString  DeinterlacerPref(MythDeintType Deint);
    static QString  DeinterlacerName(MythDeintType Deint, bool DoubleRate, VideoFrameType Format = FMT_NONE);
    static MythDeintType ParseDeinterlacer(const QString& Deinterlacer);

    VideoFrameType m_type              { FMT_NONE };
    uint8_t*       m_buffer            { nullptr  };
    int            m_width             { 0 };
    int            m_height            { 0 };
    int            m_bitsPerPixel      { 0 };
    size_t         m_bufferSize        { 0 };

    // everthing below is considered metadata by ClearMetadata
    float          m_aspect            { -1.0F };
    double         m_frameRate         { -1.0 };
    long long      m_frameNumber       { 0 };
    uint64_t       m_frameCounter      { 0 };
    std::chrono::milliseconds m_timecode          { 0ms };
    std::chrono::milliseconds m_displayTimecode   { 0ms };
    std::array<uint8_t*,4> m_priv      { nullptr };
    bool           m_interlaced        { false };
    bool           m_topFieldFirst     { true };
    bool           m_interlacedReverse { false };
    bool           m_newGOP            { false };
    bool           m_repeatPic         { false };
    bool           m_forceKey          { false };
    bool           m_dummy             { false };
    bool           m_pauseFrame        { false };
    FramePitches   m_pitches           { 0 };
    FrameOffsets   m_offsets           { 0 };
    int            m_pixFmt            { 0 };
    int            m_swPixFmt          { 0 };
    bool           m_directRendering   { true };
    const VideoFrameTypes* m_renderFormats { &kDefaultRenderFormats };
    int            m_colorspace        { 1 };
    int            m_colorrange        { 1 };
    int            m_colorprimaries    { 1 };
    int            m_colortransfer     { 1 };
    int            m_chromalocation    { 1 };
    bool           m_colorshifted      { false };
    bool           m_alreadyDeinterlaced { false };
    int            m_rotation          { 0 };
    uint           m_stereo3D          { 0 };
    MythHDRVideoPtr m_hdrMetadata      { nullptr };
    MythDeintType  m_deinterlaceSingle { DEINT_NONE };
    MythDeintType  m_deinterlaceDouble { DEINT_NONE };
    MythDeintType  m_deinterlaceAllowed { DEINT_NONE };
    MythDeintType  m_deinterlaceInuse  { DEINT_NONE };
    bool           m_deinterlaceInuse2x { false };

    // Presentation details for 'pure' direct rendering methods (e.g. DRM)
    // Experimental and may be removed.
    bool           m_displayed { false };
    QRect          m_srcRect;
    QRect          m_dstRect;

    static int BitsPerPixel(VideoFrameType Type)
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

    static uint GetNumPlanes(VideoFrameType Type)
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

    static int GetHeightForPlane(VideoFrameType Type, int Height, uint Plane)
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

    static int GetPitchForPlane(VideoFrameType Type, int Width, uint Plane)
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

    static int GetWidthForPlane(VideoFrameType Type, int Width, uint Plane)
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

    static int ColorDepth(int Format)
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

    static bool HardwareFormat(VideoFrameType Type)
    {
        return (Type == FMT_VDPAU) || (Type == FMT_VAAPI) ||
               (Type == FMT_DXVA2) || (Type == FMT_MMAL) ||
               (Type == FMT_MEDIACODEC) || (Type == FMT_VTB) ||
               (Type == FMT_NVDEC) || (Type == FMT_DRMPRIME);
    }

    static bool HardwareFramesFormat(VideoFrameType Type)
    {
        return (Type == FMT_VDPAU) || (Type == FMT_VAAPI) || (Type == FMT_NVDEC);
    }

    static bool FormatIs420(VideoFrameType Type)
    {
        return (Type == FMT_YV12) || (Type == FMT_YUV420P9) || (Type == FMT_YUV420P10) ||
               (Type == FMT_YUV420P12) || (Type == FMT_YUV420P14) || (Type == FMT_YUV420P16);
    }

    static bool FormatIs422(VideoFrameType Type)
    {
        return (Type == FMT_YUV422P)   || (Type == FMT_YUV422P9) || (Type == FMT_YUV422P10) ||
               (Type == FMT_YUV422P12) || (Type == FMT_YUV422P14) || (Type == FMT_YUV422P16);
    }

    static bool FormatIs444(VideoFrameType Type)
    {
        return (Type == FMT_YUV444P)   || (Type == FMT_YUV444P9) || (Type == FMT_YUV444P10) ||
               (Type == FMT_YUV444P12) || (Type == FMT_YUV444P14) || (Type == FMT_YUV444P16);
    }

    static bool FormatIsNV12(VideoFrameType Type)
    {
        return (Type == FMT_NV12) || (Type == FMT_P010) || (Type == FMT_P016);
    }

    static bool PackedFormat(VideoFrameType Type)
    {
        return Type == FMT_YUY2;
    }

    static bool YUVFormat(VideoFrameType Type)
    {
        return FormatIs420(Type)  || FormatIs422(Type) || FormatIs444(Type) ||
               FormatIsNV12(Type) || PackedFormat(Type);
    }

    static bool FormatIsRGB(VideoFrameType Type)
    {
        return (FMT_RGB24  == Type) || (FMT_BGRA   == Type) || (FMT_RGB32 == Type) ||
               (FMT_ARGB32 == Type) || (FMT_RGBA32 == Type);
    }

  private:
    static MythDeintType GetDeinterlacer(MythDeintType Option);
};

#endif
