//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017 MythTV Developers <mythtv-dev@mythtv.org>
//
// This is part of MythTV (https://www.mythtv.org)
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#include "mythcorecontext.h"
#include "mythlogging.h"
#include "vaapi2context.h"
#include "videooutbase.h"
#include "mythplayer.h"
#include "mythhwcontext.h"

extern "C" {
    #include "libavutil/pixfmt.h"
    #include "libavutil/hwcontext.h"
    #include "libavcodec/avcodec.h"
    #include "libavfilter/avfilter.h"
    #include "libavformat/avformat.h"
    #include "libavfilter/buffersrc.h"
}

#define LOC QString("VAAPI2: ")

Vaapi2Context::~Vaapi2Context()
{
    CloseFilters();
}

// Currently this will only set up the filter after an interlaced frame.
// If we need other filters apart from deinterlace filters we will
// need to make a change here.

int Vaapi2Context::FilteredReceiveFrame(AVCodecContext *ctx, AVFrame *frame)
{
    int ret = 0;

    while (true)
    {
        if (m_filterGraph)
        {
            ret = av_buffersink_get_frame(m_bufferSinkCtx, frame);
            if  (ret >= 0)
            {
                if (priorPts[0] && ptsUsed == priorPts[1])
                {
                    frame->pts = priorPts[1] + (priorPts[1] - priorPts[0])/2;
                    frame->scte_cc_len = 0;
                    frame->atsc_cc_len = 0;
                    av_frame_remove_side_data(frame, AV_FRAME_DATA_A53_CC);
                }
                else
                {
                    frame->pts = priorPts[1];
                    ptsUsed = priorPts[1];
                }
            }
            if  (ret != AVERROR(EAGAIN))
                break;
        }

        // EAGAIN or no filter graph
        ret = avcodec_receive_frame(ctx, frame);
        if (ret < 0)
            break;
        priorPts[0]=priorPts[1];
        priorPts[1]=frame->pts;
        if (frame->interlaced_frame || m_filterGraph)
        {
            if (!m_filtersInitialized
              || width != frame->width
              || height != frame->height)
            {
                // bypass any frame of unknown format
                if (frame->format < 0)
                    break;
                ret = InitDeinterlaceFilter(ctx, frame);
                if (ret < 0)
                {
                    LOG(VB_GENERAL, LOG_ERR, LOC + "InitDeinterlaceFilter failed - continue without filters");
                    break;
                }
            }
            if (m_filterGraph)
            {
                ret = av_buffersrc_add_frame(m_bufferSrcCtx, frame);
                if (ret < 0)
                    break;
            }
            else
                break;
        }
        else
            break;
    }

    return ret;
}

