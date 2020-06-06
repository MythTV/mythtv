// MythTV
#include "config.h"
#include "mythlogging.h"
#include "mythavutil.h"
#include "mythdeinterlacer.h"

extern "C" {
#include "libavfilter/buffersrc.h"
#include "libavfilter/buffersink.h"
}

#if (HAVE_SSE2 && ARCH_X86_64)
#include "libavutil/x86/cpu.h"
#include <emmintrin.h>
bool MythDeinterlacer::s_haveSIMD = av_get_cpu_flags() & AV_CPU_FLAG_SSE2;
#elif HAVE_INTRINSICS_NEON
#if ARCH_AARCH64
#include "libavutil/aarch64/cpu.h"
#elif ARCH_ARM
#include "libavutil/arm/cpu.h"
#endif
#include <arm_neon.h>
bool MythDeinterlacer::s_haveSIMD = have_neon(av_get_cpu_flags());
#else
bool MythDeinterlacer::s_haveSIMD = false;
#endif

#define LOC QString("MythDeint: ")

/*! \class MythDeinterlacer
 * \brief Handles software based deinterlacing of video frames.
 *
 * Based upon the deinterlacing preferences passed in through the video
 * frame, MythDeinterlacer will deinterlace the given frame using the appropriate
 * quality and using single or double frame rate.
 *
 * The following deinterlacers are used:
 * Basic - onefield/bob using libswcale
 * Medium - linearblend with custom code (SSE2 and Neon assisted where available)
 * High - libavfilter's yadif (with multithreading)
 *
 * \note libavfilter frame doubling filters expect frames to be presented
 * in the correct order and will break if they do not receive a frame followed
 * by the retrieval of 2 'fields'.
 * \note There is no support for deinterlacig NV12 frame formats in libavilter
*/
MythDeinterlacer::~MythDeinterlacer()
{
    Cleanup();
}

