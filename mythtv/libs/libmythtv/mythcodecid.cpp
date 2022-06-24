#include "mythcodecid.h"
#include "libmythbase/mythlogging.h"

extern "C"
{
#include "libavcodec/avcodec.h"
}

QString toString(MythCodecID codecid)
{
    switch (codecid)
    {
        case kCodec_NONE: return "None";

        case kCodec_NUV_RTjpeg: return "NUV RTjpeg";
        case kCodec_NUV_MPEG4:  return "NUV MPEG4";

        case kCodec_MPEG1: return "MPEG1";
        case kCodec_MPEG2: return "MPEG2";
        case kCodec_H263:  return "H.263";
        case kCodec_MPEG4: return "MPEG4";
        case kCodec_H264:  return "H.264";
        case kCodec_VC1:   return "VC-1";
        case kCodec_WMV3:  return "WMV3";
        case kCodec_VP8:   return "VP8";
        case kCodec_VP9:   return "VP9";
        case kCodec_HEVC:  return "HEVC";
        case kCodec_AV1:   return "AV1";

        case kCodec_MPEG1_VDPAU: return "MPEG1 VDPAU";
        case kCodec_MPEG2_VDPAU: return "MPEG2 VDPAU";
        case kCodec_H263_VDPAU:  return "H.263 VDPAU";
        case kCodec_MPEG4_VDPAU: return "MPEG4 VDPAU";
        case kCodec_H264_VDPAU:  return "H.264 VDPAU";
        case kCodec_VC1_VDPAU:   return "VC1 VDPAU";
        case kCodec_WMV3_VDPAU:  return "WMV3 VDPAU";
        case kCodec_VP8_VDPAU:   return "VP8 VDPAU";
        case kCodec_VP9_VDPAU:   return "VP9 VDPAU";
        case kCodec_HEVC_VDPAU:  return "HEVC VDPAU";
        case kCodec_AV1_VDPAU:   return "AV1 VDPAU";

        case kCodec_MPEG1_VDPAU_DEC: return "MPEG1 VDPAU DEC";
        case kCodec_MPEG2_VDPAU_DEC: return "MPEG2 VDPAU DEC";
        case kCodec_H263_VDPAU_DEC:  return "H.263 VDPAU DEC";
        case kCodec_MPEG4_VDPAU_DEC: return "MPEG4 VDPAU DEC";
        case kCodec_H264_VDPAU_DEC:  return "H.264 VDPAU DEC";
        case kCodec_VC1_VDPAU_DEC:   return "VC1 VDPAU DEC";
        case kCodec_WMV3_VDPAU_DEC:  return "WMV3 VDPAU DEC";
        case kCodec_VP8_VDPAU_DEC:   return "VP8 VDPAU DEC";
        case kCodec_VP9_VDPAU_DEC:   return "VP9 VDPAU DEC";
        case kCodec_HEVC_VDPAU_DEC:  return "HEVC VDPAU DEC";
        case kCodec_AV1_VDPAU_DEC:   return "AV1 VDPAU DEC";

        case kCodec_MPEG1_VAAPI: return "MPEG1 VAAPI";
        case kCodec_MPEG2_VAAPI: return "MPEG2 VAAPI";
        case kCodec_H263_VAAPI:  return "H.263 VAAPI";
        case kCodec_MPEG4_VAAPI: return "MPEG4 VAAPI";
        case kCodec_H264_VAAPI:  return "H.264 VAAPI";
        case kCodec_VC1_VAAPI:   return "VC1 VAAPI";
        case kCodec_WMV3_VAAPI:  return "WMV3 VAAPI";
        case kCodec_VP8_VAAPI:   return "VP8 VAAPI";
        case kCodec_VP9_VAAPI:   return "VP9 VAAPI";
        case kCodec_HEVC_VAAPI:  return "HEVC VAAPI";
        case kCodec_AV1_VAAPI:   return "AV1 VAAPI";

        case kCodec_MPEG1_VAAPI_DEC: return "MPEG1 VAAPI DEC";
        case kCodec_MPEG2_VAAPI_DEC: return "MPEG2 VAAPI DEC";
        case kCodec_H263_VAAPI_DEC:  return "H.263 VAAPI DEC";
        case kCodec_MPEG4_VAAPI_DEC: return "MPEG4 VAAPI DEC";
        case kCodec_H264_VAAPI_DEC:  return "H.264 VAAPI DEC";
        case kCodec_VC1_VAAPI_DEC:   return "VC1 VAAPI DEC";
        case kCodec_WMV3_VAAPI_DEC:  return "WMV3 VAAPI DEC";
        case kCodec_VP8_VAAPI_DEC:   return "VP8 VAAPI DEC";
        case kCodec_VP9_VAAPI_DEC:   return "VP9 VAAPI DEC";
        case kCodec_HEVC_VAAPI_DEC:  return "HEVC VAAPI DEC";
        case kCodec_AV1_VAAPI_DEC:   return "AV1 VAAPI DEC";

        case kCodec_MPEG1_DXVA2: return "MPEG1 DXVA2";
        case kCodec_MPEG2_DXVA2: return "MPEG2 DXVA2";
        case kCodec_H263_DXVA2:  return "H.263 DXVA2";
        case kCodec_MPEG4_DXVA2: return "MPEG4 DXVA2";
        case kCodec_H264_DXVA2:  return "H.264 DXVA2";
        case kCodec_VC1_DXVA2:   return "VC1 DXVA2";
        case kCodec_WMV3_DXVA2:  return "WMV3 DXVA2";
        case kCodec_VP8_DXVA2:   return "VP8 DXVA2";
        case kCodec_VP9_DXVA2:   return "VP9 DXVA2";
        case kCodec_HEVC_DXVA2:  return "HEVC DXVA2";
        case kCodec_AV1_DXVA2:   return "AV1 DXVA2";

        case kCodec_MPEG1_MEDIACODEC: return "MPEG1 MEDIACODEC";
        case kCodec_MPEG2_MEDIACODEC: return "MPEG2 MEDIACODEC";
        case kCodec_H263_MEDIACODEC:  return "H.263 MEDIACODEC";
        case kCodec_MPEG4_MEDIACODEC: return "MPEG4 MEDIACODEC";
        case kCodec_H264_MEDIACODEC:  return "H.264 MEDIACODEC";
        case kCodec_VC1_MEDIACODEC:   return "VC1 MEDIACODEC";
        case kCodec_WMV3_MEDIACODEC:  return "WMV3 MEDIACODEC";
        case kCodec_VP8_MEDIACODEC:   return "VP8 MEDIACODEC";
        case kCodec_VP9_MEDIACODEC:   return "VP9 MEDIACODEC";
        case kCodec_HEVC_MEDIACODEC:  return "HEVC MEDIACODEC";
        case kCodec_AV1_MEDIACODEC:   return "AV1 MEDIACODEC";

        case kCodec_MPEG1_MEDIACODEC_DEC: return "MPEG1 MEDIACODEC DEC";
        case kCodec_MPEG2_MEDIACODEC_DEC: return "MPEG2 MEDIACODEC DEC";
        case kCodec_H263_MEDIACODEC_DEC:  return "H.263 MEDIACODEC DEC";
        case kCodec_MPEG4_MEDIACODEC_DEC: return "MPEG4 MEDIACODEC DEC";
        case kCodec_H264_MEDIACODEC_DEC:  return "H.264 MEDIACODEC DEC";
        case kCodec_VC1_MEDIACODEC_DEC:   return "VC1 MEDIACODEC DEC";
        case kCodec_WMV3_MEDIACODEC_DEC:  return "WMV3 MEDIACODEC DEC";
        case kCodec_VP8_MEDIACODEC_DEC:   return "VP8 MEDIACODEC DEC";
        case kCodec_VP9_MEDIACODEC_DEC:   return "VP9 MEDIACODEC DEC";
        case kCodec_HEVC_MEDIACODEC_DEC:  return "HEVC MEDIACODEC DEC";
        case kCodec_AV1_MEDIACODEC_DEC:   return "AV1 MEDIACODEC DEC";

        case kCodec_MPEG1_NVDEC: return "MPEG1 NVDEC";
        case kCodec_MPEG2_NVDEC: return "MPEG2 NVDEC";
        case kCodec_H263_NVDEC:  return "H.263 NVDEC";
        case kCodec_MPEG4_NVDEC: return "MPEG4 NVDEC";
        case kCodec_H264_NVDEC:  return "H.264 NVDEC";
        case kCodec_VC1_NVDEC:   return "VC1 NVDEC";
        case kCodec_WMV3_NVDEC:  return "WMV3 NVDEC";
        case kCodec_VP8_NVDEC:   return "VP8 NVDEC";
        case kCodec_VP9_NVDEC:   return "VP9 NVDEC";
        case kCodec_HEVC_NVDEC:  return "HEVC NVDEC";
        case kCodec_AV1_NVDEC:   return "AV1 NVDEC";

        case kCodec_MPEG1_NVDEC_DEC: return "MPEG1 NVDEC DEC";
        case kCodec_MPEG2_NVDEC_DEC: return "MPEG2 NVDEC DEC";
        case kCodec_H263_NVDEC_DEC:  return "H.263 NVDEC DEC";
        case kCodec_MPEG4_NVDEC_DEC: return "MPEG4 NVDEC DEC";
        case kCodec_H264_NVDEC_DEC:  return "H.264 NVDEC DEC";
        case kCodec_VC1_NVDEC_DEC:   return "VC1 NVDEC DEC";
        case kCodec_WMV3_NVDEC_DEC:  return "WMV3 NVDEC DEC";
        case kCodec_VP8_NVDEC_DEC:   return "VP8 NVDEC DEC";
        case kCodec_VP9_NVDEC_DEC:   return "VP9 NVDEC DEC";
        case kCodec_HEVC_NVDEC_DEC:  return "HEVC NVDEC DEC";
        case kCodec_AV1_NVDEC_DEC:   return "AV1 NVDEC DEC";

        case kCodec_MPEG1_VTB: return "MPEG1 VTB";
        case kCodec_MPEG2_VTB: return "MPEG2 VTB";
        case kCodec_H263_VTB:  return "H.263 VTB";
        case kCodec_MPEG4_VTB: return "MPEG4 VTB";
        case kCodec_H264_VTB:  return "H.264 VTB";
        case kCodec_VC1_VTB:   return "VC1 VTB";
        case kCodec_WMV3_VTB:  return "WMV3 VTB";
        case kCodec_VP8_VTB:   return "VP8 VTB";
        case kCodec_VP9_VTB:   return "VP9 VTB";
        case kCodec_HEVC_VTB:  return "HEVC VTB";
        case kCodec_AV1_VTB:   return "AV1 VTB";

        case kCodec_MPEG1_VTB_DEC: return "MPEG1 VTB DEC";
        case kCodec_MPEG2_VTB_DEC: return "MPEG2 VTB DEC";
        case kCodec_H263_VTB_DEC:  return "H.263 VTB DEC";
        case kCodec_MPEG4_VTB_DEC: return "MPEG4 VTB DEC";
        case kCodec_H264_VTB_DEC:  return "H.264 VTB DEC";
        case kCodec_VC1_VTB_DEC:   return "VC1 VTB DEC";
        case kCodec_WMV3_VTB_DEC:  return "WMV3 VTB DEC";
        case kCodec_VP8_VTB_DEC:   return "VP8 VTB DEC";
        case kCodec_VP9_VTB_DEC:   return "VP9 VTB DEC";
        case kCodec_HEVC_VTB_DEC:  return "HEVC VTB DEC";
        case kCodec_AV1_VTB_DEC:   return "AV1 VTB DEC";

        case kCodec_MPEG1_V4L2: return "MPEG1 V4L2";
        case kCodec_MPEG2_V4L2: return "MPEG2 V4L2";
        case kCodec_H263_V4L2:  return "H.263 V4L2";
        case kCodec_MPEG4_V4L2: return "MPEG4 V4L2";
        case kCodec_H264_V4L2:  return "H.264 V4L2";
        case kCodec_VC1_V4L2:   return "VC1 V4L2";
        case kCodec_WMV3_V4L2:  return "WMV3 V4L2";
        case kCodec_VP8_V4L2:   return "VP8 V4L2";
        case kCodec_VP9_V4L2:   return "VP9 V4L2";
        case kCodec_HEVC_V4L2:  return "HEVC V4L2";
        case kCodec_AV1_V4L2:   return "AV1 V4L2";

        case kCodec_MPEG1_V4L2_DEC: return "MPEG1 V4L2 DEC";
        case kCodec_MPEG2_V4L2_DEC: return "MPEG2 V4L2 DEC";
        case kCodec_H263_V4L2_DEC:  return "H.263 V4L2 DEC";
        case kCodec_MPEG4_V4L2_DEC: return "MPEG4 V4L2 DEC";
        case kCodec_H264_V4L2_DEC:  return "H.264 V4L2 DEC";
        case kCodec_VC1_V4L2_DEC:   return "VC1 V4L2 DEC";
        case kCodec_WMV3_V4L2_DEC:  return "WMV3 V4L2 DEC";
        case kCodec_VP8_V4L2_DEC:   return "VP8 V4L2 DEC";
        case kCodec_VP9_V4L2_DEC:   return "VP9 V4L2 DEC";
        case kCodec_HEVC_V4L2_DEC:  return "HEVC V4L2 DEC";
        case kCodec_AV1_V4L2_DEC:   return "AV1 V4L2 DEC";

        case kCodec_MPEG1_MMAL: return "MPEG1 MMAL";
        case kCodec_MPEG2_MMAL: return "MPEG2 MMAL";
        case kCodec_H263_MMAL:  return "H.263 MMAL";
        case kCodec_MPEG4_MMAL: return "MPEG4 MMAL";
        case kCodec_H264_MMAL:  return "H.264 MMAL";
        case kCodec_VC1_MMAL:   return "VC1 MMAL";
        case kCodec_WMV3_MMAL:  return "WMV3 MMAL";
        case kCodec_VP8_MMAL:   return "VP8 MMAL";
        case kCodec_VP9_MMAL:   return "VP9 MMAL";
        case kCodec_HEVC_MMAL:  return "HEVC MMAL";
        case kCodec_AV1_MMAL:   return "AV1 MMAL";

        case kCodec_MPEG1_MMAL_DEC: return "MPEG1 MMAL DEC";
        case kCodec_MPEG2_MMAL_DEC: return "MPEG2 MMAL DEC";
        case kCodec_H263_MMAL_DEC:  return "H.263 MMAL DEC";
        case kCodec_MPEG4_MMAL_DEC: return "MPEG4 MMAL DEC";
        case kCodec_H264_MMAL_DEC:  return "H.264 MMAL DEC";
        case kCodec_VC1_MMAL_DEC:   return "VC1 MMAL DEC";
        case kCodec_WMV3_MMAL_DEC:  return "WMV3 MMAL DEC";
        case kCodec_VP8_MMAL_DEC:   return "VP8 MMAL DEC";
        case kCodec_VP9_MMAL_DEC:   return "VP9 MMAL DEC";
        case kCodec_HEVC_MMAL_DEC:  return "HEVC MMAL DEC";
        case kCodec_AV1_MMAL_DEC:   return "AV1 MMAL DEC";

        case kCodec_MPEG1_DRMPRIME: return "MPEG1 DRMPRIME";
        case kCodec_MPEG2_DRMPRIME: return "MPEG2 DRMPRIME";
        case kCodec_H263_DRMPRIME:  return "H.263 DRMPRIME";
        case kCodec_MPEG4_DRMPRIME: return "MPEG4 DRMPRIME";
        case kCodec_H264_DRMPRIME:  return "H.264 DRMPRIME";
        case kCodec_VC1_DRMPRIME:   return "VC1 DRMPRIME";
        case kCodec_WMV3_DRMPRIME:  return "WMV3 DRMPRIME";
        case kCodec_VP8_DRMPRIME:   return "VP8 DRMPRIME";
        case kCodec_VP9_DRMPRIME:   return "VP9 DRMPRIME";
        case kCodec_HEVC_DRMPRIME:  return "HEVC DRMPRIME";
        case kCodec_AV1_DRMPRIME:   return "AV1 DRMPRIME";
        default:
            break;
    }

    return QString("Unknown(%1)").arg(codecid);
}

