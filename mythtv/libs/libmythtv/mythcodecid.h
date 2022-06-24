#ifndef MYTH_CODEC_ID_H
#define MYTH_CODEC_ID_H

#include <QString>
extern "C"
{
#include "libavcodec/avcodec.h"
}

enum MythCodecID
{
// if you add anything to this list please update
// myth2av_codecid and get_encoding_type
    kCodec_NONE = 0,

    kCodec_NORMAL_BEGIN = kCodec_NONE,

    kCodec_NUV_RTjpeg,
    kCodec_NUV_MPEG4,

    kCodec_MPEG1,
    kCodec_MPEG2,
    kCodec_H263,
    kCodec_MPEG4,
    kCodec_H264,
    kCodec_VC1,
    kCodec_WMV3,
    kCodec_VP8,
    kCodec_VP9,
    kCodec_HEVC,
    kCodec_AV1,

    kCodec_NORMAL_END,
    
    kCodec_VDPAU_BEGIN = kCodec_NORMAL_END,

    kCodec_MPEG1_VDPAU,
    kCodec_MPEG2_VDPAU,
    kCodec_H263_VDPAU,
    kCodec_MPEG4_VDPAU,
    kCodec_H264_VDPAU,
    kCodec_VC1_VDPAU,
    kCodec_WMV3_VDPAU,
    kCodec_VP8_VDPAU,
    kCodec_VP9_VDPAU,
    kCodec_HEVC_VDPAU,
    kCodec_AV1_VDPAU,

    kCodec_VDPAU_END,

    kCodec_VDPAU_DEC_BEGIN = kCodec_VDPAU_END,

    kCodec_MPEG1_VDPAU_DEC,
    kCodec_MPEG2_VDPAU_DEC,
    kCodec_H263_VDPAU_DEC,
    kCodec_MPEG4_VDPAU_DEC,
    kCodec_H264_VDPAU_DEC,
    kCodec_VC1_VDPAU_DEC,
    kCodec_WMV3_VDPAU_DEC,
    kCodec_VP8_VDPAU_DEC,
    kCodec_VP9_VDPAU_DEC,
    kCodec_HEVC_VDPAU_DEC,
    kCodec_AV1_VDPAU_DEC,

    kCodec_VDPAU_DEC_END,

    kCodec_VAAPI_BEGIN = kCodec_VDPAU_DEC_END,

    kCodec_MPEG1_VAAPI,
    kCodec_MPEG2_VAAPI,
    kCodec_H263_VAAPI,
    kCodec_MPEG4_VAAPI,
    kCodec_H264_VAAPI,
    kCodec_VC1_VAAPI,
    kCodec_WMV3_VAAPI,
    kCodec_VP8_VAAPI,
    kCodec_VP9_VAAPI,
    kCodec_HEVC_VAAPI,
    kCodec_AV1_VAAPI,

    kCodec_VAAPI_END,

    kCodec_VAAPI_DEC_BEGIN = kCodec_VAAPI_END,

    kCodec_MPEG1_VAAPI_DEC,
    kCodec_MPEG2_VAAPI_DEC,
    kCodec_H263_VAAPI_DEC,
    kCodec_MPEG4_VAAPI_DEC,
    kCodec_H264_VAAPI_DEC,
    kCodec_VC1_VAAPI_DEC,
    kCodec_WMV3_VAAPI_DEC,
    kCodec_VP8_VAAPI_DEC,
    kCodec_VP9_VAAPI_DEC,
    kCodec_HEVC_VAAPI_DEC,
    kCodec_AV1_VAAPI_DEC,

    kCodec_VAAPI_DEC_END,

    kCodec_DXVA2_BEGIN = kCodec_VAAPI_DEC_END,

    kCodec_MPEG1_DXVA2,
    kCodec_MPEG2_DXVA2,
    kCodec_H263_DXVA2,
    kCodec_MPEG4_DXVA2,
    kCodec_H264_DXVA2,
    kCodec_VC1_DXVA2,
    kCodec_WMV3_DXVA2,
    kCodec_VP8_DXVA2,
    kCodec_VP9_DXVA2,
    kCodec_HEVC_DXVA2,
    kCodec_AV1_DXVA2,

    kCodec_DXVA2_END,

    kCodec_MEDIACODEC_BEGIN = kCodec_DXVA2_END,

    kCodec_MPEG1_MEDIACODEC,
    kCodec_MPEG2_MEDIACODEC,
    kCodec_H263_MEDIACODEC,
    kCodec_MPEG4_MEDIACODEC,
    kCodec_H264_MEDIACODEC,
    kCodec_VC1_MEDIACODEC,
    kCodec_WMV3_MEDIACODEC,
    kCodec_VP8_MEDIACODEC,
    kCodec_VP9_MEDIACODEC,
    kCodec_HEVC_MEDIACODEC,
    kCodec_AV1_MEDIACODEC,

