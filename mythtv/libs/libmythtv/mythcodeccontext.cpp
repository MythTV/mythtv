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
#include "mythcodeccontext.h"
#include "videooutbase.h"
#include "mythplayer.h"
#ifdef USING_VAAPI2
#include "vaapi2context.h"
#endif
#ifdef USING_NVDEC
#include "nvdeccontext.h"
#endif

extern "C" {
    #include "libavutil/pixfmt.h"
    #include "libavutil/hwcontext.h"
    #include "libavcodec/avcodec.h"
    // #include "libavfilter/avfilter.h"
    #include "libavfilter/buffersink.h"
    #include "libavfilter/buffersrc.h"
    // #include "libavformat/avformat.h"
    #include "libavutil/opt.h"
    #include "libavutil/buffer.h"
}

#define LOC QString("MythCodecContext: ")

MythCodecContext::MythCodecContext() :
    stream(nullptr),
    buffersink_ctx(nullptr),
    buffersrc_ctx(nullptr),
    filter_graph(nullptr),
    filtersInitialized(false),
    hw_frames_ctx(nullptr),
    player(nullptr),
    ptsUsed(0),
    doublerate(false)
{
    priorPts[0] = 0;
    priorPts[1] = 0;
}

// static
MythCodecContext *MythCodecContext::createMythCodecContext(MythCodecID codec)
{
    MythCodecContext *mctx = nullptr;
#ifdef USING_VAAPI2
    if (codec_is_vaapi2(codec))
        mctx = new Vaapi2Context();
#endif
#ifdef USING_NVDEC
    if (codec_is_nvdec(codec))
        mctx = new NvdecContext();
#endif
    // In case neither was defined
    Q_UNUSED(codec);

    if (!mctx)
        mctx = new MythCodecContext();
    return mctx;
}

// static
QStringList MythCodecContext::GetDeinterlacers(const QString& decodername)
{
    QStringList ret;
#ifdef USING_VAAPI2
    if (decodername == "vaapi2")
    {
        ret.append("vaapi2default");
        ret.append("vaapi2bob");
        ret.append("vaapi2weave");
        ret.append("vaapi2motion_adaptive");
        ret.append("vaapi2motion_compensated");
        ret.append("vaapi2doubleratedefault");
        ret.append("vaapi2doubleratebob");
        ret.append("vaapi2doublerateweave");
        ret.append("vaapi2doubleratemotion_adaptive");
        ret.append("vaapi2doubleratemotion_compensated");

/*
    Explanation of vaapi2 deinterlacing modes.
    "mode", "Deinterlacing mode",
        "default", "Use the highest-numbered (and therefore possibly most advanced) deinterlacing algorithm",
        "bob", "Use the bob deinterlacing algorithm",
        "weave", "Use the weave deinterlacing algorithm",
        "motion_adaptive", "Use the motion adaptive deinterlacing algorithm",
        "motion_compensated", "Use the motion compensated deinterlacing algorithm",

    "rate", "Generate output at frame rate or field rate",
        "frame", "Output at frame rate (one frame of output for each field-pair)",
        "field", "Output at field rate (one frame of output for each field)",

    "auto", "Only deinterlace fields, passing frames through unchanged",
        1 = enabled
        0 = disabled
*/
    }
#endif
#ifdef USING_NVDEC
    if (decodername == "nvdec")
    {
        ret.append("nvdecweave");
        ret.append("nvdecbob");
        ret.append("nvdecadaptive");
        ret.append("nvdecdoublerateweave");
        ret.append("nvdecdoubleratebob");
        ret.append("nvdecdoublerateadaptive");

/*
    Explanation of nvdec deinterlacing modes.
        "weave",    "Weave deinterlacing (do nothing)"
        "bob",      "Bob deinterlacing"
        "adaptive", "Adaptive deinterlacing"
        "drop_second_field", "Drop second field when deinterlacing"
*/

    }
#endif
    // in case neither of the above was defined
    Q_UNUSED(decodername);
    return ret;
}
// static - Find if a deinterlacer is codec-provided
bool MythCodecContext::isCodecDeinterlacer(const QString& decodername)
{
    return (decodername.startsWith("vaapi2")
            || decodername.startsWith("nvdec") );
}

// Currently this will only set up the filter after an interlaced frame.
// If we need other filters apart from deinterlace filters we will
// need to make a change here.

int MythCodecContext::FilteredReceiveFrame(AVCodecContext *ctx, AVFrame *frame)
{
    int ret = 0;

    while (true)
    {
        if (filter_graph)
        {
            ret = av_buffersink_get_frame(buffersink_ctx, frame);
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
        if (frame->interlaced_frame || filter_graph)
        {
            if (!filtersInitialized
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
            if (filter_graph)
            {
                ret = av_buffersrc_add_frame(buffersrc_ctx, frame);
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

// Setup or change deinterlacer.
// Same usage as VideoOutBase::SetupDeinterlace
// enable - true to enable, false to disable
// name - empty to use video profile deinterlacers, otherwise
//        use the supplied name.
// return true if the deinterlacer was found as a hardware deinterlacer.
// return false if the deinterlacer is nnt a hardware deinterlacer,
//        and a videououtput deinterlacer should be tried instead.

bool MythCodecContext::setDeinterlacer(bool enable, QString name)
{
    QMutexLocker lock(&contextLock);
    // Code to disable interlace
    if (!enable)
    {
        if (deinterlacername.isEmpty())
            return true;
        deinterlacername.clear();
        doublerate = false;
        filtersInitialized = false;
        return true;
    }

    // Code to enable or change interlace
    if (name.isEmpty())
    {
        if (deinterlacername.isEmpty())
        {
            DecoderBase *dec = nullptr;
            VideoDisplayProfile *vdp = nullptr;
            if (player)
                dec = player->GetDecoder();
            if (dec)
                vdp = dec->GetVideoDisplayProfile();
            if (vdp)
                name = vdp->GetFilteredDeint(QString());
        }
        else
            name = deinterlacername;
    }
    bool ret = true;
    if (!isCodecDeinterlacer(name))
        name.clear();

    if (name.isEmpty())
        ret = false;

    if (deinterlacername == name)
        return ret;

    deinterlacername = name;
    doublerate = deinterlacername.contains("doublerate");
    filtersInitialized = false;
    return ret;
}

bool MythCodecContext::BestDeint(void)
{
    deinterlacername.clear();
    doublerate = false;
    return setDeinterlacer(true);
}

bool MythCodecContext::FallbackDeint(void)
{
    return setDeinterlacer(true,GetFallbackDeint());
}

QString MythCodecContext::GetFallbackDeint(void)
{

    DecoderBase *dec = nullptr;
    VideoDisplayProfile *vdp = nullptr;
    if (player)
        dec = player->GetDecoder();
    if (dec)
        vdp = dec->GetVideoDisplayProfile();
    if (vdp)
        return vdp->GetFallbackDeinterlacer();
    return QString();
}

// Dummy default method for those that do not use deinterlacers

int MythCodecContext::InitDeinterlaceFilter(AVCodecContext * /*ctx*/, AVFrame *frame)
{
    QMutexLocker lock(&contextLock);
    width = frame->width;
    height = frame->height;
    filtersInitialized = true;
    return 0;
}
