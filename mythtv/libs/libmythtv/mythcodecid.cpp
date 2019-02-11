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
        case kCodec_VP9:
            return "VP9";
        case kCodec_HEVC:
            return "HEVC";

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
        case kCodec_VP9_VDPAU:
            return "VP9 VDPAU";
        case kCodec_HEVC_VDPAU:
            return "HEVC VDPAU";

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
        case kCodec_VP9_VAAPI:
            return "VP9 VAAPI";
        case kCodec_HEVC_VAAPI:
            return "HEVC VAAPI";

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
        case kCodec_VP9_DXVA2:
            return "VP9 DXVA2";
        case kCodec_HEVC_DXVA2:
            return "HEVC DXVA2";

        case kCodec_MPEG1_MEDIACODEC:
            return "MPEG1 MEDIACODEC";
        case kCodec_MPEG2_MEDIACODEC:
            return "MPEG2 MEDIACODEC";
        case kCodec_H263_MEDIACODEC:
            return "H.263 MEDIACODEC";
        case kCodec_MPEG4_MEDIACODEC:
            return "MPEG4 MEDIACODEC";
        case kCodec_H264_MEDIACODEC:
            return "H.264 MEDIACODEC";
        case kCodec_VC1_MEDIACODEC:
            return "VC1 MEDIACODEC";
        case kCodec_WMV3_MEDIACODEC:
            return "WMV3 MEDIACODEC";
        case kCodec_VP8_MEDIACODEC:
            return "VP8 MEDIACODEC";
        case kCodec_VP9_MEDIACODEC:
            return "VP9 MEDIACODEC";
        case kCodec_HEVC_MEDIACODEC:
            return "HEVC MEDIACODEC";

        case kCodec_MPEG1_VAAPI2:
            return "MPEG1 VAAPI2";
        case kCodec_MPEG2_VAAPI2:
            return "MPEG2 VAAPI2";
        case kCodec_H263_VAAPI2:
            return "H.263 VAAPI2";
        case kCodec_MPEG4_VAAPI2:
            return "MPEG4 VAAPI2";
        case kCodec_H264_VAAPI2:
            return "H.264 VAAPI2";
        case kCodec_VC1_VAAPI2:
            return "VC1 VAAPI2";
        case kCodec_WMV3_VAAPI2:
            return "WMV3 VAAPI2";
        case kCodec_VP8_VAAPI2:
            return "VP8 VAAPI2";
        case kCodec_VP9_VAAPI2:
            return "VP9 VAAPI2";
        case kCodec_HEVC_VAAPI2:
            return "HEVC VAAPI2";

        case kCodec_MPEG1_NVDEC:
            return "MPEG1 NVDEC";
        case kCodec_MPEG2_NVDEC:
            return "MPEG2 NVDEC";
        case kCodec_H263_NVDEC:
            return "H.263 NVDEC";
        case kCodec_MPEG4_NVDEC:
            return "MPEG4 NVDEC";
        case kCodec_H264_NVDEC:
            return "H.264 NVDEC";
        case kCodec_VC1_NVDEC:
            return "VC1 NVDEC";
        case kCodec_WMV3_NVDEC:
            return "WMV3 NVDEC";
        case kCodec_VP8_NVDEC:
            return "VP8 NVDEC";
        case kCodec_VP9_NVDEC:
            return "VP9 NVDEC";
        case kCodec_HEVC_NVDEC:
            return "HEVC NVDEC";

        default:
            break;
    }

    return QString("Unknown(%1)").arg(codecid);
}