AVCodecID myth2av_codecid(MythCodecID codec_id)
{
    switch (codec_id)
    {
        case kCodec_NONE:
        case kCodec_NUV_MPEG4:
        case kCodec_NUV_RTjpeg: return AV_CODEC_ID_NONE;

        case kCodec_MPEG1:
        case kCodec_MPEG1_VDPAU:
        case kCodec_MPEG1_VDPAU_DEC:
        case kCodec_MPEG1_VAAPI:
        case kCodec_MPEG1_VAAPI_DEC:
        case kCodec_MPEG1_DXVA2:
        case kCodec_MPEG1_MEDIACODEC:
        case kCodec_MPEG1_MEDIACODEC_DEC:
        case kCodec_MPEG1_NVDEC:
        case kCodec_MPEG1_NVDEC_DEC:
        case kCodec_MPEG1_VTB:
        case kCodec_MPEG1_VTB_DEC:
        case kCodec_MPEG1_MMAL:
        case kCodec_MPEG1_MMAL_DEC:
        case kCodec_MPEG1_V4L2:
        case kCodec_MPEG1_DRMPRIME:
        case kCodec_MPEG1_V4L2_DEC: return AV_CODEC_ID_MPEG1VIDEO;

        case kCodec_MPEG2:
        case kCodec_MPEG2_VDPAU:
        case kCodec_MPEG2_VDPAU_DEC:
        case kCodec_MPEG2_VAAPI:
        case kCodec_MPEG2_VAAPI_DEC:
        case kCodec_MPEG2_DXVA2:
        case kCodec_MPEG2_MEDIACODEC:
        case kCodec_MPEG2_MEDIACODEC_DEC:
        case kCodec_MPEG2_NVDEC:
        case kCodec_MPEG2_NVDEC_DEC:
        case kCodec_MPEG2_VTB:
        case kCodec_MPEG2_VTB_DEC:
        case kCodec_MPEG2_MMAL:
        case kCodec_MPEG2_MMAL_DEC:
        case kCodec_MPEG2_V4L2:
        case kCodec_MPEG2_DRMPRIME:
        case kCodec_MPEG2_V4L2_DEC: return AV_CODEC_ID_MPEG2VIDEO;

        case kCodec_H263:
        case kCodec_H263_VDPAU:
        case kCodec_H263_VDPAU_DEC:
        case kCodec_H263_VAAPI:
        case kCodec_H263_VAAPI_DEC:
        case kCodec_H263_DXVA2:
        case kCodec_H263_MEDIACODEC:
        case kCodec_H263_MEDIACODEC_DEC:
        case kCodec_H263_NVDEC:
        case kCodec_H263_NVDEC_DEC:
        case kCodec_H263_VTB:
        case kCodec_H263_VTB_DEC:
        case kCodec_H263_MMAL:
        case kCodec_H263_MMAL_DEC:
        case kCodec_H263_V4L2:
        case kCodec_H263_DRMPRIME:
        case kCodec_H263_V4L2_DEC: return AV_CODEC_ID_H263;

        case kCodec_MPEG4:
        case kCodec_MPEG4_VDPAU:
        case kCodec_MPEG4_VDPAU_DEC:
        case kCodec_MPEG4_VAAPI:
        case kCodec_MPEG4_VAAPI_DEC:
        case kCodec_MPEG4_DXVA2:
        case kCodec_MPEG4_MEDIACODEC:
        case kCodec_MPEG4_MEDIACODEC_DEC:
        case kCodec_MPEG4_NVDEC:
        case kCodec_MPEG4_NVDEC_DEC:
        case kCodec_MPEG4_VTB:
        case kCodec_MPEG4_VTB_DEC:
        case kCodec_MPEG4_MMAL:
        case kCodec_MPEG4_MMAL_DEC:
        case kCodec_MPEG4_V4L2:
        case kCodec_MPEG4_DRMPRIME:
        case kCodec_MPEG4_V4L2_DEC: return AV_CODEC_ID_MPEG4;

        case kCodec_H264:
        case kCodec_H264_VDPAU:
        case kCodec_H264_VDPAU_DEC:
        case kCodec_H264_VAAPI:
        case kCodec_H264_VAAPI_DEC:
        case kCodec_H264_DXVA2:
        case kCodec_H264_MEDIACODEC:
        case kCodec_H264_MEDIACODEC_DEC:
        case kCodec_H264_NVDEC:
        case kCodec_H264_NVDEC_DEC:
        case kCodec_H264_VTB:
        case kCodec_H264_VTB_DEC:
        case kCodec_H264_MMAL:
        case kCodec_H264_MMAL_DEC:
        case kCodec_H264_V4L2:
        case kCodec_H264_DRMPRIME:
        case kCodec_H264_V4L2_DEC: return AV_CODEC_ID_H264;

        case kCodec_VC1:
        case kCodec_VC1_VDPAU:
        case kCodec_VC1_VDPAU_DEC:
        case kCodec_VC1_VAAPI:
        case kCodec_VC1_VAAPI_DEC:
        case kCodec_VC1_DXVA2:
        case kCodec_VC1_MEDIACODEC:
        case kCodec_VC1_MEDIACODEC_DEC:
        case kCodec_VC1_NVDEC:
        case kCodec_VC1_NVDEC_DEC:
        case kCodec_VC1_VTB:
        case kCodec_VC1_VTB_DEC:
        case kCodec_VC1_MMAL:
        case kCodec_VC1_MMAL_DEC:
        case kCodec_VC1_V4L2:
        case kCodec_VC1_DRMPRIME:
        case kCodec_VC1_V4L2_DEC: return AV_CODEC_ID_VC1;

        case kCodec_WMV3:
        case kCodec_WMV3_VDPAU:
        case kCodec_WMV3_VDPAU_DEC:
        case kCodec_WMV3_VAAPI:
        case kCodec_WMV3_VAAPI_DEC:
        case kCodec_WMV3_DXVA2:
        case kCodec_WMV3_MEDIACODEC:
        case kCodec_WMV3_MEDIACODEC_DEC:
        case kCodec_WMV3_NVDEC:
        case kCodec_WMV3_NVDEC_DEC:
        case kCodec_WMV3_VTB:
        case kCodec_WMV3_VTB_DEC:
        case kCodec_WMV3_MMAL:
        case kCodec_WMV3_MMAL_DEC:
        case kCodec_WMV3_V4L2:
        case kCodec_WMV3_DRMPRIME:
        case kCodec_WMV3_V4L2_DEC: return AV_CODEC_ID_WMV3;

        case kCodec_VP8:
        case kCodec_VP8_VDPAU:
        case kCodec_VP8_VDPAU_DEC:
        case kCodec_VP8_VAAPI:
        case kCodec_VP8_VAAPI_DEC:
        case kCodec_VP8_DXVA2:
        case kCodec_VP8_MEDIACODEC:
        case kCodec_VP8_MEDIACODEC_DEC:
        case kCodec_VP8_NVDEC:
        case kCodec_VP8_NVDEC_DEC:
        case kCodec_VP8_VTB:
        case kCodec_VP8_VTB_DEC:
        case kCodec_VP8_MMAL:
        case kCodec_VP8_MMAL_DEC:
        case kCodec_VP8_V4L2:
        case kCodec_VP8_DRMPRIME:
        case kCodec_VP8_V4L2_DEC: return AV_CODEC_ID_VP8;

        case kCodec_VP9:
        case kCodec_VP9_VDPAU:
        case kCodec_VP9_VDPAU_DEC:
        case kCodec_VP9_VAAPI:
        case kCodec_VP9_VAAPI_DEC:
        case kCodec_VP9_DXVA2:
        case kCodec_VP9_MEDIACODEC:
        case kCodec_VP9_MEDIACODEC_DEC:
        case kCodec_VP9_NVDEC:
        case kCodec_VP9_NVDEC_DEC:
        case kCodec_VP9_VTB:
        case kCodec_VP9_VTB_DEC:
        case kCodec_VP9_MMAL:
        case kCodec_VP9_MMAL_DEC:
        case kCodec_VP9_V4L2:
        case kCodec_VP9_DRMPRIME:
        case kCodec_VP9_V4L2_DEC: return AV_CODEC_ID_VP9;

        case kCodec_HEVC:
        case kCodec_HEVC_VDPAU:
        case kCodec_HEVC_VDPAU_DEC:
        case kCodec_HEVC_VAAPI:
        case kCodec_HEVC_VAAPI_DEC:
        case kCodec_HEVC_DXVA2:
        case kCodec_HEVC_MEDIACODEC:
        case kCodec_HEVC_MEDIACODEC_DEC:
        case kCodec_HEVC_NVDEC:
        case kCodec_HEVC_NVDEC_DEC:
        case kCodec_HEVC_VTB:
        case kCodec_HEVC_VTB_DEC:
        case kCodec_HEVC_MMAL:
        case kCodec_HEVC_MMAL_DEC:
        case kCodec_HEVC_V4L2:
        case kCodec_HEVC_DRMPRIME:
        case kCodec_HEVC_V4L2_DEC: return AV_CODEC_ID_HEVC;

        case kCodec_AV1:
        case kCodec_AV1_VDPAU:
        case kCodec_AV1_VDPAU_DEC:
        case kCodec_AV1_VAAPI:
        case kCodec_AV1_VAAPI_DEC:
        case kCodec_AV1_DXVA2:
        case kCodec_AV1_MEDIACODEC:
        case kCodec_AV1_MEDIACODEC_DEC:
        case kCodec_AV1_NVDEC:
        case kCodec_AV1_NVDEC_DEC:
        case kCodec_AV1_VTB:
        case kCodec_AV1_VTB_DEC:
        case kCodec_AV1_MMAL:
        case kCodec_AV1_MMAL_DEC:
        case kCodec_AV1_V4L2:
        case kCodec_AV1_DRMPRIME:
        case kCodec_AV1_V4L2_DEC: return AV_CODEC_ID_AV1;

        case kCodec_NORMAL_END:
        case kCodec_VDPAU_END:
        case kCodec_VDPAU_DEC_END:
        case kCodec_VAAPI_END:
        case kCodec_VAAPI_DEC_END:
        case kCodec_DXVA2_END:
        case kCodec_MEDIACODEC_END:
        case kCodec_MEDIACODEC_DEC_END:
        case kCodec_NVDEC_END:
        case kCodec_NVDEC_DEC_END:
        case kCodec_VTB_END:
        case kCodec_VTB_DEC_END:
        case kCodec_MMAL_END:
        case kCodec_MMAL_DEC_END:
        case kCodec_V4L2_END:
        case kCodec_DRMPRIME_END:
        case kCodec_V4L2_DEC_END: break;
    }
    return AV_CODEC_ID_NONE;
}