/*! \brief Deinterlace Frame if needed
 *
 * Software deinterlacing is the choice of last resort i.e. shader or driver
 * based deinterlacers are always preferred if available.
 *
 * If using double rate deinterlacing, the frame must be presented in the correct
 * order i.e. kScan_Interlaced followed by kScan_Intr2ndField.
 *
 * The appropriate field to deinterlace is determined by the scan type and the flags
 * for interlaced_reverse and top_field_first in VideoFrame.
 *
 * libavilter's must be recreated if any parameter is changed as there is no method
 * to select the field 'on the fly'.
 *
 * \param Force Set to true to ensure a deinterlaced frame is always returned.
 * Used for preview images.
*/
void MythDeinterlacer::Filter(VideoFrame *Frame, FrameScanType Scan,
                              VideoDisplayProfile *Profile, bool Force)
{
    // nothing to see here

    if (!Frame || !is_interlaced(Scan))
    {
        Cleanup();
        return;
    }

    if (Frame->already_deinterlaced)
        return;

    // Sanity check frame format
    if (!format_is_yuv(Frame->codec))
    {
        Cleanup();
        return;
    }

    // check for software deinterlacing
    bool doublerate = true;
    bool topfieldfirst = Frame->interlaced_reversed ? !Frame->top_field_first : Frame->top_field_first;

    MythDeintType deinterlacer = GetDoubleRateOption(Frame, DEINT_CPU);
    MythDeintType other        = GetDoubleRateOption(Frame, DEINT_SHADER);
    if (other)
    {
        Cleanup();
        return;
    }

    if (!deinterlacer)
    {
        doublerate   = false;
        deinterlacer = GetSingleRateOption(Frame, DEINT_CPU);
        other        = GetSingleRateOption(Frame, DEINT_SHADER);
        if (!deinterlacer || other)
        {
            Cleanup();
            return;
        }
    }

    // libavfilter will not deinterlace NV12 frames. Allow shaders in this case.
    // libswscale (for bob/onefield) is fine, as is our linearblend.
    if ((deinterlacer == DEINT_HIGH) && format_is_nv12(Frame->codec))
    {
        Cleanup();
        Frame->deinterlace_single = Frame->deinterlace_single | DEINT_SHADER;
        Frame->deinterlace_double = Frame->deinterlace_double | DEINT_SHADER;
        return;
    }

    // certain material (telecined?) continually changes the field order. This
    // cripples performance as the libavfiler deinterlacer is continually
    // destroyed and recreated. Using 'auto' for the field order breaks user
    // override of the interlacing order - so track switches in the field order
    // and switch to auto if it is too frequent
    bool fieldorderchanged = topfieldfirst != m_topFirst;
    if (fieldorderchanged && deinterlacer != DEINT_HIGH)
    {
        fieldorderchanged = false;
        m_topFirst = topfieldfirst;
    }

    bool otherchanged = Frame->width != m_width     || Frame->height  != m_height ||
                        deinterlacer != m_deintType || doublerate     != m_doubleRate ||
                        Frame->codec != m_inputType;

    if ((deinterlacer == DEINT_HIGH) && fieldorderchanged)
    {
        bool alreadyauto = m_autoFieldOrder;
        bool change = m_lastFieldChange && (qAbs(m_lastFieldChange - Frame->frameCounter) < 10);
        if (change && !m_autoFieldOrder)
        {
            m_autoFieldOrder = true;
            LOG(VB_PLAYBACK, LOG_INFO, LOC + "Enabled 'auto' for field order");
        }
        else if (!change && m_autoFieldOrder)
        {
            m_autoFieldOrder = false;
            LOG(VB_PLAYBACK, LOG_INFO, LOC + "Disabled 'auto' for field order");
        }
        if (alreadyauto && m_autoFieldOrder)
            fieldorderchanged = false;
        m_lastFieldChange = Frame->frameCounter;
    }

    // Check for a change in input or deinterlacer
    if (fieldorderchanged || otherchanged)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Deinterlacer change: %1x%2 %3 dr:%4 tff:%5 -> %6x%7 %8 dr:%9 tff:%10")
            .arg(m_width).arg(m_height).arg(format_description(m_inputType))
            .arg(m_doubleRate).arg(m_topFirst)
            .arg(Frame->width).arg(Frame->height).arg(format_description(Frame->codec))
            .arg(doublerate).arg(topfieldfirst));
        if (!Initialise(Frame, deinterlacer, doublerate, topfieldfirst, Profile))
        {
            Cleanup();
            return;
        }
        Force = true;
    }
    else if ((m_deintType == DEINT_HIGH) && (abs(Frame->frameCounter - m_discontinuityCounter) > 1))
    {
        if (!Initialise(Frame, deinterlacer, doublerate, topfieldfirst, Profile))
        {
            Cleanup();
            return;
        }
        Force = true;
    }

    m_discontinuityCounter = Frame->frameCounter;

    // Set in use deinterlacer for debugging
    Frame->deinterlace_inuse = m_deintType | DEINT_CPU;
    Frame->deinterlace_inuse2x = m_doubleRate;

    // onefield or bob
    if (m_deintType == DEINT_BASIC)
    {
        OneField(Frame, Scan);
        return;
    }

    // linear blend
    if (m_deintType == DEINT_MEDIUM)
    {
        Blend(Frame, Scan);
        return;
    }

    // We need a filter
    if (!m_graph)
        return;

    // Convert VideoFrame to AVFrame - no copy
    if (AVPictureFill(m_frame, Frame, m_inputFmt) < 1)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Error converting frame");
        return;
    }

    m_frame->width  = Frame->width;
    m_frame->height = Frame->height;
    m_frame->format = Frame->pix_fmt;
    m_frame->pts    = Frame->timecode;

    auto AddFrame = [](AVFilterContext* Source, AVFrame *AvFrame)
        { return av_buffersrc_add_frame(Source, AvFrame); };

    // Add frame on first pass only
    if (kScan_Interlaced == Scan)
    {
        if (AddFrame(m_source, m_frame) < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Error adding frame");
            return;
        }
        // yadif needs 2 frames to work with - add the frame again if we
        // need a result now
        if (Force)
            AddFrame(m_source, m_frame);
    }

    // Retrieve frame
    int res = av_buffersink_get_frame(m_sink, m_frame);
    if (res < 0)
    {
        if (res == AVERROR(EAGAIN))
            return;
        LOG(VB_GENERAL, LOG_ERR, LOC + "Error retrieving frame");
        return;
    }

    // Ensure AVFrame is in the expected format
    if ((m_frame->format != m_inputFmt) || (Frame->pitches[0] < m_frame->linesize[0]) ||
        (Frame->pitches[1] < m_frame->linesize[1]) || (Frame->pitches[2] < m_frame->linesize[2]))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Filter returned unexpected format");
        return;
    }

    // Copy AVFrame back to VideoFrame
    uint count = planes(Frame->codec);
    for (uint plane = 0; plane < count; ++plane)
        copyplane(Frame->buf + Frame->offsets[plane], Frame->pitches[plane], m_frame->data[plane], m_frame->linesize[plane],
                  pitch_for_plane(m_inputType, m_frame->width, plane), height_for_plane(m_inputType, m_frame->height, plane));

    Frame->timecode = m_frame->pts;
    Frame->already_deinterlaced = 1;

    // Free frame data
    av_frame_unref(m_frame);
}

