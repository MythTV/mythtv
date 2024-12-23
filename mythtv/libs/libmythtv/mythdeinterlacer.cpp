// MythTV
#include "libmythbase/mythconfig.h"
#include "libmythbase/mythlogging.h"

#include "mythavutil.h"
#include "mythdeinterlacer.h"
#include "mythvideoprofile.h"

#include <algorithm>
#include <thread>

extern "C" {
#include "libavfilter/buffersrc.h"
#include "libavfilter/buffersink.h"
#include "libavutil/cpu.h"
}

#include <QtGlobal>

#ifdef Q_PROCESSOR_X86_64
#   include <emmintrin.h>
static const bool s_haveSIMD = true;
#elif HAVE_INTRINSICS_NEON
#   include <arm_neon.h>
static const bool s_haveSIMD = av_get_cpu_flags() & AV_CPU_FLAG_NEON;
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
void MythDeinterlacer::Filter(MythVideoFrame *Frame, FrameScanType Scan,
                              MythVideoProfile *Profile, bool Force)
{
    // nothing to see here
    if (!Frame || !is_interlaced(Scan))
    {
        Cleanup();
        return;
    }

    if (Frame->m_alreadyDeinterlaced)
        return;

    // Sanity check frame format
    if (!MythVideoFrame::YUVFormat(Frame->m_type))
    {
        Cleanup();
        return;
    }

    // check for software deinterlacing
    bool doublerate = true;
    bool topfieldfirst = Frame->m_interlacedReverse ? !Frame->m_topFieldFirst : Frame->m_topFieldFirst;

    auto deinterlacer = Frame->GetDoubleRateOption(DEINT_CPU);
    auto other        = Frame->GetDoubleRateOption(DEINT_SHADER);
    if (other)
    {
        Cleanup();
        return;
    }

    if (!deinterlacer)
    {
        doublerate   = false;
        deinterlacer = Frame->GetSingleRateOption(DEINT_CPU);
        other        = Frame->GetSingleRateOption(DEINT_SHADER);
        if (!deinterlacer || other)
        {
            Cleanup();
            return;
        }
    }

    // libavfilter will not deinterlace NV12 frames. Allow shaders in this case.
    // libswscale (for bob/onefield) is fine, as is our linearblend.
    if ((deinterlacer == DEINT_HIGH) && MythVideoFrame::FormatIsNV12(Frame->m_type))
    {
        Cleanup();
        Frame->m_deinterlaceSingle = Frame->m_deinterlaceSingle | DEINT_SHADER;
        Frame->m_deinterlaceDouble = Frame->m_deinterlaceDouble | DEINT_SHADER;
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

    bool otherchanged = Frame->m_width != m_width     || Frame->m_height  != m_height ||
                        deinterlacer != m_deintType || doublerate     != m_doubleRate ||
                        Frame->m_type != m_inputType;

    if ((deinterlacer == DEINT_HIGH) && fieldorderchanged)
    {
        bool alreadyauto = m_autoFieldOrder;
        bool change = m_lastFieldChange && (qAbs(m_lastFieldChange - Frame->m_frameCounter) < 10);
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
        m_lastFieldChange = Frame->m_frameCounter;
    }

    // Check for a change in input or deinterlacer
    if (fieldorderchanged || otherchanged)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Deinterlacer change: %1x%2 %3 dr:%4 tff:%5 -> %6x%7 %8 dr:%9 tff:%10")
            .arg(m_width).arg(m_height).arg(MythVideoFrame::FormatDescription(m_inputType))
            .arg(m_doubleRate).arg(m_topFirst)
            .arg(Frame->m_width).arg(Frame->m_height)
            .arg(MythVideoFrame::FormatDescription(Frame->m_type))
            .arg(doublerate).arg(topfieldfirst));
        if (!Initialise(Frame, deinterlacer, doublerate, topfieldfirst, Profile))
        {
            Cleanup();
            return;
        }
        Force = true;
    }
    else if ((m_deintType == DEINT_HIGH) && (qAbs(Frame->m_frameCounter - m_discontinuityCounter) > 1))
    {
        if (!Initialise(Frame, deinterlacer, doublerate, topfieldfirst, Profile))
        {
            Cleanup();
            return;
        }
        Force = true;
    }

    m_discontinuityCounter = Frame->m_frameCounter;

    // Set in use deinterlacer for debugging
    Frame->m_deinterlaceInuse = m_deintType | DEINT_CPU;
    Frame->m_deinterlaceInuse2x = m_doubleRate;

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
    if (MythAVUtil::FillAVFrame(m_frame, Frame, m_inputFmt) < 1)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Error converting frame");
        return;
    }

    m_frame->width  = Frame->m_width;
    m_frame->height = Frame->m_height;
    m_frame->format = Frame->m_pixFmt;
    m_frame->pts    = Frame->m_timecode.count();

    auto AddFrame = [](AVFilterContext* Source, AVFrame* AvFrame)
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
    if ((m_frame->format != m_inputFmt) || (Frame->m_pitches[0] < m_frame->linesize[0]) ||
        (Frame->m_pitches[1] < m_frame->linesize[1]) || (Frame->m_pitches[2] < m_frame->linesize[2]))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Filter returned unexpected format");
        return;
    }

    // Copy AVFrame back to VideoFrame
    uint count = MythVideoFrame::GetNumPlanes(Frame->m_type);
    for (uint plane = 0; plane < count; ++plane)
    {
        MythVideoFrame::CopyPlane(Frame->m_buffer + Frame->m_offsets[plane], Frame->m_pitches[plane],
                                  m_frame->data[plane], m_frame->linesize[plane],
                                  MythVideoFrame::GetPitchForPlane(m_inputType, m_frame->width, plane),
                                  MythVideoFrame::GetHeightForPlane(m_inputType, m_frame->height, plane));
    }

    Frame->m_timecode = std::chrono::milliseconds(m_frame->pts);
    Frame->m_alreadyDeinterlaced = true;

    // Free frame data
    av_frame_unref(m_frame);
}

