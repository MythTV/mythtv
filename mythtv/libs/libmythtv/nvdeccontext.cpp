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
#include "nvdeccontext.h"
#include "videooutbase.h"
#include "mythplayer.h"

extern "C" {
    #include "libavutil/pixfmt.h"
    #include "libavutil/hwcontext.h"
    #include "libavutil/opt.h"
    #include "libavcodec/avcodec.h"
}

#define LOC QString("NVDEC: ")

MythCodecID NvdecContext::GetBestSupportedCodec(
    AVCodec **ppCodec,
    const QString &decoder,
    uint stream_type,
    AVPixelFormat &pix_fmt)
{
    enum AVHWDeviceType type = AV_HWDEVICE_TYPE_CUDA;

    AVPixelFormat fmt = AV_PIX_FMT_NONE;
    if (decoder == "nvdec")
    {
        for (int i = 0;; i++) {
            const AVCodecHWConfig *config = avcodec_get_hw_config(*ppCodec, i);
            if (!config)
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC +
                    QString("Decoder %1 does not support device type %2.")
                        .arg((*ppCodec)->name).arg(av_hwdevice_get_type_name(type)));
                break;
            }
            if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
                config->device_type == type)
            {
                QString decodername = QString((*ppCodec)->name) + "_cuvid";
                if (decodername == "mpeg2video_cuvid")
                    decodername = "mpeg2_cuvid";
                AVCodec *newCodec = avcodec_find_decoder_by_name (decodername.toLocal8Bit());
                if (newCodec)
                {
                    *ppCodec = newCodec;
                    fmt = config->pix_fmt;
                }
                else
                    LOG(VB_PLAYBACK, LOG_INFO, LOC +
                        QString("Decoder %1 does not exist.")
                            .arg(decodername));
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
    return (MythCodecID)(kCodec_MPEG1_NVDEC + (stream_type - 1));
}

int NvdecContext::HwDecoderInit(AVCodecContext *ctx)
{
    int ret = 0;
    AVBufferRef *hw_device_ctx = nullptr;

    const char *device = nullptr;
    QString nvdecDevice = gCoreContext->GetSetting("NVDECDevice");
    if (!nvdecDevice.isEmpty())
    {
        device = nvdecDevice.toLocal8Bit().constData();
    }

    ret = av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_CUDA,
                                      device, nullptr, 0);
    if (ret < 0)
    {
        char error[AV_ERROR_MAX_STRING_SIZE];
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("av_hwdevice_ctx_create  Device = <%3> error: %1 (%2)")
            .arg(av_make_error_string(error, sizeof(error), ret))
            .arg(ret).arg(nvdecDevice));
    }
    else
    {
        ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
        av_buffer_unref(&hw_device_ctx);
    }

    SetDeinterlace(ctx);

    return ret;
}


bool NvdecContext::isValidDeinterlacer(QString filtername)
{
    return filtername.startsWith("nvdec");
}

QStringList NvdecContext::GetDeinterlacers(void)
{
    return MythCodecContext::GetDeinterlacers("nvdec");
}

int NvdecContext::SetDeinterlace(AVCodecContext *ctx)
{
    QMutexLocker lock(&contextLock);
    bool dropSecondFld = false;
    int ret = 0;
    QString mode = GetDeinterlaceMode(dropSecondFld);
    if (mode.isEmpty())
    {
        ret = av_opt_set(ctx->priv_data,"deint","weave",0);
        if (ret == 0)
            LOG(VB_GENERAL, LOG_INFO, LOC +
                "Disabled hardware decoder based deinterlacer.");
        return ret;
    }
    ret = av_opt_set(ctx->priv_data,"deint",mode.toLocal8Bit(),0);
    // ret = av_opt_set(ctx,"deint",mode.toLocal8Bit(),AV_OPT_SEARCH_CHILDREN);
    if (ret == 0)
        ret = av_opt_set_int(ctx->priv_data,"drop_second_field",(int)dropSecondFld,0);
        // ret = av_opt_set_int(ctx,"drop_second_field",(int)dropSecondFld,AV_OPT_SEARCH_CHILDREN);
    if (ret == 0)
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Enabled hardware decoder based deinterlace '%1' mode: <%2>.")
                .arg(deinterlacername).arg(mode));

    return ret;
}

QString NvdecContext::GetDeinterlaceMode(bool &dropSecondFld)
{
    // example mode - weave
    // example deinterlacername - nvdecdoublerateweave

    dropSecondFld=true;
    QString mode;
    if (!isValidDeinterlacer(deinterlacername))
        return mode;
    mode = deinterlacername;
    mode.remove(0,5); //remove "nvdec"
    if (mode.startsWith("doublerate"))
    {
        dropSecondFld=false;
        mode.remove(0,10);  // remove "doublerate"
    }
    return mode;
}
