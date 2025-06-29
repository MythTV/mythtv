// MythTV
#include "libmythbase/mythlogging.h"
#include "mythframe.h"
#include "mythvideoprofile.h"

// FFmpeg - for av_malloc/av_free
extern "C" {
#include "libavcodec/avcodec.h"
}

#define LOC QString("VideoFrame: ")

/*! \class MythVideoFrame
 *
 * \var m_frameCounter Raw frame counter/ticker for discontinuity checks in the player.
 * \var m_newGOP       Signals to the player that the scan can be unlocked.
 * \var m_colorshifted Hardware decoded 10/12bit frames are already bit-shifted.
 * \var m_alreadyDeinterlaced The frame has already been deinterlaced (either in the
 * decoder or on the first pass of multi-pass display).
 *
*/
MythVideoFrame::~MythVideoFrame()
{
    if (m_buffer && HardwareFormat(m_type))
        LOG(VB_GENERAL, LOG_ERR, LOC + "Frame still contains a hardware buffer!");
    else if (m_buffer)
        av_freep(reinterpret_cast<void*>(&m_buffer));
}

MythVideoFrame::MythVideoFrame(VideoFrameType Type, int Width, int Height, const VideoFrameTypes* RenderFormats)
{
    Init(Type, Width, Height, (RenderFormats == nullptr) ? &kDefaultRenderFormats : RenderFormats);
}

MythVideoFrame::MythVideoFrame(VideoFrameType Type, uint8_t* Buffer, size_t BufferSize,
                               int Width, int Height, const VideoFrameTypes* RenderFormats, int Alignment)
{
    const VideoFrameTypes* formats = (RenderFormats == nullptr) ? &kDefaultRenderFormats : RenderFormats;
    Init(Type, Buffer, BufferSize, Width, Height, formats, Alignment);
}

void MythVideoFrame::Init(VideoFrameType Type, int Width, int Height, const VideoFrameTypes* RenderFormats)
{
    size_t   newsize   = 0;
    uint8_t* newbuffer = nullptr;
    if ((Width > 0 && Height > 0) && (Type != FMT_NONE) && !HardwareFormat(Type))
    {
        newsize = GetBufferSize(Type, Width, Height);
        bool reallocate = (Width != m_width) || (Height != m_height) || (newsize != m_bufferSize) || (Type != m_type);
        newbuffer = reallocate ? GetAlignedBuffer(newsize) : m_buffer;
        newsize   = reallocate ? newsize : m_bufferSize;
    }
    Init(Type, newbuffer, newsize, Width, Height, (RenderFormats == nullptr) ? &kDefaultRenderFormats : RenderFormats);
}

void MythVideoFrame::Init(VideoFrameType Type, uint8_t *Buffer, size_t BufferSize,
                          int Width, int Height, const VideoFrameTypes* RenderFormats, int Alignment)
{
    if (HardwareFormat(m_type) && m_buffer)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Trying to reinitialise a hardware frame. Ignoring");
        return;
    }

    if (std::any_of(m_priv.cbegin(), m_priv.cend(), [](const uint8_t* P) { return P != nullptr; }))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Priv buffers are set (hardware frame?). Ignoring Init");
        return;
    }

    if ((Buffer && !BufferSize) || (!Buffer && BufferSize))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Inconsistent frame buffer data");
        return;
    }

    if (m_buffer && (m_buffer != Buffer))
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC + "Deleting old frame buffer");
        av_freep(reinterpret_cast<void*>(&m_buffer));
    }

    m_type         = Type;
    m_bitsPerPixel = BitsPerPixel(m_type);
    m_buffer       = Buffer;
    m_bufferSize   = BufferSize;
    m_width        = Width;
    m_height       = Height;
    m_renderFormats = (RenderFormats == nullptr) ? &kDefaultRenderFormats : RenderFormats;

    ClearMetadata();

    if ((m_type == FMT_NONE) || HardwareFormat(m_type) || !m_buffer || !m_bufferSize)
        return;

    int alignedwidth = Alignment > 0 ? (m_width + Alignment - 1) & ~(Alignment - 1) : m_width;
    int alignedheight = (m_height + MYTH_HEIGHT_ALIGNMENT - 1) & ~(MYTH_HEIGHT_ALIGNMENT -1);

    for (uint i = 0; i < 3; ++i)
        m_pitches[i] = GetPitchForPlane(m_type, alignedwidth, i);

    m_offsets[0] = 0;
    if (FMT_YV12 == m_type)
    {
        m_offsets[1] = alignedwidth * alignedheight;
        m_offsets[2] = m_offsets[1] + (((alignedwidth + 1) >> 1) * ((alignedheight+1) >> 1));
    }
    else if (FormatIs420(m_type))
    {
        m_offsets[1] = (alignedwidth << 1) * alignedheight;
        m_offsets[2] = m_offsets[1] + (alignedwidth * (alignedheight >> 1));
    }
    else if (FMT_YUV422P == m_type)
    {
        m_offsets[1] = alignedwidth * alignedheight;
        m_offsets[2] = m_offsets[1] + (((alignedwidth + 1) >> 1) * alignedheight);
    }
    else if (FormatIs422(m_type))
    {
        m_offsets[1] = (alignedwidth << 1) * alignedheight;
        m_offsets[2] = m_offsets[1] + (alignedwidth * alignedheight);
    }
    else if (FMT_YUV444P == m_type)
    {
        m_offsets[1] = alignedwidth * alignedheight;
        m_offsets[2] = m_offsets[1] + (alignedwidth * alignedheight);
    }
    else if (FormatIs444(m_type))
    {
        m_offsets[1] = (alignedwidth << 1) * alignedheight;
        m_offsets[2] = m_offsets[1] + ((alignedwidth << 1) * alignedheight);
    }
    else if (FMT_NV12 == m_type)
    {
        m_offsets[1] = alignedwidth * alignedheight;
        m_offsets[2] = 0;
    }
    else if (FormatIsNV12(m_type))
    {
        m_offsets[1] = (alignedwidth << 1) * alignedheight;
        m_offsets[2] = 0;
    }
    else
    {
        m_offsets[1] = m_offsets[2] = 0;
    }
}