void MythDeinterlacer::Cleanup(void)
{
    if (m_graph || m_swsContext)
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Removing CPU deinterlacer");

    avfilter_graph_free(&m_graph);
    sws_freeContext(m_swsContext);
    m_swsContext = nullptr;
    m_discontinuityCounter = 0;
    m_autoFieldOrder = false;
    m_lastFieldChange = 0;

    if (m_bobFrame)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Removing 'bob' cache frame");
        av_free(m_bobFrame->buf);
        delete m_bobFrame;
        m_bobFrame = nullptr;
    }

    m_deintType = DEINT_NONE;
}

///\brief Initialise deinterlacing using the given MythDeintType
bool MythDeinterlacer::Initialise(VideoFrame *Frame, MythDeintType Deinterlacer,
                                  bool DoubleRate, bool TopFieldFirst, VideoDisplayProfile *Profile)
{
    auto autofieldorder = m_autoFieldOrder;
    auto lastfieldchange = m_lastFieldChange;
    Cleanup();
    m_source = nullptr;
    m_sink   = nullptr;

    if (!Frame)
        return false;

    m_width     = Frame->width;
    m_height    = Frame->height;
    m_inputType = Frame->codec;
    m_inputFmt  = FrameTypeToPixelFormat(Frame->codec);
    QString name = DeinterlacerName(Deinterlacer | DEINT_CPU, DoubleRate);

    // simple onefield/bob?
    if (Deinterlacer == DEINT_BASIC || Deinterlacer == DEINT_MEDIUM)
    {
        m_deintType  = Deinterlacer;
        m_doubleRate = DoubleRate;
        m_topFirst   = TopFieldFirst;
        if (Deinterlacer == DEINT_BASIC)
        {
            m_swsContext = sws_getCachedContext(m_swsContext, m_width, m_height >> 1, m_inputFmt,
                                                m_width, m_height, m_inputFmt, SWS_FAST_BILINEAR,
                                                nullptr, nullptr, nullptr);
            if (m_swsContext == nullptr)
                return false;
        }
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Using deinterlacer '%1'").arg(name));
        return true;
    }

    // Sanity check the frame formats
    if (PixelFormatToFrameType(m_inputFmt) != m_inputType)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Inconsistent frame formats");
        return false;
    }

    m_graph = avfilter_graph_alloc();
    if (!m_graph)
        return false;

    uint threads = 1;
    if (Profile)
    {
        threads = Profile->GetMaxCPUs();
        if (threads < 1 || threads > 8)
            threads = 1;
    }

    AVFilterInOut* inputs = nullptr;
    AVFilterInOut* outputs = nullptr;

    QString deint = QString("yadif=mode=%1:parity=%2:threads=%3")
        .arg(DoubleRate ? 1 : 0).arg(m_autoFieldOrder ? -1 : TopFieldFirst ? 0 : 1).arg(threads);

    QString graph = QString("buffer=video_size=%1x%2:pix_fmt=%3:time_base=1/1[in];[in]%4[out];[out] buffersink")
                       .arg(m_width).arg(m_height).arg(m_inputFmt).arg(deint);

    int res = avfilter_graph_parse2(m_graph, graph.toLatin1().constData(), &inputs, &outputs);
    if (res >= 0 && !inputs && !outputs)
    {
        res = avfilter_graph_config(m_graph, nullptr);
        if (res >= 0)
        {
            m_source = avfilter_graph_get_filter(m_graph, "Parsed_buffer_0");
            m_sink   = avfilter_graph_get_filter(m_graph, "Parsed_buffersink_2");

            if (m_source && m_sink)
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Created deinterlacer '%1' (%2 threads)")
                    .arg(name).arg(threads));
                m_deintType  = Deinterlacer;
                m_doubleRate = DoubleRate;
                m_topFirst   = TopFieldFirst;
                m_autoFieldOrder = autofieldorder;
                m_lastFieldChange = lastfieldchange;
                return true;
            }
        }
    }

    LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create avfilter");
    m_deintType = DEINT_NONE;
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    return false;
}