    kCodec_MEDIACODEC_END,

    kCodec_MEDIACODEC_DEC_BEGIN = kCodec_MEDIACODEC_END,

    kCodec_MPEG1_MEDIACODEC_DEC,
    kCodec_MPEG2_MEDIACODEC_DEC,
    kCodec_H263_MEDIACODEC_DEC,
    kCodec_MPEG4_MEDIACODEC_DEC,
    kCodec_H264_MEDIACODEC_DEC,
    kCodec_VC1_MEDIACODEC_DEC,
    kCodec_WMV3_MEDIACODEC_DEC,
    kCodec_VP8_MEDIACODEC_DEC,
    kCodec_VP9_MEDIACODEC_DEC,
    kCodec_HEVC_MEDIACODEC_DEC,
    kCodec_AV1_MEDIACODEC_DEC,

    kCodec_MEDIACODEC_DEC_END,

    kCodec_NVDEC_BEGIN = kCodec_MEDIACODEC_DEC_END,

    kCodec_MPEG1_NVDEC,
    kCodec_MPEG2_NVDEC,
    kCodec_H263_NVDEC,
    kCodec_MPEG4_NVDEC,
    kCodec_H264_NVDEC,
    kCodec_VC1_NVDEC,
    kCodec_WMV3_NVDEC,
    kCodec_VP8_NVDEC,
    kCodec_VP9_NVDEC,
    kCodec_HEVC_NVDEC,
    kCodec_AV1_NVDEC,

    kCodec_NVDEC_END,

    kCodec_NVDEC_DEC_BEGIN = kCodec_NVDEC_END,

    kCodec_MPEG1_NVDEC_DEC,
    kCodec_MPEG2_NVDEC_DEC,
    kCodec_H263_NVDEC_DEC,
    kCodec_MPEG4_NVDEC_DEC,
    kCodec_H264_NVDEC_DEC,
    kCodec_VC1_NVDEC_DEC,
    kCodec_WMV3_NVDEC_DEC,
    kCodec_VP8_NVDEC_DEC,
    kCodec_VP9_NVDEC_DEC,
    kCodec_HEVC_NVDEC_DEC,
    kCodec_AV1_NVDEC_DEC,

    kCodec_NVDEC_DEC_END,

    kCodec_VTB_BEGIN = kCodec_NVDEC_DEC_END,

    kCodec_MPEG1_VTB,
    kCodec_MPEG2_VTB,
    kCodec_H263_VTB,
    kCodec_MPEG4_VTB,
    kCodec_H264_VTB,
    kCodec_VC1_VTB,
    kCodec_WMV3_VTB,
    kCodec_VP8_VTB,
    kCodec_VP9_VTB,
    kCodec_HEVC_VTB,
    kCodec_AV1_VTB,

    kCodec_VTB_END,

    kCodec_VTB_DEC_BEGIN = kCodec_VTB_END,

    kCodec_MPEG1_VTB_DEC,
    kCodec_MPEG2_VTB_DEC,
    kCodec_H263_VTB_DEC,
    kCodec_MPEG4_VTB_DEC,
    kCodec_H264_VTB_DEC,
    kCodec_VC1_VTB_DEC,
    kCodec_WMV3_VTB_DEC,
    kCodec_VP8_VTB_DEC,
    kCodec_VP9_VTB_DEC,
    kCodec_HEVC_VTB_DEC,
    kCodec_AV1_VTB_DEC,

    kCodec_VTB_DEC_END,

    kCodec_V4L2_BEGIN = kCodec_VTB_DEC_END,

    kCodec_MPEG1_V4L2,
    kCodec_MPEG2_V4L2,
    kCodec_H263_V4L2,
    kCodec_MPEG4_V4L2,
    kCodec_H264_V4L2,
    kCodec_VC1_V4L2,
    kCodec_WMV3_V4L2,
    kCodec_VP8_V4L2,
    kCodec_VP9_V4L2,
    kCodec_HEVC_V4L2,
    kCodec_AV1_V4L2,

    kCodec_V4L2_END,

    kCodec_V4L2_DEC_BEGIN = kCodec_V4L2_END,