void MythVideoFrame::ClearMetadata()
{
    m_aspect              = -1.0F;
    m_frameRate           = -1.0 ;
    m_frameNumber         = 0;
    m_frameCounter        = 0;
    m_timecode            = 0ms;
    m_displayTimecode     = 0ms;
    m_priv                = { nullptr };
    m_interlaced          = false;
    m_topFieldFirst       = true;
    m_interlacedReverse   = false;
    m_newGOP              = false;
    m_repeatPic           = false;
    m_forceKey            = false;
    m_dummy               = false;
    m_pauseFrame          = false;
    m_pitches             = { 0 };
    m_offsets             = { 0 };
    m_pixFmt              = 0;
    m_swPixFmt            = 0;
    m_directRendering     = true;
    m_colorspace          = 1;
    m_colorrange          = 1;
    m_colorprimaries      = 1;
    m_colortransfer       = 1;
    m_chromalocation      = 1;
    m_colorshifted        = false;
    m_alreadyDeinterlaced = false;
    m_rotation            = 0;
    m_stereo3D            = 0;
    m_hdrMetadata         = nullptr;
    m_deinterlaceSingle   = DEINT_NONE;
    m_deinterlaceDouble   = DEINT_NONE;
    m_deinterlaceAllowed  = DEINT_NONE;
    m_deinterlaceInuse    = DEINT_NONE;
    m_deinterlaceInuse2x  = false;
}

void MythVideoFrame::CopyPlane(uint8_t *To, int ToPitch, const uint8_t *From, int FromPitch,
                               int PlaneWidth, int PlaneHeight)
{
    if ((ToPitch == PlaneWidth) && (FromPitch == PlaneWidth))
    {
        memcpy(To, From, static_cast<size_t>(PlaneWidth) * PlaneHeight);
        return;
    }

    for (int y = 0; y < PlaneHeight; y++)
    {
        memcpy(To, From, static_cast<size_t>(PlaneWidth));
        From += FromPitch;
        To += ToPitch;
    }
}