bool MythDeinterlacer::SetUpCache(VideoFrame *Frame)
{
    if (!Frame)
        return false;

    if (!m_bobFrame)
    {
        m_bobFrame = new VideoFrame;
        if (!m_bobFrame)
            return false;
        memset(m_bobFrame, 0, sizeof(VideoFrame));
        LOG(VB_PLAYBACK, LOG_INFO, "Created new 'bob' cache frame");
    }

    // copy Frame metadata, preserving any existing buffer allocation
    unsigned char *buf = m_bobFrame->buf;
    int size = m_bobFrame->size;
    memcpy(m_bobFrame, Frame, sizeof(VideoFrame));
    m_bobFrame->priv[0] = m_bobFrame->priv[1] = m_bobFrame->priv[2] = m_bobFrame->priv[3] = nullptr;
    m_bobFrame->buf = buf;
    m_bobFrame->size = size;

    if (!m_bobFrame->buf || (m_bobFrame->size != Frame->size))
    {
        av_free(m_bobFrame->buf);
        m_bobFrame->buf = static_cast<unsigned char*>(av_malloc(static_cast<size_t>(Frame->size + 64)));
        m_bobFrame->size = Frame->size;
    }

    return m_bobFrame->buf != nullptr;
}

void MythDeinterlacer::OneField(VideoFrame *Frame, FrameScanType Scan)
{
    if (!m_swsContext)
        return;

    // we need a frame for caching - both to preserve the second field if
    // needed and ensure we are not filtering in place (i.e. from src to src).
    if (!SetUpCache(Frame))
        return;

    // copy/cache on first pass
    if (kScan_Interlaced == Scan)
        memcpy(m_bobFrame->buf, Frame->buf, static_cast<size_t>(m_bobFrame->size));

    // Convert VideoFrame to AVFrame - no copy
    AVFrame dstframe;
    if ((AVPictureFill(m_frame, m_bobFrame, m_inputFmt) < 1) ||
        (AVPictureFill(&dstframe, Frame, m_inputFmt) < 1))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Error converting frame");
        return;
    }

    bool topfield   = Scan == kScan_Interlaced ? m_topFirst : !m_topFirst;
    dstframe.width  = Frame->width;
    dstframe.height = Frame->height;
    dstframe.format = Frame->pix_fmt;

    m_frame->width  = m_bobFrame->width;
    m_frame->format = m_bobFrame->pix_fmt;

    // Fake the frame height and stride to simulate a single field
    m_frame->height = Frame->height >> 1;
    m_frame->interlaced_frame = 0;
    uint nbplanes = planes(m_inputType);
    for (uint i = 0; i < nbplanes; i++)
    {
        if (!topfield)
            m_frame->data[i] = m_frame->data[i] + m_frame->linesize[i];
        m_frame->linesize[i] = m_frame->linesize[i] << 1;
    }

    // and scale to full height
    int result = sws_scale(m_swsContext, m_frame->data, m_frame->linesize, 0, m_frame->height,
                           dstframe.data, dstframe.linesize);

    if (result != Frame->height)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Error scaling frame: height %1 expected %2")
            .arg(result).arg(Frame->height));
    }
    Frame->already_deinterlaced = 1;
}

inline static uint32_t avg(uint32_t A, uint32_t B)
{
    return (((A ^ B) & 0xFEFEFEFEUL) >> 1) + (A & B);
}

