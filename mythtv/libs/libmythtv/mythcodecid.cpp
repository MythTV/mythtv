#include "mythcodecid.h"
#include "mythcontext.h"

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

        default:
            break;
    }

    return QString("Unknown(%1)").arg(codecid);
}

int myth2av_codecid(MythCodecID codec_id,
                    bool &vld, bool &idct, bool &mc)
{
    vld = idct = mc = false;
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
            VERBOSE(VB_IMPORTANT, "Error: DVDV MPEG not supported by ffmpeg");
            break;
        case kCodec_H264_DVDV:
            VERBOSE(VB_IMPORTANT, "Error: DVDV H.265 not supported by ffmpeg");
            break;

        default:
            VERBOSE(VB_IMPORTANT,
                    QString("Error: MythCodecID %1 has not been "
                            "added to myth2av_codecid").arg(codec_id));
            break;
    } // switch(codec_id)
    return ret;
}
