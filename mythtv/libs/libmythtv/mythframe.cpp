// MythTV
#include "mythlogging.h"
#include "mythframe.h"

MythDeintType GetSingleRateOption(const MythVideoFrame* Frame, MythDeintType Type,
                                  MythDeintType Override)
{
    if (Frame)
    {
        MythDeintType options = Frame->deinterlace_single &
                (Override ? Override : Frame->deinterlace_allowed);
        if (options & Type)
            return GetDeinterlacer(options);
    }
    return DEINT_NONE;
}

MythDeintType GetDoubleRateOption(const MythVideoFrame* Frame, MythDeintType Type,
                                  MythDeintType Override)
{
    if (Frame)
    {
        MythDeintType options = Frame->deinterlace_double &
                (Override ? Override : Frame->deinterlace_allowed);
        if (options & Type)
            return GetDeinterlacer(options);
    }
    return DEINT_NONE;
}

MythDeintType GetDeinterlacer(MythDeintType Option)
{
    return Option & (DEINT_BASIC | DEINT_MEDIUM | DEINT_HIGH);
}

void MythVideoFrame::CopyPlane(uint8_t *To, int ToPitch, const uint8_t *From, int FromPitch,
                               int PlaneWidth, int PlaneHeight)
{
    if ((ToPitch == PlaneWidth) && (FromPitch == PlaneWidth))
    {
        memcpy(To, From, static_cast<size_t>(PlaneWidth * PlaneHeight));
        return;
    }

    for (int y = 0; y < PlaneHeight; y++)
    {
        memcpy(To, From, static_cast<size_t>(PlaneWidth));
        From += FromPitch;
        To += ToPitch;
    }
}

void MythVideoFrame::Clear()
{
    if (!buf)
        return;

    // luma (or RGBA)
    int uv_height = GetHeightForPlane(codec, height, 1);
    int uv = (1 << (ColorDepth(codec) - 1)) - 1;
    if (FMT_YV12 == codec || FMT_YUV422P == codec || FMT_YUV444P == codec)
    {
        memset(buf + offsets[0], 0, static_cast<size_t>(pitches[0] * height));
        memset(buf + offsets[1], uv & 0xff, static_cast<size_t>(pitches[1] * uv_height));
        memset(buf + offsets[2], uv & 0xff, static_cast<size_t>(pitches[2] * uv_height));
    }
    else if ((FormatIs420(codec) || FormatIs422(codec) || FormatIs444(codec)) &&
             (pitches[1] == pitches[2]))
    {
        memset(buf + offsets[0], 0, static_cast<size_t>(pitches[0] * height));
        unsigned char uv1 = (uv & 0xff00) >> 8;
        unsigned char uv2 = (uv & 0x00ff);
        unsigned char* buf1 = buf + offsets[1];
        unsigned char* buf2 = buf + offsets[2];
        for (int row = 0; row < uv_height; ++row)
        {
            for (int col = 0; col < pitches[1]; col += 2)
            {
                buf1[col]     = buf2[col]     = uv1;
                buf1[col + 1] = buf2[col + 1] = uv2;
            }
            buf1 += pitches[1];
            buf2 += pitches[2];
        }
    }
    else if (FMT_NV12 == codec)
    {
        memset(buf + offsets[0], 0, static_cast<size_t>(pitches[0] * height));
        memset(buf + offsets[1], uv & 0xff, static_cast<size_t>(pitches[1] * uv_height));
    }
    else if (FormatIsNV12(codec))
    {
        memset(buf + offsets[0], 0, static_cast<size_t>(pitches[0] * height));
        unsigned char uv1 = (uv & 0xff00) >> 8;
        unsigned char uv2 = (uv & 0x00ff);
        unsigned char* buf3 = buf + offsets[1];
        for (int row = 0; row < uv_height; ++row)
        {
            for (int col = 0; col < pitches[1]; col += 4)
            {
                buf3[col]     = buf3[col + 2] = uv1;
                buf3[col + 1] = buf3[col + 3] = uv2;
            }
            buf3 += pitches[1];
        }
    }
}