uint mpeg_version(AVCodecID codec_id)
{
    switch (codec_id)
    {
        case AV_CODEC_ID_MPEG1VIDEO: return 1;
        case AV_CODEC_ID_MPEG2VIDEO: return 2;
        case AV_CODEC_ID_H263:       return 3;
        case AV_CODEC_ID_MPEG4:      return 4;
        case AV_CODEC_ID_H264:       return 5;
        case AV_CODEC_ID_VC1:        return 6;
        case AV_CODEC_ID_WMV3:       return 7;
        case AV_CODEC_ID_VP8:        return 8;
        case AV_CODEC_ID_VP9:        return 9;
        case AV_CODEC_ID_HEVC:       return 10;
        case AV_CODEC_ID_AV1:        return 11;
        default: break;
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
        case kCodec_MPEG1_VDPAU_DEC:
        case kCodec_MPEG1_VAAPI:
        case kCodec_MPEG1_VAAPI_DEC:
        case kCodec_MPEG1_DXVA2:
        case kCodec_MPEG1_MEDIACODEC:
        case kCodec_MPEG1_MEDIACODEC_DEC:
        case kCodec_MPEG1_NVDEC:
        case kCodec_MPEG1_NVDEC_DEC:
        case kCodec_MPEG1_VTB:
        case kCodec_MPEG1_VTB_DEC:
        case kCodec_MPEG1_V4L2:
        case kCodec_MPEG1_V4L2_DEC:
        case kCodec_MPEG1_MMAL:
        case kCodec_MPEG1_MMAL_DEC:
        case kCodec_MPEG1_DRMPRIME:
        case kCodec_MPEG2:
        case kCodec_MPEG2_VDPAU:
        case kCodec_MPEG2_VDPAU_DEC:
        case kCodec_MPEG2_VAAPI:
        case kCodec_MPEG2_VAAPI_DEC:
        case kCodec_MPEG2_DXVA2:
        case kCodec_MPEG2_MEDIACODEC:
        case kCodec_MPEG2_MEDIACODEC_DEC:
        case kCodec_MPEG2_NVDEC:
        case kCodec_MPEG2_NVDEC_DEC:
        case kCodec_MPEG2_VTB:
        case kCodec_MPEG2_VTB_DEC:
        case kCodec_MPEG2_V4L2:
        case kCodec_MPEG2_V4L2_DEC:
        case kCodec_MPEG2_MMAL:
        case kCodec_MPEG2_MMAL_DEC:
        case kCodec_MPEG2_DRMPRIME:
            return "MPEG-2";

        case kCodec_H263:
        case kCodec_H263_VDPAU:
        case kCodec_H263_VDPAU_DEC:
        case kCodec_H263_VAAPI:
        case kCodec_H263_VAAPI_DEC:
        case kCodec_H263_DXVA2:
        case kCodec_H263_MEDIACODEC:
        case kCodec_H263_MEDIACODEC_DEC:
        case kCodec_H263_NVDEC:
        case kCodec_H263_NVDEC_DEC:
        case kCodec_H263_VTB:
        case kCodec_H263_VTB_DEC:
        case kCodec_H263_V4L2:
        case kCodec_H263_V4L2_DEC:
        case kCodec_H263_MMAL:
        case kCodec_H263_MMAL_DEC:
        case kCodec_H263_DRMPRIME:
            return "H.263";

        case kCodec_NUV_MPEG4:
        case kCodec_MPEG4:
        case kCodec_MPEG4_VDPAU:
        case kCodec_MPEG4_VDPAU_DEC:
        case kCodec_MPEG4_VAAPI:
        case kCodec_MPEG4_VAAPI_DEC:
        case kCodec_MPEG4_DXVA2:
        case kCodec_MPEG4_MEDIACODEC:
        case kCodec_MPEG4_MEDIACODEC_DEC:
        case kCodec_MPEG4_NVDEC:
        case kCodec_MPEG4_NVDEC_DEC:
        case kCodec_MPEG4_VTB:
        case kCodec_MPEG4_VTB_DEC:
        case kCodec_MPEG4_V4L2:
        case kCodec_MPEG4_V4L2_DEC:
        case kCodec_MPEG4_MMAL:
        case kCodec_MPEG4_MMAL_DEC:
        case kCodec_MPEG4_DRMPRIME:
            return "MPEG-4";

        case kCodec_H264:
        case kCodec_H264_VDPAU:
        case kCodec_H264_VDPAU_DEC:
        case kCodec_H264_VAAPI:
        case kCodec_H264_VAAPI_DEC:
        case kCodec_H264_DXVA2:
        case kCodec_H264_MEDIACODEC:
        case kCodec_H264_MEDIACODEC_DEC:
        case kCodec_H264_NVDEC:
        case kCodec_H264_NVDEC_DEC:
        case kCodec_H264_VTB:
        case kCodec_H264_VTB_DEC:
        case kCodec_H264_V4L2:
        case kCodec_H264_V4L2_DEC:
        case kCodec_H264_MMAL:
        case kCodec_H264_MMAL_DEC:
        case kCodec_H264_DRMPRIME:
            return "H.264";

        case kCodec_VC1:
        case kCodec_VC1_VDPAU:
        case kCodec_VC1_VDPAU_DEC:
        case kCodec_VC1_VAAPI:
        case kCodec_VC1_VAAPI_DEC:
        case kCodec_VC1_DXVA2:
        case kCodec_VC1_MEDIACODEC:
        case kCodec_VC1_MEDIACODEC_DEC:
        case kCodec_VC1_NVDEC:
        case kCodec_VC1_NVDEC_DEC:
        case kCodec_VC1_VTB:
        case kCodec_VC1_VTB_DEC:
        case kCodec_VC1_V4L2:
        case kCodec_VC1_V4L2_DEC:
        case kCodec_VC1_MMAL:
        case kCodec_VC1_MMAL_DEC:
        case kCodec_VC1_DRMPRIME:
            return "VC-1";

        case kCodec_WMV3:
        case kCodec_WMV3_VDPAU:
        case kCodec_WMV3_VDPAU_DEC:
        case kCodec_WMV3_VAAPI:
        case kCodec_WMV3_VAAPI_DEC:
        case kCodec_WMV3_DXVA2:
        case kCodec_WMV3_MEDIACODEC:
        case kCodec_WMV3_MEDIACODEC_DEC:
        case kCodec_WMV3_NVDEC:
        case kCodec_WMV3_NVDEC_DEC:
        case kCodec_WMV3_VTB:
        case kCodec_WMV3_VTB_DEC:
        case kCodec_WMV3_V4L2:
        case kCodec_WMV3_V4L2_DEC:
        case kCodec_WMV3_MMAL:
        case kCodec_WMV3_MMAL_DEC:
        case kCodec_WMV3_DRMPRIME:
            return "WMV3";

        case kCodec_VP8:
        case kCodec_VP8_VDPAU:
        case kCodec_VP8_VDPAU_DEC:
        case kCodec_VP8_VAAPI:
        case kCodec_VP8_VAAPI_DEC:
        case kCodec_VP8_DXVA2:
        case kCodec_VP8_MEDIACODEC:
        case kCodec_VP8_MEDIACODEC_DEC:
        case kCodec_VP8_NVDEC:
        case kCodec_VP8_NVDEC_DEC:
        case kCodec_VP8_VTB:
        case kCodec_VP8_VTB_DEC:
        case kCodec_VP8_V4L2:
        case kCodec_VP8_V4L2_DEC:
        case kCodec_VP8_MMAL:
        case kCodec_VP8_MMAL_DEC:
        case kCodec_VP8_DRMPRIME:
            return "VP8";

        case kCodec_VP9:
        case kCodec_VP9_VDPAU:
        case kCodec_VP9_VDPAU_DEC:
        case kCodec_VP9_VAAPI:
        case kCodec_VP9_VAAPI_DEC:
        case kCodec_VP9_DXVA2:
        case kCodec_VP9_MEDIACODEC:
        case kCodec_VP9_MEDIACODEC_DEC:
        case kCodec_VP9_NVDEC:
        case kCodec_VP9_NVDEC_DEC:
        case kCodec_VP9_VTB:
        case kCodec_VP9_VTB_DEC:
        case kCodec_VP9_V4L2:
        case kCodec_VP9_V4L2_DEC:
        case kCodec_VP9_MMAL:
        case kCodec_VP9_MMAL_DEC:
        case kCodec_VP9_DRMPRIME:
            return "VP9";

        case kCodec_HEVC:
        case kCodec_HEVC_VDPAU:
        case kCodec_HEVC_VDPAU_DEC:
        case kCodec_HEVC_VAAPI:
        case kCodec_HEVC_VAAPI_DEC:
        case kCodec_HEVC_DXVA2:
        case kCodec_HEVC_MEDIACODEC:
        case kCodec_HEVC_MEDIACODEC_DEC:
        case kCodec_HEVC_NVDEC:
        case kCodec_HEVC_NVDEC_DEC:
        case kCodec_HEVC_VTB:
        case kCodec_HEVC_VTB_DEC:
        case kCodec_HEVC_V4L2:
        case kCodec_HEVC_V4L2_DEC:
        case kCodec_HEVC_MMAL:
        case kCodec_HEVC_MMAL_DEC:
        case kCodec_HEVC_DRMPRIME:
            return "HEVC";

        case kCodec_AV1:
        case kCodec_AV1_VDPAU:
        case kCodec_AV1_VDPAU_DEC:
        case kCodec_AV1_VAAPI:
        case kCodec_AV1_VAAPI_DEC:
        case kCodec_AV1_DXVA2:
        case kCodec_AV1_MEDIACODEC:
        case kCodec_AV1_MEDIACODEC_DEC:
        case kCodec_AV1_NVDEC:
        case kCodec_AV1_NVDEC_DEC:
        case kCodec_AV1_VTB:
        case kCodec_AV1_VTB_DEC:
        case kCodec_AV1_V4L2:
        case kCodec_AV1_V4L2_DEC:
        case kCodec_AV1_MMAL:
        case kCodec_AV1_MMAL_DEC:
        case kCodec_AV1_DRMPRIME:
            return "AV1";

        case kCodec_NONE:
        case kCodec_NORMAL_END:
        case kCodec_VDPAU_END:
        case kCodec_VDPAU_DEC_END:
        case kCodec_VAAPI_END:
        case kCodec_VAAPI_DEC_END:
        case kCodec_DXVA2_END:
        case kCodec_MEDIACODEC_END:
        case kCodec_MEDIACODEC_DEC_END:
        case kCodec_NVDEC_END:
        case kCodec_NVDEC_DEC_END:
        case kCodec_VTB_END:
        case kCodec_VTB_DEC_END:
        case kCodec_V4L2_END:
        case kCodec_V4L2_DEC_END:
        case kCodec_MMAL_END:
        case kCodec_MMAL_DEC_END:
        case kCodec_DRMPRIME_END:
            return {};
    }

    return {};
}

