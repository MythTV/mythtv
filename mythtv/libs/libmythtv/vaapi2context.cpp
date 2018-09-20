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
}

#define LOC QString("VAAPI2: ")

Vaapi2Context::Vaapi2Context() :
    MythCodecContext()
{

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
    else
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("Decoder %1 supports device type %2.")
                .arg((*ppCodec)->name).arg(av_hwdevice_get_type_name(type)));
        pix_fmt = fmt;
        return (MythCodecID)(kCodec_MPEG1_VAAPI2 + (stream_type - 1));
    }
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