int Vaapi2Context::InitDeinterlaceFilter(AVCodecContext *ctx, AVFrame *frame)
{
    QMutexLocker lock(&contextLock);
    char args[512];
    int ret = 0;
    CloseFilters();
    width = frame->width;
    height = frame->height;
    m_filtersInitialized = true;
    if (!player || !m_stream)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Player or stream is not set up in MythCodecContext");
        return -1;
    }
    if (m_doubleRate && !player->CanSupportDoubleRate())
    {
        QString request = deinterlacername;
        deinterlacername = GetFallbackDeint();
        LOG(VB_PLAYBACK, LOG_INFO, LOC
          + QString("Deinterlacer %1 requires double rate, switching to %2 instead.")
          .arg(request).arg(deinterlacername));
        if (!isCodecDeinterlacer(deinterlacername))
            deinterlacername.clear();
        m_doubleRate = deinterlacername.contains("doublerate");

        // if the fallback is a non-vaapi - deinterlace will be turned off
        // and the videoout methods can take over.
    }
    QString filters;
    if (isValidDeinterlacer(deinterlacername))
        filters = GetDeinterlaceFilter();

    if (filters.isEmpty())
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            "Disabled hardware decoder based deinterlacer.");
        return ret;
    }
    const AVFilter *buffersrc  = avfilter_get_by_name("buffer");
    const AVFilter *buffersink = avfilter_get_by_name("buffersink");
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    AVRational time_base = m_stream->time_base;
    AVBufferSrcParameters* params = nullptr;

    m_filterGraph = avfilter_graph_alloc();
    if (!outputs || !inputs || !m_filterGraph)
    {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    /* buffer video source: the decoded frames from the decoder will be inserted here. */
    snprintf(args, sizeof(args),
            "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
            frame->width, frame->height, frame->format, // ctx->pix_fmt,
            time_base.num, time_base.den,
            ctx->sample_aspect_ratio.num, ctx->sample_aspect_ratio.den);

    // isInterlaced = frame->interlaced_frame;

    ret = avfilter_graph_create_filter(&m_bufferSrcCtx, buffersrc, "in",
                                       args, nullptr, m_filterGraph);
    if (ret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "avfilter_graph_create_filter failed for buffer source");
        goto end;
    }

    params = av_buffersrc_parameters_alloc();
    if (m_hwFramesCtx)
        av_buffer_unref(&m_hwFramesCtx);
    m_hwFramesCtx = av_buffer_ref(frame->m_hwFramesCtx);
    params->m_hwFramesCtx = m_hwFramesCtx;

    ret = av_buffersrc_parameters_set(m_bufferSrcCtx, params);

    if (ret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "av_buffersrc_parameters_set failed");
        goto end;
    }

    av_freep(&params);

    /* buffer video sink: to terminate the filter chain. */
    ret = avfilter_graph_create_filter(&m_bufferSinkCtx, buffersink, "out",
                                       nullptr, nullptr, m_filterGraph);
    if (ret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "avfilter_graph_create_filter failed for buffer sink");
        goto end;
    }

    /*
     * Set the endpoints for the filter graph. The filter_graph will
     * be linked to the graph described by filters_descr.
     */

    /*
     * The buffer source output must be connected to the input pad of
     * the first filter described by filters_descr; since the first
     * filter input label is not specified, it is set to "in" by
     * default.
     */
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = m_bufferSrcCtx;
    outputs->pad_idx    = 0;
    outputs->next       = nullptr;

    /*
     * The buffer sink input must be connected to the output pad of
     * the last filter described by filters_descr; since the last
     * filter output label is not specified, it is set to "out" by
     * default.
     */
    inputs->name       = av_strdup("out");
    inputs->filter_ctx = m_bufferSinkCtx;
    inputs->pad_idx    = 0;
    inputs->next       = nullptr;

    if ((ret = avfilter_graph_parse_ptr(m_filterGraph, filters.toLocal8Bit(),
                                    &inputs, &outputs,nullptr)) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC
            + QString("avfilter_graph_parse_ptr failed for %1").arg(filters));
        goto end;
    }

    if ((ret = avfilter_graph_config(m_filterGraph, nullptr)) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC
            + QString("avfilter_graph_config failed"));
        goto end;
    }

    LOG(VB_GENERAL, LOG_INFO, LOC +
        QString("Enabled hardware decoder based deinterlace filter '%1': <%2>.")
            .arg(deinterlacername).arg(filters));
end:
    if (ret < 0)
    {
        avfilter_graph_free(&m_filterGraph);
        m_filterGraph = nullptr;
        m_doubleRate = false;
    }
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    return ret;
}

void Vaapi2Context::CloseFilters()
{
    avfilter_graph_free(&m_filterGraph);
    m_filterGraph = nullptr;
    m_bufferSinkCtx = nullptr;
    m_bufferSrcCtx = nullptr;
    m_filtersInitialized = false;
    m_ptsUsed = 0;
    m_priorPts[0] = 0;
    m_priorPts[1] = 0;
    // isInterlaced = 0;
    m_width = 0;
    m_height = 0;

    if (m_hwFramesCtx)
        av_buffer_unref(&m_hwFramesCtx);
}