void MythDeinterlacer::Cleanup()
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
        delete m_bobFrame;
        m_bobFrame = nullptr;
    }

    m_deintType = DEINT_NONE;
}

///\brief Initialise deinterlacing using the given MythDeintType
bool MythDeinterlacer::Initialise(MythVideoFrame *Frame, MythDeintType Deinterlacer,
                                  bool DoubleRate, bool TopFieldFirst, MythVideoProfile *Profile)
{
    auto autofieldorder = m_autoFieldOrder;
    auto lastfieldchange = m_lastFieldChange;
    Cleanup();
    m_source = nullptr;
    m_sink   = nullptr;

    if (!Frame)
        return false;

    m_width     = Frame->m_width;
    m_height    = Frame->m_height;
    m_inputType = Frame->m_type;
    m_inputFmt  = MythAVUtil::FrameTypeToPixelFormat(Frame->m_type);
    auto name   = MythVideoFrame::DeinterlacerName(Deinterlacer | DEINT_CPU, DoubleRate);

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
    if (MythAVUtil::PixelFormatToFrameType(m_inputFmt) != m_inputType)
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
        threads = std::clamp(Profile->GetMaxCPUs(), 1U, std::max(8U, std::thread::hardware_concurrency()));
    }

    AVFilterInOut* inputs = nullptr;
    AVFilterInOut* outputs = nullptr;

    int parity {1};
    if (m_autoFieldOrder)
        parity = -1;
    else if (TopFieldFirst)
        parity = 0;
    auto deint = QString("yadif=mode=%1:parity=%2:threads=%3")
        .arg(DoubleRate ? 1 : 0).arg(parity).arg(threads);

    auto graph = QString("buffer=video_size=%1x%2:pix_fmt=%3:time_base=1/1[in];[in]%4[out];[out] buffersink")
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

