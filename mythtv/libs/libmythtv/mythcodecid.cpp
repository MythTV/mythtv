#include "mythcodecid.h"
#include "mythlogging.h"

extern "C"
{
#include "libavcodec/avcodec.h"
}

QString toString(MythCodecID codecid)
{
    switch (codecid)
    {
        case kCodec_NONE:
            return "None";

        case kCodec_NUV_RTjpeg:
            return "NUV RTjpeg";
        case kCodec_NUV_MPEG4:
            return "NUV MPEG4";

        case kCodec_MPEG1:
            return "MPEG1";
        case kCodec_MPEG2:
            return "MPEG2";
        case kCodec_H263:
            return "H.263";
        case kCodec_MPEG4:
            return "MPEG4";
        case kCodec_H264:
            return "H.264";
        case kCodec_VC1:
            return "VC-1";
        case kCodec_WMV3:
            return "WMV3";
        case kCodec_VP8:
            return "VP8";

        case kCodec_MPEG1_VDPAU:
            return "MPEG1 VDPAU";
        case kCodec_MPEG2_VDPAU:
            return "MPEG2 VDPAU";
        case kCodec_H263_VDPAU:
            return "H.263 VDPAU";
        case kCodec_MPEG4_VDPAU:
            return "MPEG4 VDPAU";
        case kCodec_H264_VDPAU:
            return "H.264 VDPAU";
        case kCodec_VC1_VDPAU:
            return "VC1 VDPAU";
        case kCodec_WMV3_VDPAU:
            return "WMV3 VDPAU";
        case kCodec_VP8_VDPAU:
            return "VP8 VDPAU";

        case kCodec_MPEG1_VAAPI:
            return "MPEG1 VAAPI";
        case kCodec_MPEG2_VAAPI:
            return "MPEG2 VAAPI";
        case kCodec_H263_VAAPI:
            return "H.263 VAAPI";
        case kCodec_MPEG4_VAAPI:
            return "MPEG4 VAAPI";
        case kCodec_H264_VAAPI:
            return "H.264 VAAPI";
        case kCodec_VC1_VAAPI:
            return "VC1 VAAPI";
        case kCodec_WMV3_VAAPI:
            return "WMV3 VAAPI";
        case kCodec_VP8_VAAPI:
            return "VP8 VAAPI";

        case kCodec_MPEG1_DXVA2:
            return "MPEG1 DXVA2";
        case kCodec_MPEG2_DXVA2:
            return "MPEG2 DXVA2";
        case kCodec_H263_DXVA2:
            return "H.263 DXVA2";
        case kCodec_MPEG4_DXVA2:
            return "MPEG4 DXVA2";
        case kCodec_H264_DXVA2:
            return "H.264 DXVA2";
        case kCodec_VC1_DXVA2:
            return "VC1 DXVA2";
        case kCodec_WMV3_DXVA2:
            return "WMV3 DXVA2";
        case kCodec_VP8_DXVA2:
            return "VP8 DXVA2";

        default:
            break;
    }

    return QString("Unknown(%1)").arg(codecid);
}