// Optimised version with 4x4 alignment
static inline void BlendC4x4(unsigned char *Src, int Width, int FirstRow, int LastRow, int Pitch,
                             unsigned char *Dst, int DstPitch, bool Second)
{
    int srcpitch = Pitch << 1;
    int dstpitch = DstPitch << 1;
    int maxrows  = LastRow - 3;

    unsigned char *above   = Src + ((FirstRow - 1) * Pitch);
    unsigned char *dest1   = Dst + (FirstRow * DstPitch);
    unsigned char *middle  = above + srcpitch;
    unsigned char *dest2   = dest1 + dstpitch;
    unsigned char *below   = middle + srcpitch;
    unsigned char *dstcpy1 = Dst + ((FirstRow - 1) * DstPitch);
    unsigned char *dstcpy2 = dstcpy1 + dstpitch;

    srcpitch <<= 1;
    dstpitch <<= 1;

    // 4 rows per pass
    for (int row = FirstRow; row < maxrows; row += 4)
    {
        if (Second)
        {
            // On second pass, copy over the original, current field
            memcpy(dstcpy1, above,  static_cast<size_t>(DstPitch));
            memcpy(dstcpy2, middle, static_cast<size_t>(DstPitch));
            dstcpy1 += dstpitch;
            dstcpy2 += dstpitch;
        }
        for (int col = 0; col < Width; col += 4)
        {
            *reinterpret_cast<uint32_t*>(&dest1[col]) =
                    avg(*reinterpret_cast<uint32_t*>(&above[col]), *reinterpret_cast<uint32_t*>(&middle[col]));
            *reinterpret_cast<uint32_t*>(&dest2[col]) =
                    avg(*reinterpret_cast<uint32_t*>(&middle[col]), *reinterpret_cast<uint32_t*>(&below[col]));
        }
        above  += srcpitch;
        middle += srcpitch;
        below  += srcpitch;
        dest1  += dstpitch;
        dest2  += dstpitch;
    }
}

#if (HAVE_SSE2 && ARCH_X86_64) || HAVE_INTRINSICS_NEON
// SIMD optimised version with 16x4 alignment
static inline void BlendSIMD16x4(unsigned char *Src, int Width, int FirstRow, int LastRow, int Pitch,
                                 unsigned char *Dst, int DstPitch, bool Second)
{
    int srcpitch = Pitch << 1;
    int dstpitch = DstPitch << 1;
    int maxrows  = LastRow - 3;

    unsigned char *above   = Src + ((FirstRow - 1) * Pitch);
    unsigned char *dest1   = Dst + (FirstRow * DstPitch);
    unsigned char *middle  = above + srcpitch;
    unsigned char *dest2   = dest1 + dstpitch;
    unsigned char *below   = middle + srcpitch;
    unsigned char *dstcpy1 = Dst + ((FirstRow - 1) * DstPitch);
    unsigned char *dstcpy2 = dstcpy1 + dstpitch;

    srcpitch <<= 1;
    dstpitch <<= 1;

    // 4 rows per pass
    for (int row = FirstRow; row < maxrows; row += 4)
    {
        if (Second)
        {
            // On second pass, copy over the original, current field
            memcpy(dstcpy1, above,  static_cast<size_t>(DstPitch));
            memcpy(dstcpy2, middle, static_cast<size_t>(DstPitch));
            dstcpy1 += dstpitch;
            dstcpy2 += dstpitch;
        }
        for (int col = 0; col < Width; col += 16)
        {
#if (HAVE_SSE2 && ARCH_X86_64)
            __m128i mid = *reinterpret_cast<__m128i*>(&middle[col]);
            *reinterpret_cast<__m128i*>(&dest1[col]) =
                    _mm_avg_epu8(*reinterpret_cast<__m128i*>(&above[col]), mid);
            *reinterpret_cast<__m128i*>(&dest2[col]) =
                    _mm_avg_epu8(*reinterpret_cast<__m128i*>(&below[col]), mid);
#endif
#if HAVE_INTRINSICS_NEON
            uint8x16_t mid = *reinterpret_cast<uint8x16_t*>(&middle[col]);
            *reinterpret_cast<uint8x16_t*>(&dest1[col]) =
                    vrhaddq_u8(*reinterpret_cast<uint8x16_t*>(&above[col]), mid);
            *reinterpret_cast<uint8x16_t*>(&dest2[col]) =
                    vrhaddq_u8(*reinterpret_cast<uint8x16_t*>(&below[col]), mid);
#endif
        }
        above  += srcpitch;
        middle += srcpitch;
        below  += srcpitch;
        dest1  += dstpitch;
        dest2  += dstpitch;
    }
}