bool MythVideoFrame::CopyFrame(MythVideoFrame *To, MythVideoFrame *From)
{
    // Sanity checks
    if (!(To && From) || (To == From))
        return false;

    if (To->codec != From->codec)
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot copy frames of differing types");
        return false;
    }

    if (From->codec == FMT_NONE || HardwareFormat(From->codec))
    {
        LOG(VB_GENERAL, LOG_ERR, "Invalid frame format");
        return false;
    }

    if (!((To->width > 0) && (To->height > 0) &&
          (To->width == From->width) && (To->height == From->height)))
    {
        LOG(VB_GENERAL, LOG_ERR, "Invalid frame sizes");
        return false;
    }

    if (!(To->buf && From->buf && (To->buf != From->buf)))
    {
        LOG(VB_GENERAL, LOG_ERR, "Invalid frames for copying");
        return false;
    }

    // N.B. Minimum based on zero width alignment but will apply height alignment
    int minsize = static_cast<int>(GetBufferSize(To->codec, To->width, To->height, 0));
    if ((To->size < minsize) || (From->size < minsize))
    {
        LOG(VB_GENERAL, LOG_ERR, "Invalid buffer size");
        return false;
    }

    // We have 2 frames of the same valid size and format, they are not hardware frames
    // and both have buffers reported to satisfy a minimal size.

    // Copy data
    uint count = GetNumPlanes(From->codec);
    for (uint plane = 0; plane < count; plane++)
    {
        CopyPlane(To->buf + To->offsets[plane], To->pitches[plane],
                  From->buf + From->offsets[plane], From->pitches[plane],
                  GetPitchForPlane(From->codec, From->width, plane),
                  GetHeightForPlane(From->codec, From->height, plane));
    }

    // Copy metadata
    // Not copied: codec, width, height - should already be the same
    // Not copied: buf, size, pitches, offsets - should/could be different
    // Not copied: priv - hardware frames only
    To->aspect               = From->aspect;
    To->frame_rate           = From->frame_rate;
    To->bpp                  = From->bpp;
    To->frameNumber          = From->frameNumber;
    To->frameCounter         = From->frameCounter;
    To->timecode             = From->timecode;
    To->disp_timecode        = From->disp_timecode;
    To->interlaced_frame     = From->interlaced_frame;
    To->top_field_first      = From->top_field_first;
    To->interlaced_reversed  = From->interlaced_reversed;
    To->new_gop              = From->new_gop;
    To->repeat_pict          = From->repeat_pict;
    To->forcekey             = From->forcekey;
    To->dummy                = From->dummy;
    To->pause_frame          = From->pause_frame;
    To->pix_fmt              = From->pix_fmt;
    To->sw_pix_fmt           = From->sw_pix_fmt;
    To->directrendering      = From->directrendering;
    To->colorspace           = From->colorspace;
    To->colorrange           = From->colorrange;
    To->colorprimaries       = From->colorprimaries;
    To->colortransfer        = From->colortransfer;
    To->chromalocation       = From->chromalocation;
    To->colorshifted         = From->colorshifted;
    To->already_deinterlaced = From->already_deinterlaced;
    To->rotation             = From->rotation;
    To->deinterlace_single   = From->deinterlace_single;
    To->deinterlace_double   = From->deinterlace_double;
    To->deinterlace_allowed  = From->deinterlace_allowed;
    To->deinterlace_inuse    = From->deinterlace_inuse;
    To->deinterlace_inuse2x  = From->deinterlace_inuse2x;

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
    return static_cast<uint>((adj_w * adj_h * bpp) / bpb + (remainder ? 1 : 0));
}

uint8_t *MythVideoFrame::GetAlignedBuffer(size_t Size)
{
    return static_cast<uint8_t*>(av_malloc(Size + 64));
}

uint8_t* MythVideoFrame::GetAlignedBufferZero(size_t Size)
{
    return static_cast<uint8_t*>(av_mallocz(Size + 64));
}

uint8_t *MythVideoFrame::CreateBuffer(VideoFrameType Type, int Width, int Height)
{
    size_t size = GetBufferSize(Type, Width, Height);
    return GetAlignedBuffer(size);
}
