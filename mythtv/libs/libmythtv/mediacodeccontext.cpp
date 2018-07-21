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

#include "mediacodeccontext.h"

#include "mythlogging.h"

extern "C" {
    #include "libavutil/pixfmt.h"
    #include "libavutil/hwcontext.h"
    #include "libavcodec/avcodec.h"
}

#define LOC QString("MEDIACODEC: ")

MythCodecID MediaCodecContext::GetBestSupportedCodec(
    AVCodec **ppCodec,
    const QString &decoder,
    uint stream_type,
    AVPixelFormat &pix_fmt)
{
    enum AVHWDeviceType type = AV_HWDEVICE_TYPE_MEDIACODEC;

    AVPixelFormat fmt = AV_PIX_FMT_NONE;
    if (decoder == "mediacodec")
    {
        QString decodername = QString((*ppCodec)->name) + "_mediacodec";
        if (decodername == "mpeg2video_mediacodec")
            decodername = "mpeg2_mediacodec";
        AVCodec *newCodec = avcodec_find_decoder_by_name (decodername.toLocal8Bit());
        if (newCodec)
        {
            *ppCodec = newCodec;
            fmt = AV_PIX_FMT_MEDIACODEC;
        }
        else
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("Decoder %1 does not support device type %2.")
                    .arg((*ppCodec)->name).arg(av_hwdevice_get_type_name(type)));
    }

    if (fmt == AV_PIX_FMT_NONE)
        return (MythCodecID)(kCodec_MPEG1 + (stream_type - 1));
    else
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("Decoder %1 supports device type %2.")
                .arg((*ppCodec)->name).arg(av_hwdevice_get_type_name(type)));
        pix_fmt = fmt;
        return (MythCodecID)(kCodec_MPEG1_MEDIACODEC + (stream_type - 1));
    }
}
