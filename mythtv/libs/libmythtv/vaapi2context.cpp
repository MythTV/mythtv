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


MythCodecID Vaapi2Context::GetBestSupportedCodec(
    AVCodec **ppCodec,
    const QString &decoder,
    uint stream_type,
    AVPixelFormat &pix_fmt)
{
    enum AVHWDeviceType type = AV_HWDEVICE_TYPE_VAAPI;

    AVPixelFormat fmt = AV_PIX_FMT_NONE;
    if (decoder == "vaapi2")
    {
        for (int i = 0;; i++) {
            const AVCodecHWConfig *config = avcodec_get_hw_config(*ppCodec, i);
            if (!config) {
                LOG(VB_PLAYBACK, LOG_INFO, LOC +
                    QString("Decoder %1 does not support device type %2.")
                        .arg((*ppCodec)->name).arg(av_hwdevice_get_type_name(type)));
                break;
            }
            if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
                config->device_type == type) {
                fmt = config->pix_fmt;
                break;
            }
        }
    }
    if (fmt == AV_PIX_FMT_NONE)
        return (MythCodecID)(kCodec_MPEG1 + (stream_type - 1));

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("Decoder %1 supports device type %2.")
        .arg((*ppCodec)->name).arg(av_hwdevice_get_type_name(type)));
    pix_fmt = fmt;
    return (MythCodecID)(kCodec_MPEG1_VAAPI2 + (stream_type - 1));
}

// const char *filter_descr = "scale=78:24,transpose=cclock";
/* other way:
   scale=78:24 [scl]; [scl] transpose=cclock // assumes "[in]" and "[out]" to be input output pads respectively
 */

int Vaapi2Context::HwDecoderInit(AVCodecContext *ctx)
{
    int ret = 0;
    AVBufferRef *hw_device_ctx = nullptr;

    const char *device = nullptr;
    QString vaapiDevice = gCoreContext->GetSetting("VAAPIDevice");
    if (!vaapiDevice.isEmpty())
    {
        device = vaapiDevice.toLocal8Bit().constData();
    }

    ret = av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_VAAPI,
                                      device, nullptr, 0);
    if (ret < 0)
    {
        char error[AV_ERROR_MAX_STRING_SIZE];
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("av_hwdevice_ctx_create  Device = <%3> error: %1 (%2)")
            .arg(av_make_error_string(error, sizeof(error), ret))
            .arg(ret).arg(vaapiDevice));
    }
    else
    {
        ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
        av_buffer_unref(&hw_device_ctx);
    }

    return ret;
}

QString Vaapi2Context::GetDeinterlaceFilter()
{
    // example filter - deinterlace_vaapi=mode=default:rate=frame:auto=1
    // example deinterlacername - vaapi2doubleratemotion_compensated
    QString ret;
    QString rate="frame";
    if (!isValidDeinterlacer(deinterlacername))
        return ret;
    QString filtername = deinterlacername;
    filtername.remove(0,6); //remove "vaapi2"
    if (filtername.startsWith("doublerate"))
    {
        rate="field";
        filtername.remove(0,10);  // remove "doublerate"
    }
    ret=QString("deinterlace_vaapi=mode=%1:rate=%2:auto=1")
        .arg(filtername).arg(rate);

    return ret;
}

bool Vaapi2Context::isValidDeinterlacer(QString filtername)
{
    return filtername.startsWith("vaapi2");
}

QStringList Vaapi2Context::GetDeinterlacers(void)
{
    return MythCodecContext::GetDeinterlacers("vaapi2");
}


int Vaapi2Context::InitDeinterlaceFilter(AVCodecContext *ctx, AVFrame *frame)
{
    QMutexLocker lock(&contextLock);
    char args[512];
    int ret = 0;
    CloseFilters();
    width = frame->width;
    height = frame->height;
    filtersInitialized = true;
    if (!player || !stream)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Player or stream is not set up in MythCodecContext");
        return -1;
    }
    if (doublerate && !player->CanSupportDoubleRate())
    {
        QString request = deinterlacername;
        deinterlacername = GetFallbackDeint();
        LOG(VB_PLAYBACK, LOG_INFO, LOC
          + QString("Deinterlacer %1 requires double rate, switching to %2 instead.")
          .arg(request).arg(deinterlacername));
        if (!isCodecDeinterlacer(deinterlacername))
            deinterlacername.clear();
        doublerate = deinterlacername.contains("doublerate");

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
    AVRational time_base = stream->time_base;
    AVBufferSrcParameters* params = nullptr;

    filter_graph = avfilter_graph_alloc();
    if (!outputs || !inputs || !filter_graph)
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

    ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
                                       args, nullptr, filter_graph);
    if (ret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "avfilter_graph_create_filter failed for buffer source");
        goto end;
    }

    params = av_buffersrc_parameters_alloc();
    if (hw_frames_ctx)
        av_buffer_unref(&hw_frames_ctx);
    hw_frames_ctx = av_buffer_ref(frame->hw_frames_ctx);
    params->hw_frames_ctx = hw_frames_ctx;

    ret = av_buffersrc_parameters_set(buffersrc_ctx, params);

    if (ret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "av_buffersrc_parameters_set failed");
        goto end;
    }

    av_freep(&params);

    /* buffer video sink: to terminate the filter chain. */
    ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                                       nullptr, nullptr, filter_graph);
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
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx    = 0;
    outputs->next       = nullptr;

    /*
     * The buffer sink input must be connected to the output pad of
     * the last filter described by filters_descr; since the last
     * filter output label is not specified, it is set to "out" by
     * default.
     */
    inputs->name       = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx    = 0;
    inputs->next       = nullptr;

    if ((ret = avfilter_graph_parse_ptr(filter_graph, filters.toLocal8Bit(),
                                    &inputs, &outputs,nullptr)) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC
            + QString("avfilter_graph_parse_ptr failed for %1").arg(filters));
        goto end;
    }

    if ((ret = avfilter_graph_config(filter_graph, nullptr)) < 0)
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
        avfilter_graph_free(&filter_graph);
        filter_graph = nullptr;
        doublerate = false;
    }
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    return ret;
}

void Vaapi2Context::CloseFilters()
{
    avfilter_graph_free(&filter_graph);
    filter_graph = nullptr;
    buffersink_ctx = nullptr;
    buffersrc_ctx = nullptr;
    filtersInitialized = false;
    ptsUsed = 0;
    priorPts[0] = 0;
    priorPts[1] = 0;
    // isInterlaced = 0;
    width = 0;
    height = 0;

    if (hw_frames_ctx)
        av_buffer_unref(&hw_frames_ctx);
}