    kCodec_MPEG1_V4L2_DEC,
    kCodec_MPEG2_V4L2_DEC,
    kCodec_H263_V4L2_DEC,
    kCodec_MPEG4_V4L2_DEC,
    kCodec_H264_V4L2_DEC,
    kCodec_VC1_V4L2_DEC,
    kCodec_WMV3_V4L2_DEC,
    kCodec_VP8_V4L2_DEC,
    kCodec_VP9_V4L2_DEC,
    kCodec_HEVC_V4L2_DEC,
    kCodec_AV1_V4L2_DEC,

    kCodec_V4L2_DEC_END,

    kCodec_MMAL_BEGIN = kCodec_V4L2_DEC_END,

    kCodec_MPEG1_MMAL,
    kCodec_MPEG2_MMAL,
    kCodec_H263_MMAL,
    kCodec_MPEG4_MMAL,
    kCodec_H264_MMAL,
    kCodec_VC1_MMAL,
    kCodec_WMV3_MMAL,
    kCodec_VP8_MMAL,
    kCodec_VP9_MMAL,
    kCodec_HEVC_MMAL,
    kCodec_AV1_MMAL,

    kCodec_MMAL_END,

    kCodec_MMAL_DEC_BEGIN = kCodec_MMAL_END,

    kCodec_MPEG1_MMAL_DEC,
    kCodec_MPEG2_MMAL_DEC,
    kCodec_H263_MMAL_DEC,
    kCodec_MPEG4_MMAL_DEC,
    kCodec_H264_MMAL_DEC,
    kCodec_VC1_MMAL_DEC,
    kCodec_WMV3_MMAL_DEC,
    kCodec_VP8_MMAL_DEC,
    kCodec_VP9_MMAL_DEC,
    kCodec_HEVC_MMAL_DEC,
    kCodec_AV1_MMAL_DEC,

    kCodec_MMAL_DEC_END,

    kCodec_DRMPRIME_BEGIN = kCodec_MMAL_DEC_END,

    kCodec_MPEG1_DRMPRIME,
    kCodec_MPEG2_DRMPRIME,
    kCodec_H263_DRMPRIME,
    kCodec_MPEG4_DRMPRIME,
    kCodec_H264_DRMPRIME,
    kCodec_VC1_DRMPRIME,
    kCodec_WMV3_DRMPRIME,
    kCodec_VP8_DRMPRIME,
    kCodec_VP9_DRMPRIME,
    kCodec_HEVC_DRMPRIME,
    kCodec_AV1_DRMPRIME,

    kCodec_DRMPRIME_END
};

// MythCodecID convenience functions
static inline bool codec_is_std(MythCodecID id)
    { return (id < kCodec_NORMAL_END); }
static inline bool codec_is_std_mpeg(MythCodecID id)
    { return ((id == kCodec_MPEG1) || (id == kCodec_MPEG2)); };

static inline bool codec_is_drmprime(MythCodecID id)
    { return ((id > kCodec_DRMPRIME_BEGIN) &&
              (id < kCodec_DRMPRIME_END)); };
static inline bool codec_is_vdpau(MythCodecID id)
    { return ((id > kCodec_VDPAU_BEGIN) &&
              (id < kCodec_VDPAU_END)); };
static inline bool codec_is_vdpau_hw(MythCodecID id)
    { return ((codec_is_vdpau(id) &&
               (id != kCodec_H263_VDPAU) &&
               (id != kCodec_VP8_VDPAU) &&
               (id != kCodec_VP9_VDPAU))); };
static inline bool codec_is_vdpau_dec(MythCodecID id)
    { return ((id > kCodec_VDPAU_DEC_BEGIN) &&
              (id < kCodec_VDPAU_DEC_END)); };
static inline bool codec_is_vdpau_dechw(MythCodecID id)
    { return (codec_is_vdpau_dec(id) &&
              (id != kCodec_H263_VDPAU_DEC) &&
              (id != kCodec_VP8_VDPAU_DEC) &&
              (id != kCodec_VP9_VDPAU)); };

static inline bool codec_is_vaapi(MythCodecID id)
    { return ((id > kCodec_VAAPI_BEGIN) &&
              (id < kCodec_VAAPI_END)); };
static inline bool codec_is_vaapi_dec(MythCodecID id)
    { return ((id > kCodec_VAAPI_DEC_BEGIN) &&
              (id < kCodec_VAAPI_DEC_END)); };

static inline bool codec_is_dxva2(MythCodecID id)
    { return ((id > kCodec_DXVA2_BEGIN) &&
              (id < kCodec_DXVA2_END)); };
static inline bool codec_is_dxva2_hw(MythCodecID id)
    { return (codec_is_dxva2(id) &&
              ((id == kCodec_H264_DXVA2)  ||
               (id == kCodec_MPEG2_DXVA2) ||
               (id == kCodec_VC1_DXVA2))); };