bool MythDeinterlacer::SetUpCache(MythVideoFrame *Frame)
{
    if (!Frame)
        return false;

    if (m_bobFrame && !((m_bobFrame->m_bufferSize == Frame->m_bufferSize)) && (m_bobFrame->m_width == Frame->m_width) &&
                        (m_bobFrame->m_height == Frame->m_height) && (m_bobFrame->m_type == Frame->m_type))
    {
        delete m_bobFrame;
        m_bobFrame = nullptr;
    }

    if (!m_bobFrame)
    {
        m_bobFrame = new MythVideoFrame(Frame->m_type, MythVideoFrame::GetAlignedBuffer(Frame->m_bufferSize),
                                        Frame->m_bufferSize, Frame->m_width, Frame->m_height);
        LOG(VB_PLAYBACK, LOG_INFO, "Created new 'bob' cache frame");
    }

    return m_bobFrame && m_bobFrame->m_buffer != nullptr;
}

void MythDeinterlacer::OneField(MythVideoFrame *Frame, FrameScanType Scan)
{
    if (!m_swsContext)
        return;

    // we need a frame for caching - both to preserve the second field if
    // needed and ensure we are not filtering in place (i.e. from src to src).
    if (!SetUpCache(Frame))
        return;

    // copy/cache on first pass
    if (kScan_Interlaced == Scan)
        memcpy(m_bobFrame->m_buffer, Frame->m_buffer, m_bobFrame->m_bufferSize);

    // Convert VideoFrame to AVFrame - no copy
    AVFrame dstframe;
    if ((MythAVUtil::FillAVFrame(m_frame, m_bobFrame, m_inputFmt) < 1) ||
        (MythAVUtil::FillAVFrame(&dstframe, Frame, m_inputFmt) < 1))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Error converting frame");
        return;
    }

    bool topfield   = Scan == kScan_Interlaced ? m_topFirst : !m_topFirst;
    dstframe.width  = Frame->m_width;
    dstframe.height = Frame->m_height;
    dstframe.format = Frame->m_pixFmt;

    m_frame->width  = m_bobFrame->m_width;
    m_frame->format = m_bobFrame->m_pixFmt;

    // Fake the frame height and stride to simulate a single field
    m_frame->height = Frame->m_height >> 1;
    m_frame->flags &= ~AV_FRAME_FLAG_INTERLACED;
    uint nbplanes = MythVideoFrame::GetNumPlanes(m_inputType);
    for (uint i = 0; i < nbplanes; i++)
    {
        if (!topfield)
            m_frame->data[i] = m_frame->data[i] + m_frame->linesize[i];
        m_frame->linesize[i] = m_frame->linesize[i] << 1;
    }

    // and scale to full height
    int result = sws_scale(m_swsContext, m_frame->data, m_frame->linesize, 0, m_frame->height,
                           dstframe.data, dstframe.linesize);

    if (result != Frame->m_height)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Error scaling frame: height %1 expected %2")
            .arg(result).arg(Frame->m_height));
    }
    Frame->m_alreadyDeinterlaced = true;
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

    unsigned char *above   = Src + ((FirstRow - 1) * static_cast<ptrdiff_t>(Pitch));
    unsigned char *dest1   = Dst + (FirstRow * static_cast<ptrdiff_t>(DstPitch));
    unsigned char *middle  = above + srcpitch;
    unsigned char *dest2   = dest1 + dstpitch;
    unsigned char *below   = middle + srcpitch;
    unsigned char *dstcpy1 = Dst + ((FirstRow - 1) * static_cast<ptrdiff_t>(DstPitch));
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

