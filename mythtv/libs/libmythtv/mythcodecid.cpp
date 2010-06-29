#include "mythcodecid.h"
#include "mythverbose.h"

extern "C"
{
#include "libs/libavcodec/avcodec.h"
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

        case kCodec_MPEG1_XVMC:
            return "MPEG1 XvMC";
        case kCodec_MPEG2_XVMC:
            return "MPEG2 XvMC";
        case kCodec_H263_XVMC:
            return "H.263 XvMC";
        case kCodec_MPEG4_XVMC:
            return "MPEG4 XvMC";
        case kCodec_H264_XVMC:
            return "H.264 XvMC";

        case kCodec_MPEG1_IDCT:
            return "MPEG1 IDCT";
        case kCodec_MPEG2_IDCT:
            return "MPEG2 IDCT";
        case kCodec_H263_IDCT:
            return "H.263 IDCT";
        case kCodec_MPEG4_IDCT:
            return "MPEG4 IDCT";
        case kCodec_H264_IDCT:
            return "H.264 IDCT";

        case kCodec_MPEG1_VLD:
            return "MPEG1 VLD";
        case kCodec_MPEG2_VLD:
            return "MPEG2 VLD";
        case kCodec_H263_VLD:
            return "H.263 VLD";
        case kCodec_MPEG4_VLD:
            return "MPEG4 VLD";
        case kCodec_H264_VLD:
            return "H.264 VLD";

        case kCodec_MPEG1_DVDV:
            return "MPEG1 DVDV";
        case kCodec_MPEG2_DVDV:
            return "MPEG2 DVDV";
        case kCodec_H263_DVDV:
            return "H.263 DVDV";
        case kCodec_MPEG4_DVDV:
            return "MPEG4 DVDV";
        case kCodec_H264_DVDV:
            return "H.264 DVDV";

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

        default:
            break;
    }

    return QString("Unknown(%1)").arg(codecid);
}

