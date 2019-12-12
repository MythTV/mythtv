// MythTV
#include "mythlogging.h"
#include "mythavutil.h"
#include "mythdeinterlacer.h"

extern "C" {
#include "libavfilter/buffersrc.h"
#include "libavfilter/buffersink.h"
}

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
 * Medium - libavfilter's yadif
 * High - libavfilter's bwdif
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
void MythDeinterlacer::Filter(VideoFrame *Frame, FrameScanType Scan, bool Force)
{
    // nothing to see here
    if (!Frame || (Scan != kScan_Interlaced && Scan != kScan_Intr2ndField))
    {
        Cleanup();
        return;
    }

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
    // libswscale (for bob/onefield) is fine.
    if (format_is_nv12(Frame->codec) && (deinterlacer != DEINT_BASIC))
    {
        Cleanup();
        Frame->deinterlace_single = Frame->deinterlace_single | DEINT_SHADER;
        Frame->deinterlace_double = Frame->deinterlace_double | DEINT_SHADER;
        return;
    }

    // Check for a change in input or deinterlacer
    if (Frame->width != m_width     || Frame->height  != m_height ||
        deinterlacer != m_deintType || doublerate     != m_doubleRate ||
        topfieldfirst != m_topFirst || Frame->codec   != m_inputType)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Deinterlacer change: %1x%2 %3 dr:%4 tff:%5 -> %6x%7 %8 dr:%9 tff:%10")
            .arg(m_width).arg(m_height).arg(format_description(m_inputType))
            .arg(m_doubleRate).arg(m_topFirst)
            .arg(Frame->width).arg(Frame->height).arg(format_description(Frame->codec))
            .arg(doublerate).arg(topfieldfirst));
        if (!Initialise(Frame, deinterlacer, doublerate, topfieldfirst))
        {
            Cleanup();
            return;
        }
    }

    // Set in use deinterlacer for debugging
    Frame->deinterlace_inuse = m_deintType | DEINT_CPU;
    Frame->deinterlace_inuse2x = m_doubleRate;

    // onefield or bob
    if (m_deintType == DEINT_BASIC)
    {
        if (!m_swsContext)
            return;

        // we need a frame for caching - both to preserve the second field if
        // needed and ensure we are not filtering in place (i.e. from src to src).
        if (!m_bobFrame)
        {
            m_bobFrame = new VideoFrame;
            if (!m_bobFrame)
                return;
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
            m_bobFrame->buf = static_cast<unsigned char*>(av_malloc(Frame->size + 64));
            m_bobFrame->size = Frame->size;
        }

        if (!m_bobFrame->buf)
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
        // Both yadif and bwdif need 2 frames to work with - add the frame
        // again if we need a result now
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
                                  bool DoubleRate, bool TopFieldFirst)
{
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
    if (Deinterlacer == DEINT_BASIC)
    {
        m_deintType  = Deinterlacer;
        m_doubleRate = DoubleRate;
        m_topFirst   = TopFieldFirst;
        m_swsContext = sws_getCachedContext(m_swsContext, m_width, m_height >> 1, m_inputFmt,
                                            m_width, m_height, m_inputFmt, SWS_FAST_BILINEAR,
                                            nullptr, nullptr, nullptr);
        if (m_swsContext != nullptr)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Using deinterlacer '%1'").arg(name));
            return true;
        }
        return false;
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

    AVFilterInOut* inputs = nullptr;
    AVFilterInOut* outputs = nullptr;

    QString deint;
    switch (Deinterlacer)
    {
        case DEINT_MEDIUM:
            deint = QString("yadif=mode=%1:parity=%2").arg(DoubleRate ? 1 : 0).arg(TopFieldFirst ? 0 : 1);
            break;
        case DEINT_HIGH:
            deint = QString("bwdif=mode=%1:parity=%2").arg(DoubleRate ? 1 : 0).arg(TopFieldFirst ? 0 : 1);
            break;
        default: break;
    }

    if (deint.isEmpty())
        return false;

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
                LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Created deinterlacer '%1'").arg(name));
                m_deintType  = Deinterlacer;
                m_doubleRate = DoubleRate;
                m_topFirst   = TopFieldFirst;
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