static inline bool codec_is_mediacodec(MythCodecID id)
    { return ((id > kCodec_MEDIACODEC_BEGIN) &&
              (id < kCodec_MEDIACODEC_END)); };
static inline bool codec_is_mediacodec_dec(MythCodecID id)
    { return ((id > kCodec_MEDIACODEC_DEC_BEGIN) &&
              (id < kCodec_MEDIACODEC_DEC_END)); };

static inline bool codec_is_nvdec(MythCodecID id)
    { return ((id > kCodec_NVDEC_BEGIN) &&
              (id < kCodec_NVDEC_END)); };
static inline bool codec_is_nvdec_dec(MythCodecID id)
    { return ((id > kCodec_NVDEC_DEC_BEGIN) &&
              (id < kCodec_NVDEC_DEC_END)); };

static inline bool codec_is_vtb(MythCodecID id)
    { return ((id > kCodec_VTB_BEGIN) &&
              (id < kCodec_VTB_END)); };
static inline bool codec_is_vtb_dec(MythCodecID id)
    { return ((id > kCodec_VTB_DEC_BEGIN) &&
              (id < kCodec_VTB_DEC_END)); };

static inline bool codec_is_v4l2(MythCodecID id)
    { return ((id > kCodec_V4L2_BEGIN) && (id < kCodec_V4L2_END)); };
static inline bool codec_is_v4l2_dec(MythCodecID id)
    { return ((id > kCodec_V4L2_DEC_BEGIN) && (id < kCodec_V4L2_DEC_END)); };

static inline bool codec_is_mmal(MythCodecID id)
    { return ((id > kCodec_MMAL_BEGIN) && (id < kCodec_MMAL_END)); };
static inline bool codec_is_mmal_dec(MythCodecID id)
    { return ((id > kCodec_MMAL_DEC_BEGIN) && (id < kCodec_MMAL_DEC_END)); };

static inline bool codec_is_copyback(MythCodecID id)
    { return (codec_is_mediacodec_dec(id) ||
              codec_is_vaapi_dec(id) || codec_is_nvdec_dec(id) ||
              codec_is_vtb_dec(id) || codec_is_vdpau_dec(id) ||
              codec_is_v4l2_dec(id) || codec_is_mmal_dec(id)); };

static inline bool codec_sw_copy(MythCodecID id)
    { return codec_is_std(id) || codec_is_copyback(id); };

QString get_encoding_type(MythCodecID codecid);
QString get_decoder_name(MythCodecID codec_id);
QString toString(MythCodecID codecid);
AVCodecID myth2av_codecid(MythCodecID codec_id);

// AV codec id convenience functions
uint mpeg_version(AVCodecID codec_id);
static inline bool CODEC_IS_H264(AVCodecID id)
    { return mpeg_version(id) == 5; };
static inline bool CODEC_IS_MPEG(AVCodecID id)
    { return (mpeg_version(id) != 0) && (mpeg_version(id) <= 2); };
#ifdef USING_VDPAU
static inline bool CODEC_IS_VDPAU(const struct AVCodec *codec, const AVCodecContext *enc)
    { return (codec != nullptr) && (enc->pix_fmt == AV_PIX_FMT_VDPAU); };
#else
static inline bool CODEC_IS_VDPAU(const struct AVCodec */*codec*/)
    { return false; };
#endif

#ifdef USING_VAAPI
static inline bool CODEC_IS_VAAPI(const struct AVCodec *codec, const AVCodecContext *enc)
    { return (codec != nullptr) && (enc->pix_fmt == AV_PIX_FMT_VAAPI); };
#else
static inline bool CODEC_IS_VAAPI(const struct AVCodec */*codec*/, const AVCodecContext */*enc*/)
    { return false; };
#endif

#ifdef USING_DXVA2
static inline bool CODEC_IS_DXVA2(const struct AVCodec *codec, const AVCodecContext *enc)
    { return (codec  != nullptr) && (enc->pix_fmt == AV_PIX_FMT_DXVA2_VLD); };
#else
static inline bool CODEC_IS_DXVA2(const struct AVCodec */*codec*/, const AVCodecContext */*enc*/)
    { return false; };
#endif

#ifdef USING_MEDIACODEC
static inline bool CODEC_IS_MEDIACODEC(const struct AVCodec *codec)
    { return (codec != nullptr) && (QString("mediacodec") == codec->wrapper_name); };
#else
static inline bool CODEC_IS_MEDIACODEC(const struct AVCodec */*codec*/)
    { return false; };
#endif

#endif // MYTH_CODEC_ID_H