void MythVideoFrame::ClearBufferToBlank()
{
    if (!m_buffer)
        return;

    if (HardwareFormat(m_type))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Cannot clear a hardware frame");
        return;
    }

    // luma (or RGBA)
    int uv_height = GetHeightForPlane(m_type, m_height, 1);
    int uv = (1 << (ColorDepth(m_type) - 1)) - 1;
    if (FMT_YV12 == m_type || FMT_YUV422P == m_type || FMT_YUV444P == m_type)
    {
        memset(m_buffer + m_offsets[0], 0, static_cast<size_t>(m_pitches[0]) * m_height);
        memset(m_buffer + m_offsets[1], uv & 0xff, static_cast<size_t>(m_pitches[1]) * uv_height);
        memset(m_buffer + m_offsets[2], uv & 0xff, static_cast<size_t>(m_pitches[2]) * uv_height);
    }
    else if ((FormatIs420(m_type) || FormatIs422(m_type) || FormatIs444(m_type)) &&
             (m_pitches[1] == m_pitches[2]))
    {
        memset(m_buffer + m_offsets[0], 0, static_cast<size_t>(m_pitches[0]) * m_height);
        unsigned char uv1 = (uv & 0xff00) >> 8;
        unsigned char uv2 = (uv & 0x00ff);
        unsigned char* buf1 = m_buffer + m_offsets[1];
        unsigned char* buf2 = m_buffer + m_offsets[2];
        for (int row = 0; row < uv_height; ++row)
        {
            for (int col = 0; col < m_pitches[1]; col += 2)
            {
                buf1[col]     = buf2[col]     = uv1;
                buf1[col + 1] = buf2[col + 1] = uv2;
            }
            buf1 += m_pitches[1];
            buf2 += m_pitches[2];
        }
    }
    else if (FMT_NV12 == m_type)
    {
        memset(m_buffer + m_offsets[0], 0, static_cast<size_t>(m_pitches[0]) * m_height);
        memset(m_buffer + m_offsets[1], uv & 0xff, static_cast<size_t>(m_pitches[1]) * uv_height);
    }
    else if (FormatIsNV12(m_type))
    {
        memset(m_buffer + m_offsets[0], 0, static_cast<size_t>(m_pitches[0]) * m_height);
        unsigned char uv1 = (uv & 0xff00) >> 8;
        unsigned char uv2 = (uv & 0x00ff);
        unsigned char* buf3 = m_buffer + m_offsets[1];
        for (int row = 0; row < uv_height; ++row)
        {
            for (int col = 0; col < m_pitches[1]; col += 4)
            {
                buf3[col]     = buf3[col + 2] = uv1;
                buf3[col + 1] = buf3[col + 3] = uv2;
            }
            buf3 += m_pitches[1];
        }
    }
    else if (PackedFormat(m_type))
    {
        // TODO
    }
    else
    {
        memset(m_buffer, 0, m_bufferSize);
    }
}

bool MythVideoFrame::CopyFrame(MythVideoFrame *From)
{
    // Sanity checks
    if (!From || (this == From))
        return false;

    if (m_type != From->m_type)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot copy frames of differing types");
        return false;
    }

    if (From->m_type == FMT_NONE || HardwareFormat(From->m_type))
    {
        LOG(VB_GENERAL, LOG_ERR, "Invalid frame format");
        return false;
    }

    if ((m_width <= 0) || (m_height <= 0) ||
          (m_width != From->m_width) || (m_height != From->m_height))
    {
        LOG(VB_GENERAL, LOG_ERR, "Invalid frame sizes");
        return false;
    }

    if (!m_buffer || !From->m_buffer || (m_buffer == From->m_buffer))
    {
        LOG(VB_GENERAL, LOG_ERR, "Invalid frames for copying");
        return false;
    }

    // N.B. Minimum based on zero width alignment but will apply height alignment
    size_t minsize = GetBufferSize(m_type, m_width, m_height, 0);
    if ((m_bufferSize < minsize) || (From->m_bufferSize < minsize))
    {
        LOG(VB_GENERAL, LOG_ERR, "Invalid buffer size");
        return false;
    }

    // We have 2 frames of the same valid size and format, they are not hardware frames
    // and both have buffers reported to satisfy a minimal size.

    // Copy data
    uint count = GetNumPlanes(From->m_type);
    for (uint plane = 0; plane < count; plane++)
    {
        CopyPlane(m_buffer + m_offsets[plane], m_pitches[plane],
                  From->m_buffer + From->m_offsets[plane], From->m_pitches[plane],
                  GetPitchForPlane(From->m_type, From->m_width, plane),
                  GetHeightForPlane(From->m_type, From->m_height, plane));
    }

    // Copy metadata
    // Not copied: codec, width, height - should already be the same
    // Not copied: buf, size, pitches, offsets - should/could be different
    // Not copied: priv - hardware frames only
    m_aspect              = From->m_aspect;
    m_frameRate           = From->m_frameRate;
    m_bitsPerPixel        = From->m_bitsPerPixel;
    m_frameNumber         = From->m_frameNumber;
    m_frameCounter        = From->m_frameCounter;
    m_timecode            = From->m_timecode;
    m_displayTimecode     = From->m_displayTimecode;
    m_interlaced          = From->m_interlaced;
    m_topFieldFirst       = From->m_topFieldFirst;
    m_interlacedReverse   = From->m_interlacedReverse;
    m_newGOP              = From->m_newGOP;
    m_repeatPic           = From->m_repeatPic;
    m_forceKey            = From->m_forceKey;
    m_dummy               = From->m_dummy;
    m_pauseFrame          = From->m_pauseFrame;
    m_pixFmt              = From->m_pixFmt;
    m_swPixFmt            = From->m_swPixFmt;
    m_directRendering     = From->m_directRendering;
    m_colorspace          = From->m_colorspace;
    m_colorrange          = From->m_colorrange;
    m_colorprimaries      = From->m_colorprimaries;
    m_colortransfer       = From->m_colortransfer;
    m_chromalocation      = From->m_chromalocation;
    m_colorshifted        = From->m_colorshifted;
    m_alreadyDeinterlaced = From->m_alreadyDeinterlaced;
    m_rotation            = From->m_rotation;
    m_stereo3D            = From->m_stereo3D;
    m_hdrMetadata         = From->m_hdrMetadata;
    m_deinterlaceSingle   = From->m_deinterlaceSingle;
    m_deinterlaceDouble   = From->m_deinterlaceDouble;
    m_deinterlaceAllowed  = From->m_deinterlaceAllowed;
    m_deinterlaceInuse    = From->m_deinterlaceInuse;
    m_deinterlaceInuse2x  = From->m_deinterlaceInuse2x;

    return true;
}