// SIMD optimised version with 16x4 alignment for 10/12/16bit video
static inline void BlendSIMD8x4(unsigned char *Src, int Width, int FirstRow, int LastRow, int Pitch,
                                 unsigned char *Dst, int DstPitch, bool Second)
{
    int srcpitch = Pitch << 1;
    int dstpitch = DstPitch << 1;
    int maxrows  = LastRow - 3;

    unsigned char *above   = Src + ((FirstRow - 1) * Pitch);
    unsigned char *dest1   = Dst + (FirstRow * DstPitch);
    unsigned char *middle  = above + srcpitch;
    unsigned char *dest2   = dest1 + dstpitch;
    unsigned char *below   = middle + srcpitch;
    unsigned char *dstcpy1 = Dst + ((FirstRow - 1) * DstPitch);
    unsigned char *dstcpy2 = dstcpy1 + dstpitch;

    srcpitch <<= 1;
    dstpitch <<= 1;

    // 4 rows per pass
    for (int row = FirstRow; row < maxrows; row += 4)
    {
        if (Second)
        {
            // On second pass, copy over the original, current field
            memcpy(dstcpy1, above,  static_cast<size_t>(DstPitch));
            memcpy(dstcpy2, middle, static_cast<size_t>(DstPitch));
            dstcpy1 += dstpitch;
            dstcpy2 += dstpitch;
        }
        for (int col = 0; col < Width; col += 16)
        {
#if (HAVE_SSE2 && ARCH_X86_64)
            __m128i mid = *reinterpret_cast<__m128i*>(&middle[col]);
            *reinterpret_cast<__m128i*>(&dest1[col]) =
                    _mm_avg_epu16(*reinterpret_cast<__m128i*>(&above[col]), mid);
            *reinterpret_cast<__m128i*>(&dest2[col]) =
                    _mm_avg_epu16(*reinterpret_cast<__m128i*>(&below[col]), mid);
#endif
#if HAVE_INTRINSICS_NEON
            uint16x8_t mid = *reinterpret_cast<uint16x8_t*>(&middle[col]);
            *reinterpret_cast<uint16x8_t*>(&dest1[col]) =
                    vrhaddq_u16(*reinterpret_cast<uint16x8_t*>(&above[col]), mid);
            *reinterpret_cast<uint16x8_t*>(&dest2[col]) =
                    vrhaddq_u16(*reinterpret_cast<uint16x8_t*>(&below[col]), mid);
#endif
        }
        above  += srcpitch;
        middle += srcpitch;
        below  += srcpitch;
        dest1  += dstpitch;
        dest2  += dstpitch;
    }
}
#endif

void MythDeinterlacer::Blend(VideoFrame *Frame, FrameScanType Scan)
{
    if (Frame->height < 16 || Frame->width < 16)
        return;

    bool second = false;
    VideoFrame *src = Frame;

    if (m_doubleRate)
    {
        if (!SetUpCache(Frame))
            return;
        // copy/cache on first pass.
        if (kScan_Interlaced == Scan)
            memcpy(m_bobFrame->buf, Frame->buf, static_cast<size_t>(m_bobFrame->size));
        else
            second = true;
        src = m_bobFrame;
    }

    bool hidepth = ColorDepth(src->codec) > 8;
    bool top = second ? !m_topFirst : m_topFirst;
    uint count = planes(src->codec);
    for (uint plane = 0; plane < count; plane++)
    {
        int  height  = height_for_plane(src->codec, src->height, plane);
        int firstrow = top ? 1 : 2;
        bool height4 = (height % 4) == 0;
        bool width4  = (src->pitches[plane] % 4) == 0;
        // N.B. all frames allocated by MythTV should have 16 byte alignment
        // for all planes
#if (HAVE_SSE2 && ARCH_X86_64) || HAVE_INTRINSICS_NEON
        bool width16 = (src->pitches[plane] % 16) == 0;
        // profiling SSE2 suggests it is usually 4x faster - as expected
        if (s_haveSIMD && height4 && width16)
        {
            if (hidepth)
            {
                BlendSIMD8x4(src->buf + src->offsets[plane],
                             pitch_for_plane(src->codec, src->width, plane),
                             firstrow, height, src->pitches[plane],
                             Frame->buf + Frame->offsets[plane], Frame->pitches[plane],
                             second);
            }
            else
            {
                BlendSIMD16x4(src->buf + src->offsets[plane],
                              width_for_plane(src->codec, src->width, plane),
                              firstrow, height, src->pitches[plane],
                              Frame->buf + Frame->offsets[plane], Frame->pitches[plane],
                              second);
            }
        }
        else
#endif
        // N.B. There is no 10bit support here - but it shouldn't be necessary
        // as everything should be 16byte aligned and 10/12bit interlaced video
        // is virtually unheard of.
        if (width4 && height4 && !hidepth)
        {
            BlendC4x4(src->buf + src->offsets[plane],
                      width_for_plane(src->codec, src->width, plane),
                      firstrow, height, src->pitches[plane],
                      Frame->buf + Frame->offsets[plane], Frame->pitches[plane],
                      second);
        }
    }
    Frame->already_deinterlaced = 1;
}