QString get_decoder_name(MythCodecID codec_id)
{
    if (codec_is_vdpau(codec_id))
        return "vdpau";
    if (codec_is_vdpau_dec(codec_id))
        return "vdpau-dec";
    if (codec_is_vaapi(codec_id))
        return "vaapi";
    if (codec_is_vaapi_dec(codec_id))
        return "vaapi-dec";
    if (codec_is_dxva2(codec_id))
        return "dxva2";
    if (codec_is_mediacodec(codec_id))
        return "mediacodec";
    if (codec_is_mediacodec_dec(codec_id))
        return "mediacodec-dec";
    if (codec_is_nvdec(codec_id))
        return "nvdec";
    if (codec_is_nvdec_dec(codec_id))
        return "nvdec-dec";
    if (codec_is_vtb(codec_id))
        return "vtb";
    if (codec_is_vtb_dec(codec_id))
        return "vtb-dec";
    if (codec_is_v4l2(codec_id))
        return "v4l2";
    if (codec_is_v4l2_dec(codec_id))
        return "v4l2-dec";
    if (codec_is_mmal(codec_id))
        return "mmal";
    if (codec_is_mmal_dec(codec_id))
        return "mmal-dec";
    if (codec_is_drmprime(codec_id))
        return "drmprime";
    return "ffmpeg";
}