QString MythVideoFrame::FormatDescription(VideoFrameType Type)
{
    switch (Type)
    {
        case FMT_NONE:       return "None";
        case FMT_RGB24:      return "RGB24";
        case FMT_YV12:       return "YUV420P";
        case FMT_RGB32:      return "RGB32";
        case FMT_ARGB32:     return "ARGB32";
        case FMT_RGBA32:     return "RGBA32";
        case FMT_YUV422P:    return "YUV422P";
        case FMT_BGRA:       return "BGRA";
        case FMT_YUY2:       return "YUY2";
        case FMT_NV12:       return "NV12";
        case FMT_P010:       return "P010";
        case FMT_P016:       return "P016";
        case FMT_YUV420P9:   return "YUV420P9";
        case FMT_YUV420P10:  return "YUV420P10";
        case FMT_YUV420P12:  return "YUV420P12";
        case FMT_YUV420P14:  return "YUV420P14";
        case FMT_YUV420P16:  return "YUV420P16";
        case FMT_YUV422P9:   return "YUV422P9";
        case FMT_YUV422P10:  return "YUV422P10";
        case FMT_YUV422P12:  return "YUV422P12";
        case FMT_YUV422P14:  return "YUV422P14";
        case FMT_YUV422P16:  return "YUV422P16";
        case FMT_YUV444P:    return "YUV444P";
        case FMT_YUV444P9:   return "YUV444P9";
        case FMT_YUV444P10:  return "YUV444P10";
        case FMT_YUV444P12:  return "YUV444P12";
        case FMT_YUV444P14:  return "YUV444P14";
        case FMT_YUV444P16:  return "YUV444P16";
        case FMT_VDPAU:      return "VDPAU";
        case FMT_VAAPI:      return "VAAPI";
        case FMT_DXVA2:      return "DXVA2";
        case FMT_MMAL:       return "MMAL";
        case FMT_MEDIACODEC: return "MediaCodec";
        case FMT_VTB:        return "VideoToolBox";
        case FMT_NVDEC:      return "NVDec";
        case FMT_DRMPRIME:   return "DRM-PRIME";
    }
    return "?";
}

size_t MythVideoFrame::GetBufferSize(VideoFrameType Type, int Width, int Height, int Aligned)
{
    // bits per pixel div common factor
    int bpp = BitsPerPixel(Type) / 4;
    // bits per byte div common factor
    int bpb =  8 / 4;

    // Align height and width. We always align height to 16 rows and the *default*
    // width alignment is 64bytes, which allows SIMD operations on subsampled
    // planes (i.e. 32byte alignment)
    int adj_w = Aligned ? ((Width  + Aligned - 1) & ~(Aligned - 1)) : Width;
    int adj_h = (Height + MYTH_HEIGHT_ALIGNMENT - 1) & ~(MYTH_HEIGHT_ALIGNMENT - 1);

    // Calculate rounding as necessary.
    int remainder = (adj_w * adj_h * bpp) % bpb;
    return static_cast<uint>(((adj_w * adj_h * bpp) / bpb) + (remainder ? 1 : 0));
}

uint8_t *MythVideoFrame::GetAlignedBuffer(size_t Size)
{
    return static_cast<uint8_t*>(av_malloc(Size + 64));
}