int myth2av_codecid(MythCodecID codec_id,
                    bool &vld, bool &idct, bool &mc, bool &vdpau)
{
    vld = idct = mc = vdpau = false;
    CodecID ret = CODEC_ID_NONE;
    switch (codec_id)
    {
        case kCodec_NONE:
        case kCodec_NUV_MPEG4:
        case kCodec_NUV_RTjpeg:
            ret = CODEC_ID_NONE;
            break;

        case kCodec_MPEG1:
            ret = CODEC_ID_MPEG1VIDEO;
            break;
        case kCodec_MPEG2:
            ret = CODEC_ID_MPEG2VIDEO;
            break;
        case kCodec_H263:
            ret = CODEC_ID_H263;
            break;
        case kCodec_MPEG4:
            ret = CODEC_ID_MPEG4;
            break;
        case kCodec_H264:
            ret = CODEC_ID_H264;
            break;

        case kCodec_VC1:
            ret = CODEC_ID_VC1;
            break;
        case kCodec_WMV3:
            ret = CODEC_ID_WMV3;
            break;

        case kCodec_MPEG1_XVMC:
        case kCodec_MPEG2_XVMC:
            mc = true;
            ret = CODEC_ID_MPEG2VIDEO_XVMC;
            break;
        case kCodec_H263_XVMC:
            VERBOSE(VB_IMPORTANT, "Error: XvMC H.263 not supported by ffmpeg");
            break;
        case kCodec_MPEG4_XVMC:
            VERBOSE(VB_IMPORTANT, "Error: XvMC MPEG4 not supported by ffmpeg");
            break;
        case kCodec_H264_XVMC:
            VERBOSE(VB_IMPORTANT, "Error: XvMC H.264 not supported by ffmpeg");
            break;

        case kCodec_MPEG1_IDCT:
        case kCodec_MPEG2_IDCT:
            idct = mc = true;
            ret = CODEC_ID_MPEG2VIDEO_XVMC;
            break;
        case kCodec_H263_IDCT:
            VERBOSE(VB_IMPORTANT,
                    "Error: XvMC-IDCT H.263 not supported by ffmpeg");
            break;
        case kCodec_MPEG4_IDCT:
            VERBOSE(VB_IMPORTANT,
                    "Error: XvMC-IDCT MPEG4 not supported by ffmpeg");
            break;
        case kCodec_H264_IDCT:
            VERBOSE(VB_IMPORTANT,
                    "Error: XvMC-IDCT H.264 not supported by ffmpeg");
            break;

        case kCodec_MPEG1_VLD:
        case kCodec_MPEG2_VLD:
            vld = true;
            ret = CODEC_ID_MPEG2VIDEO_XVMC_VLD;
            break;
        case kCodec_H263_VLD:
            VERBOSE(VB_IMPORTANT,
                    "Error: XvMC-VLD H.263 not supported by ffmpeg");
            break;
        case kCodec_MPEG4_VLD:
            VERBOSE(VB_IMPORTANT,
                    "Error: XvMC-VLD MPEG4 not supported by ffmpeg");
            break;
        case kCodec_H264_VLD:
            VERBOSE(VB_IMPORTANT,
                    "Error: XvMC-VLD H.264 not supported by ffmpeg");
            break;

        case kCodec_MPEG1_DVDV:
        case kCodec_MPEG2_DVDV:
            ret = CODEC_ID_MPEG2VIDEO_DVDV;
            break;
        case kCodec_H263_DVDV:
            VERBOSE(VB_IMPORTANT, "Error: DVDV H.263 not supported by ffmpeg");
            break;
        case kCodec_MPEG4_DVDV:
            VERBOSE(VB_IMPORTANT, "Error: DVDV MPEG4 not supported by ffmpeg");
            break;
        case kCodec_H264_DVDV:
            VERBOSE(VB_IMPORTANT, "Error: DVDV H.264 not supported by ffmpeg");
            break;

        case kCodec_MPEG1_VDPAU:
            ret = CODEC_ID_MPEG1VIDEO;
            vdpau = true;
            break;
        case kCodec_MPEG2_VDPAU:
            ret = CODEC_ID_MPEG2VIDEO;
            vdpau = true;
            break;
        case kCodec_H263_VDPAU:
            VERBOSE(VB_IMPORTANT, "Error: VDPAU H.263 not supported by ffmpeg");
            break;
        case kCodec_MPEG4_VDPAU:
            ret = CODEC_ID_MPEG4;
            break;

        case kCodec_H264_VDPAU:
            ret = CODEC_ID_H264;
            vdpau = true;
            break;
        case kCodec_VC1_VDPAU:
            ret = CODEC_ID_VC1;
            vdpau = true;
            break;
        case kCodec_WMV3_VDPAU:
            ret = CODEC_ID_WMV3;
            vdpau = true;
            break;

        case kCodec_MPEG1_VAAPI:
            ret = CODEC_ID_MPEG1VIDEO;
            break;
        case kCodec_MPEG2_VAAPI:
            ret = CODEC_ID_MPEG2VIDEO;
            break;
        case kCodec_H263_VAAPI:
            ret = CODEC_ID_H263;
            break;
        case kCodec_MPEG4_VAAPI:
            ret = CODEC_ID_MPEG4;
            break;
        case kCodec_H264_VAAPI:
            ret = CODEC_ID_H264;
            break;
        case kCodec_VC1_VAAPI:
            ret = CODEC_ID_VC1;
            break;
        case kCodec_WMV3_VAAPI:
            ret = CODEC_ID_WMV3;
            break;

        default:
            VERBOSE(VB_IMPORTANT,
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
        case CODEC_ID_MPEG1VIDEO:
            return 1;
        case CODEC_ID_MPEG2VIDEO:
        case CODEC_ID_MPEG2VIDEO_XVMC:
        case CODEC_ID_MPEG2VIDEO_XVMC_VLD:
        case CODEC_ID_MPEG2VIDEO_DVDV:
            return 2;
        case CODEC_ID_H263:
            return 3;
        case CODEC_ID_MPEG4:
            return 4;
        case CODEC_ID_H264:
            return 5;
        case CODEC_ID_VC1:
            return 6;
        case CODEC_ID_WMV3:
            return 7;
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
        case kCodec_MPEG1_XVMC:
        case kCodec_MPEG1_IDCT:
        case kCodec_MPEG1_VLD:
        case kCodec_MPEG1_DVDV:
        case kCodec_MPEG1_VDPAU:
        case kCodec_MPEG2:
        case kCodec_MPEG2_XVMC:
        case kCodec_MPEG2_IDCT:
        case kCodec_MPEG2_VLD:
        case kCodec_MPEG2_DVDV:
        case kCodec_MPEG2_VDPAU:
        case kCodec_MPEG2_VAAPI:
            return "MPEG-2";

        case kCodec_H263:
        case kCodec_H263_XVMC:
        case kCodec_H263_IDCT:
        case kCodec_H263_VLD:
        case kCodec_H263_DVDV:
        case kCodec_H263_VDPAU:
        case kCodec_H263_VAAPI:
            return "H.263";

        case kCodec_NUV_MPEG4:
        case kCodec_MPEG4:
        case kCodec_MPEG4_IDCT:
        case kCodec_MPEG4_XVMC:
        case kCodec_MPEG4_VLD:
        case kCodec_MPEG4_DVDV:
        case kCodec_MPEG4_VDPAU:
        case kCodec_MPEG4_VAAPI:
            return "MPEG-4";

        case kCodec_H264:
        case kCodec_H264_XVMC:
        case kCodec_H264_IDCT:
        case kCodec_H264_VLD:
        case kCodec_H264_DVDV:
        case kCodec_H264_VDPAU:
            return "H.264";

        case kCodec_NONE:
        case kCodec_NORMAL_END:
        case kCodec_STD_XVMC_END:
        case kCodec_VLD_END:
        case kCodec_DVDV_END:
            return QString::null;
    }

    return QString::null;
}

QString get_decoder_name(MythCodecID codec_id, bool libmpeg2)
{
    if (libmpeg2)
        return "libmpeg2";

    if (codec_is_dvdv(codec_id))
        return "macaccel";

    if (codec_is_xvmc_std(codec_id))
        return "xvmc";

    if (codec_is_xvmc_vld(codec_id))
        return "xvmc-vld";

    if (codec_is_vdpau(codec_id))
        return "vdpau";

    if (codec_is_vaapi(codec_id))
        return "vaapi";

    return "ffmpeg";
}