AVCodecID myth2av_codecid(MythCodecID codec_id, bool &vdpau)
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
        case kCodec_VP9:
            ret = AV_CODEC_ID_VP9;
            break;
        case kCodec_HEVC:
            ret = AV_CODEC_ID_HEVC;
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
        case kCodec_VP9_VDPAU:
            ret = AV_CODEC_ID_VP9;
            break;
        case kCodec_HEVC_VDPAU:
            ret = AV_CODEC_ID_HEVC;
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
        case kCodec_VP9_VAAPI:
            ret = AV_CODEC_ID_VP9;
            break;
        case kCodec_HEVC_VAAPI:
            ret = AV_CODEC_ID_HEVC;
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
        case kCodec_VP9_DXVA2:
            ret = AV_CODEC_ID_VP9;
            break;
        case kCodec_HEVC_DXVA2:
            ret = AV_CODEC_ID_HEVC;
            break;

        case kCodec_MPEG1_MEDIACODEC:
            ret = AV_CODEC_ID_MPEG1VIDEO;
            break;
        case kCodec_MPEG2_MEDIACODEC:
            ret = AV_CODEC_ID_MPEG2VIDEO;
            break;
        case kCodec_H263_MEDIACODEC:
            ret = AV_CODEC_ID_H263;
            break;
        case kCodec_MPEG4_MEDIACODEC:
            ret = AV_CODEC_ID_MPEG4;
            break;
        case kCodec_H264_MEDIACODEC:
            ret = AV_CODEC_ID_H264;
            break;
        case kCodec_VC1_MEDIACODEC:
            ret = AV_CODEC_ID_VC1;
            break;
        case kCodec_WMV3_MEDIACODEC:
            ret = AV_CODEC_ID_WMV3;
            break;
        case kCodec_VP8_MEDIACODEC:
            ret = AV_CODEC_ID_VP8;
            break;
        case kCodec_VP9_MEDIACODEC:
            ret = AV_CODEC_ID_VP9;
            break;
        case kCodec_HEVC_MEDIACODEC:
            ret = AV_CODEC_ID_HEVC;
            break;

        case kCodec_MPEG1_VAAPI2:
            ret = AV_CODEC_ID_MPEG1VIDEO;
            break;
        case kCodec_MPEG2_VAAPI2:
            ret = AV_CODEC_ID_MPEG2VIDEO;
            break;
        case kCodec_H263_VAAPI2:
            ret = AV_CODEC_ID_H263;
            break;
        case kCodec_MPEG4_VAAPI2:
            ret = AV_CODEC_ID_MPEG4;
            break;
        case kCodec_H264_VAAPI2:
            ret = AV_CODEC_ID_H264;
            break;
        case kCodec_VC1_VAAPI2:
            ret = AV_CODEC_ID_VC1;
            break;
        case kCodec_WMV3_VAAPI2:
            ret = AV_CODEC_ID_WMV3;
            break;
        case kCodec_VP8_VAAPI2:
            ret = AV_CODEC_ID_VP8;
            break;
        case kCodec_VP9_VAAPI2:
            ret = AV_CODEC_ID_VP9;
            break;
        case kCodec_HEVC_VAAPI2:
            ret = AV_CODEC_ID_HEVC;
            break;

        case kCodec_MPEG1_NVDEC:
            ret = AV_CODEC_ID_MPEG1VIDEO;
            break;
        case kCodec_MPEG2_NVDEC:
            ret = AV_CODEC_ID_MPEG2VIDEO;
            break;
        case kCodec_H263_NVDEC:
            ret = AV_CODEC_ID_H263;
            break;
        case kCodec_MPEG4_NVDEC:
            ret = AV_CODEC_ID_MPEG4;
            break;
        case kCodec_H264_NVDEC:
            ret = AV_CODEC_ID_H264;
            break;
        case kCodec_VC1_NVDEC:
            ret = AV_CODEC_ID_VC1;
            break;
        case kCodec_WMV3_NVDEC:
            ret = AV_CODEC_ID_WMV3;
            break;
        case kCodec_VP8_NVDEC:
            ret = AV_CODEC_ID_VP8;
            break;
        case kCodec_VP9_NVDEC:
            ret = AV_CODEC_ID_VP9;
            break;
        case kCodec_HEVC_NVDEC:
            ret = AV_CODEC_ID_HEVC;
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
        case AV_CODEC_ID_VP9:
            return 9;
        case AV_CODEC_ID_HEVC:
            return 10;
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
        case kCodec_MPEG1_MEDIACODEC:
        case kCodec_MPEG1_VAAPI2:
        case kCodec_MPEG1_NVDEC:
        case kCodec_MPEG2:
        case kCodec_MPEG2_VDPAU:
        case kCodec_MPEG2_VAAPI:
        case kCodec_MPEG2_DXVA2:
        case kCodec_MPEG2_MEDIACODEC:
        case kCodec_MPEG2_VAAPI2:
        case kCodec_MPEG2_NVDEC:
            return "MPEG-2";

        case kCodec_H263:
        case kCodec_H263_VDPAU:
        case kCodec_H263_VAAPI:
        case kCodec_H263_DXVA2:
        case kCodec_H263_MEDIACODEC:
        case kCodec_H263_VAAPI2:
        case kCodec_H263_NVDEC:
            return "H.263";

        case kCodec_NUV_MPEG4:
        case kCodec_MPEG4:
        case kCodec_MPEG4_VDPAU:
        case kCodec_MPEG4_VAAPI:
        case kCodec_MPEG4_DXVA2:
        case kCodec_MPEG4_MEDIACODEC:
        case kCodec_MPEG4_VAAPI2:
        case kCodec_MPEG4_NVDEC:
            return "MPEG-4";

        case kCodec_H264:
        case kCodec_H264_VDPAU:
        case kCodec_H264_VAAPI:
        case kCodec_H264_DXVA2:
        case kCodec_H264_MEDIACODEC:
        case kCodec_H264_VAAPI2:
        case kCodec_H264_NVDEC:
            return "H.264";

        case kCodec_VC1:
        case kCodec_VC1_VDPAU:
        case kCodec_VC1_VAAPI:
        case kCodec_VC1_DXVA2:
        case kCodec_VC1_MEDIACODEC:
        case kCodec_VC1_VAAPI2:
        case kCodec_VC1_NVDEC:
            return "VC-1";

        case kCodec_WMV3:
        case kCodec_WMV3_VDPAU:
        case kCodec_WMV3_VAAPI:
        case kCodec_WMV3_DXVA2:
        case kCodec_WMV3_MEDIACODEC:
        case kCodec_WMV3_VAAPI2:
        case kCodec_WMV3_NVDEC:
            return "WMV3";

        case kCodec_VP8:
        case kCodec_VP8_VDPAU:
        case kCodec_VP8_VAAPI:
        case kCodec_VP8_DXVA2:
        case kCodec_VP8_MEDIACODEC:
        case kCodec_VP8_VAAPI2:
        case kCodec_VP8_NVDEC:
            return "VP8";

        case kCodec_VP9:
        case kCodec_VP9_VDPAU:
        case kCodec_VP9_VAAPI:
        case kCodec_VP9_DXVA2:
        case kCodec_VP9_MEDIACODEC:
        case kCodec_VP9_VAAPI2:
        case kCodec_VP9_NVDEC:
            return "VP8";

        case kCodec_HEVC:
        case kCodec_HEVC_VDPAU:
        case kCodec_HEVC_VAAPI:
        case kCodec_HEVC_DXVA2:
        case kCodec_HEVC_MEDIACODEC:
        case kCodec_HEVC_VAAPI2:
        case kCodec_HEVC_NVDEC:
            return "HEVC";

        case kCodec_NONE:
        case kCodec_NORMAL_END:
        case kCodec_VDPAU_END:
        case kCodec_VAAPI_END:
        case kCodec_DXVA2_END:
        case kCodec_MEDIACODEC_END:
        case kCodec_VAAPI2_END:
        case kCodec_NVDEC_END:
            return QString();
    }

    return QString();
}

QString get_decoder_name(MythCodecID codec_id)
{
    if (codec_is_vdpau(codec_id))
        return "vdpau";

    if (codec_is_vaapi(codec_id))
        return "vaapi";

    if (codec_is_dxva2(codec_id))
        return "dxva2";

    if (codec_is_mediacodec(codec_id))
        return "mediacodec";

    if (codec_is_vaapi2(codec_id))
        return "vaapi2";

    if (codec_is_nvdec(codec_id))
        return "nvdec";

    return "ffmpeg";
}