uint8_t *MythVideoFrame::CreateBuffer(VideoFrameType Type, int Width, int Height)
{
    size_t size = GetBufferSize(Type, Width, Height);
    return GetAlignedBuffer(size);
}

MythDeintType MythVideoFrame::GetSingleRateOption(MythDeintType Type, MythDeintType Override) const
{
    MythDeintType options = m_deinterlaceSingle & (Override ? Override : m_deinterlaceAllowed);
    if (options & Type)
        return GetDeinterlacer(options);
    return DEINT_NONE;
}

MythDeintType MythVideoFrame::GetDoubleRateOption(MythDeintType Type, MythDeintType Override) const
{
    MythDeintType options = m_deinterlaceDouble & (Override ? Override : m_deinterlaceAllowed);
    if (options & Type)
        return GetDeinterlacer(options);
    return DEINT_NONE;
}

MythDeintType MythVideoFrame::GetDeinterlacer(MythDeintType Option)
{
    return Option & (DEINT_BASIC | DEINT_MEDIUM | DEINT_HIGH);
}

QString MythVideoFrame::DeinterlacerName(MythDeintType Deint, bool DoubleRate, VideoFrameType Format)
{
    MythDeintType deint = GetDeinterlacer(Deint);
    QString result = DoubleRate ? "2x " : "";
    if (Deint & DEINT_CPU)
    {
        result += "CPU ";
        switch (deint)
        {
            case DEINT_HIGH:   return result + "Yadif";
            case DEINT_MEDIUM: return result + "Linearblend";
            case DEINT_BASIC:  return result + "Onefield";
            default: break;
        }
    }
    else if (Deint & DEINT_SHADER)
    {
        result += "GLSL ";
        switch (deint)
        {
            case DEINT_HIGH:   return result + "Kernel";
            case DEINT_MEDIUM: return result + "Linearblend";
            case DEINT_BASIC:  return result + "Onefield";
            default: break;
        }
    }
    else if (Deint & DEINT_DRIVER)
    {
        switch (Format)
        {
            case FMT_MEDIACODEC: return "MediaCodec";
            case FMT_DRMPRIME: return result + "EGL Onefield";
            case FMT_VDPAU:
                result += "VDPAU ";
                switch (deint)
                {
                    case DEINT_HIGH:   return result + "Advanced";
                    case DEINT_MEDIUM: return result + "Temporal";
                    case DEINT_BASIC:  return result + "Basic";
                    default: break;
                }
                break;
            case FMT_NVDEC:
                result += "NVDec ";
                switch (deint)
                {
                    case DEINT_HIGH:
                    case DEINT_MEDIUM: return result + "Adaptive";
                    case DEINT_BASIC:  return result + "Basic";
                    default: break;
                }
                break;
            case FMT_VAAPI:
                result += "VAAPI ";
                switch (deint)
                {
                    case DEINT_HIGH:   return result + "Compensated";
                    case DEINT_MEDIUM: return result + "Adaptive";
                    case DEINT_BASIC:  return result + "Basic";
                    default: break;
                }
                break;
            default: break;
        }
    }
    return "None";
}

QString MythVideoFrame::DeinterlacerPref(MythDeintType Deint)
{
    if (DEINT_NONE == Deint)
        return {"None"};
    QString result;
    if (Deint & DEINT_BASIC)       result = "Basic";
    else if (Deint & DEINT_MEDIUM) result = "Medium";
    else if (Deint & DEINT_HIGH)   result = "High";
    if (Deint & DEINT_CPU)         result += "|CPU";
    if (Deint & DEINT_SHADER)      result += "|GLSL";
    if (Deint & DEINT_DRIVER)      result += "|DRIVER";
    return result;
}

MythDeintType MythVideoFrame::ParseDeinterlacer(const QString& Deinterlacer)
{
    MythDeintType result = DEINT_NONE;

    if (Deinterlacer.contains(DEINT_QUALITY_HIGH))
        result = DEINT_HIGH;
    else if (Deinterlacer.contains(DEINT_QUALITY_MEDIUM))
        result = DEINT_MEDIUM;
    else if (Deinterlacer.contains(DEINT_QUALITY_LOW))
        result = DEINT_BASIC;

    if (result != DEINT_NONE)
    {
        result = result | DEINT_CPU; // NB always assumed
        if (Deinterlacer.contains(DEINT_QUALITY_SHADER))
            result = result | DEINT_SHADER;
        if (Deinterlacer.contains(DEINT_QUALITY_DRIVER))
            result = result | DEINT_DRIVER;
    }

    return result;
}