int myth2av_codecid(MythCodecID codec_id, bool &vdpau)
{
    vdpau = false;
    AVCodecID ret = AV_CODEC_ID_NONE;
    switch (codec_id)
    {
        case kCodec_NONE:
        case kCodec_NUV_MPEG4:
        case kCodec_NUV_RTjpeg:
            ret = AV_CODEC_ID_NONE;
            break;

        case kCodec_MPEG1:
            ret = AV_CODEC_ID_MPEG1VIDEO;
            break;
        case kCodec_MPEG2:
            ret = AV_CODEC_ID_MPEG2VIDEO;
            break;
        case kCodec_H263:
            ret = AV_CODEC_ID_H263;
            break;
        case kCodec_MPEG4:
            ret = AV_CODEC_ID_MPEG4;
            break;
        case kCodec_H264:
            ret = AV_CODEC_ID_H264;
            break;
        case kCodec_VP8:
            ret = AV_CODEC_ID_VP8;
            break;

        case kCodec_VC1:
            ret = AV_CODEC_ID_VC1;
            break;
        case kCodec_WMV3:
            ret = AV_CODEC_ID_WMV3;
            break;

        case kCodec_MPEG1_VDPAU:
            ret = AV_CODEC_ID_MPEG1VIDEO;
            vdpau = true;
            break;
        case kCodec_MPEG2_VDPAU:
            ret = AV_CODEC_ID_MPEG2VIDEO;
            vdpau = true;
            break;
        case kCodec_H263_VDPAU:
            LOG(VB_GENERAL, LOG_ERR,
                "Error: VDPAU H.263 not supported by ffmpeg");
            break;
        case kCodec_MPEG4_VDPAU:
            ret = AV_CODEC_ID_MPEG4;
            break;

        case kCodec_H264_VDPAU:
            ret = AV_CODEC_ID_H264;
            vdpau = true;
            break;
        case kCodec_VC1_VDPAU:
            ret = AV_CODEC_ID_VC1;
            vdpau = true;
            break;
        case kCodec_WMV3_VDPAU:
            ret = AV_CODEC_ID_WMV3;
            vdpau = true;
            break;
        case kCodec_VP8_VDPAU:
            ret = AV_CODEC_ID_VP8;
            break;

        case kCodec_MPEG1_VAAPI:
            ret = AV_CODEC_ID_MPEG1VIDEO;
            break;
        case kCodec_MPEG2_VAAPI:
            ret = AV_CODEC_ID_MPEG2VIDEO;
            break;
        case kCodec_H263_VAAPI:
            ret = AV_CODEC_ID_H263;
            break;
        case kCodec_MPEG4_VAAPI:
            ret = AV_CODEC_ID_MPEG4;
            break;
        case kCodec_H264_VAAPI:
            ret = AV_CODEC_ID_H264;
            break;
        case kCodec_VC1_VAAPI:
            ret = AV_CODEC_ID_VC1;
            break;
        case kCodec_WMV3_VAAPI:
            ret = AV_CODEC_ID_WMV3;
            break;
        case kCodec_VP8_VAAPI:
            ret = AV_CODEC_ID_VP8;
            break;

        case kCodec_MPEG1_DXVA2:
            ret = AV_CODEC_ID_MPEG1VIDEO;
            break;
        case kCodec_MPEG2_DXVA2:
            ret = AV_CODEC_ID_MPEG2VIDEO;
            break;
        case kCodec_H263_DXVA2:
            ret = AV_CODEC_ID_H263;
            break;
        case kCodec_MPEG4_DXVA2:
            ret = AV_CODEC_ID_MPEG4;
            break;
        case kCodec_H264_DXVA2:
            ret = AV_CODEC_ID_H264;
            break;
        case kCodec_VC1_DXVA2:
            ret = AV_CODEC_ID_VC1;
            break;
        case kCodec_WMV3_DXVA2:
            ret = AV_CODEC_ID_WMV3;
            break;
        case kCodec_VP8_DXVA2:
            ret = AV_CODEC_ID_VP8;
            break;

        default:
            LOG(VB_GENERAL, LOG_ERR,
                QString("Error: MythCodecID %1 has not been "
                        "added to myth2av_codecid").arg(codec_id));
            break;
    } // switch(codec_id)
    return ret;
}

int mpeg_version(int codec_id)
{
    switch (codec_id)
    {
        case AV_CODEC_ID_MPEG1VIDEO:
            return 1;
        case AV_CODEC_ID_MPEG2VIDEO:
            return 2;
        case AV_CODEC_ID_H263:
            return 3;
        case AV_CODEC_ID_MPEG4:
            return 4;
        case AV_CODEC_ID_H264:
            return 5;
        case AV_CODEC_ID_VC1:
            return 6;
        case AV_CODEC_ID_WMV3:
            return 7;
        case AV_CODEC_ID_VP8:
            return 8;
        default:
            break;
    }
    return 0;
}

QString get_encoding_type(MythCodecID codecid)
{
    switch (codecid)
    {
        case kCodec_NUV_RTjpeg:
            return "RTjpeg";

        case kCodec_MPEG1:
        case kCodec_MPEG1_VDPAU:
        case kCodec_MPEG1_VAAPI:
        case kCodec_MPEG1_DXVA2:
        case kCodec_MPEG2:
        case kCodec_MPEG2_VDPAU:
        case kCodec_MPEG2_VAAPI:
        case kCodec_MPEG2_DXVA2:
            return "MPEG-2";

        case kCodec_H263:
        case kCodec_H263_VDPAU:
        case kCodec_H263_VAAPI:
        case kCodec_H263_DXVA2:
            return "H.263";

        case kCodec_NUV_MPEG4:
        case kCodec_MPEG4:
        case kCodec_MPEG4_VDPAU:
        case kCodec_MPEG4_VAAPI:
        case kCodec_MPEG4_DXVA2:
            return "MPEG-4";

        case kCodec_H264:
        case kCodec_H264_VDPAU:
        case kCodec_H264_VAAPI:
        case kCodec_H264_DXVA2:
            return "H.264";

        case kCodec_VC1:
        case kCodec_VC1_VDPAU:
        case kCodec_VC1_VAAPI:
        case kCodec_VC1_DXVA2:
            return "VC-1";

        case kCodec_WMV3:
        case kCodec_WMV3_VDPAU:
        case kCodec_WMV3_VAAPI:
        case kCodec_WMV3_DXVA2:
            return "WMV3";

        case kCodec_VP8:
        case kCodec_VP8_VDPAU:
        case kCodec_VP8_VAAPI:
        case kCodec_VP8_DXVA2:
            return "VP8";

        case kCodec_NONE:
        case kCodec_NORMAL_END:
        case kCodec_VDPAU_END:
        case kCodec_VAAPI_END:
        case kCodec_DXVA2_END:
            return QString::null;
    }

    return QString::null;
}

QString get_decoder_name(MythCodecID codec_id)
{
    if (codec_is_vdpau(codec_id))
        return "vdpau";

    if (codec_is_vaapi(codec_id))
        return "vaapi";

    if (codec_is_dxva2(codec_id))
        return "dxva2";

    return "ffmpeg";
}