#if defined(Q_PROCESSOR_X86_64) || HAVE_INTRINSICS_NEON
// SIMD optimised version with 16x4 alignment
static inline void BlendSIMD16x4(unsigned char *Src, int Width, int FirstRow, int LastRow, int Pitch,
                                 unsigned char *Dst, int DstPitch, bool Second)
{
    int srcpitch = Pitch << 1;
    int dstpitch = DstPitch << 1;
    int maxrows  = LastRow - 3;

    unsigned char *above   = Src + ((FirstRow - 1) * static_cast<ptrdiff_t>(Pitch));
    unsigned char *dest1   = Dst + (FirstRow * static_cast<ptrdiff_t>(DstPitch));
    unsigned char *middle  = above + srcpitch;
    unsigned char *dest2   = dest1 + dstpitch;
    unsigned char *below   = middle + srcpitch;
    unsigned char *dstcpy1 = Dst + ((FirstRow - 1) * static_cast<ptrdiff_t>(DstPitch));
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
#if defined(Q_PROCESSOR_X86_64)
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

    unsigned char *above   = Src + ((FirstRow - 1) * static_cast<ptrdiff_t>(Pitch));
    unsigned char *dest1   = Dst + (FirstRow * static_cast<ptrdiff_t>(DstPitch));
    unsigned char *middle  = above + srcpitch;
    unsigned char *dest2   = dest1 + dstpitch;
    unsigned char *below   = middle + srcpitch;
    unsigned char *dstcpy1 = Dst + ((FirstRow - 1) * static_cast<ptrdiff_t>(DstPitch));
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
#if defined(Q_PROCESSOR_X86_64)
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

void MythDeinterlacer::Blend(MythVideoFrame *Frame, FrameScanType Scan)
{
    if (Frame->m_height < 16 || Frame->m_width < 16)
        return;

    bool second = false;
    MythVideoFrame *src = Frame;

    if (m_doubleRate)
    {
        if (!SetUpCache(Frame))
            return;
        // copy/cache on first pass.
        if (kScan_Interlaced == Scan)
            memcpy(m_bobFrame->m_buffer, Frame->m_buffer, m_bobFrame->m_bufferSize);
        else
            second = true;
        src = m_bobFrame;
    }

    bool hidepth = MythVideoFrame::ColorDepth(src->m_type) > 8;
    bool top = second ? !m_topFirst : m_topFirst;
    uint count = MythVideoFrame::GetNumPlanes(src->m_type);
    for (uint plane = 0; plane < count; plane++)
    {
        int  height  = MythVideoFrame::GetHeightForPlane(src->m_type, src->m_height, plane);
        int firstrow = top ? 1 : 2;
        bool height4 = (height % 4) == 0;
        bool width4  = (src->m_pitches[plane] % 4) == 0;
        // N.B. all frames allocated by MythTV should have 16 byte alignment
        // for all planes
#if defined(Q_PROCESSOR_X86_64) || HAVE_INTRINSICS_NEON
        bool width16 = (src->m_pitches[plane] % 16) == 0;
        // profiling SSE2 suggests it is usually 4x faster - as expected
        if (s_haveSIMD && height4 && width16)
        {
            if (hidepth)
            {
                BlendSIMD8x4(src->m_buffer + src->m_offsets[plane],
                             MythVideoFrame::GetPitchForPlane(src->m_type, src->m_width, plane),
                             firstrow, height, src->m_pitches[plane],
                             Frame->m_buffer + Frame->m_offsets[plane], Frame->m_pitches[plane],
                             second);
            }
            else
            {
                BlendSIMD16x4(src->m_buffer + src->m_offsets[plane],
                              MythVideoFrame::GetWidthForPlane(src->m_type, src->m_width, plane),
                              firstrow, height, src->m_pitches[plane],
                              Frame->m_buffer + Frame->m_offsets[plane], Frame->m_pitches[plane],
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
            BlendC4x4(src->m_buffer + src->m_offsets[plane],
                      MythVideoFrame::GetWidthForPlane(src->m_type, src->m_width, plane),
                      firstrow, height, src->m_pitches[plane],
                      Frame->m_buffer + Frame->m_offsets[plane], Frame->m_pitches[plane],
                      second);
        }
    }
    Frame->m_alreadyDeinterlaced = true;
}
