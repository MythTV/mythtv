#ifndef MYTH_CODEC_ID_H
#define MYTH_CODEC_ID_H

#include <cstdint>
#include <QString>
extern "C"
{
#include "libavcodec/avcodec.h"
}

enum MythCodecID : std::uint8_t
{
// if you add anything to this list please update
// myth2av_codecid and get_encoding_type
    kCodec_NONE = 0,

    kCodec_NORMAL_BEGIN = kCodec_NONE,

    kCodec_NUV_RTjpeg =  1,
    kCodec_NUV_MPEG4  =  2,

    kCodec_MPEG1      =  3,
    kCodec_MPEG2      =  4,
    kCodec_H263       =  5,
    kCodec_MPEG4      =  6,
    kCodec_H264       =  7,
    kCodec_VC1        =  8,
    kCodec_WMV3       =  9,
    kCodec_VP8        = 10,
    kCodec_VP9        = 11,
    kCodec_HEVC       = 12,
    kCodec_AV1        = 13,

    kCodec_NORMAL_END = 14,
    
    kCodec_VDPAU_BEGIN = kCodec_NORMAL_END,

    kCodec_MPEG1_VDPAU = 15,
    kCodec_MPEG2_VDPAU = 16,
    kCodec_H263_VDPAU  = 17,
    kCodec_MPEG4_VDPAU = 18,
    kCodec_H264_VDPAU  = 19,
    kCodec_VC1_VDPAU   = 20,
    kCodec_WMV3_VDPAU  = 21,
    kCodec_VP8_VDPAU   = 22,
    kCodec_VP9_VDPAU   = 23,
    kCodec_HEVC_VDPAU  = 24,
    kCodec_AV1_VDPAU   = 25,

    kCodec_VDPAU_END   = 26,

    kCodec_VDPAU_DEC_BEGIN = kCodec_VDPAU_END,

    kCodec_MPEG1_VDPAU_DEC = 27,
    kCodec_MPEG2_VDPAU_DEC = 28,
    kCodec_H263_VDPAU_DEC  = 29,
    kCodec_MPEG4_VDPAU_DEC = 30,
    kCodec_H264_VDPAU_DEC  = 31,
    kCodec_VC1_VDPAU_DEC   = 32,
    kCodec_WMV3_VDPAU_DEC  = 33,
    kCodec_VP8_VDPAU_DEC   = 34,
    kCodec_VP9_VDPAU_DEC   = 35,
    kCodec_HEVC_VDPAU_DEC  = 36,
    kCodec_AV1_VDPAU_DEC   = 37,

    kCodec_VDPAU_DEC_END   = 38,

    kCodec_VAAPI_BEGIN = kCodec_VDPAU_DEC_END,

    kCodec_MPEG1_VAAPI = 39,
    kCodec_MPEG2_VAAPI = 40,
    kCodec_H263_VAAPI  = 41,
    kCodec_MPEG4_VAAPI = 42,
    kCodec_H264_VAAPI  = 43,
    kCodec_VC1_VAAPI   = 44,
    kCodec_WMV3_VAAPI  = 45,
    kCodec_VP8_VAAPI   = 46,
    kCodec_VP9_VAAPI   = 47,
    kCodec_HEVC_VAAPI  = 48,
    kCodec_AV1_VAAPI   = 49,

    kCodec_VAAPI_END   = 50,

    kCodec_VAAPI_DEC_BEGIN = kCodec_VAAPI_END,

    kCodec_MPEG1_VAAPI_DEC = 51,
    kCodec_MPEG2_VAAPI_DEC = 52,
    kCodec_H263_VAAPI_DEC  = 53,
    kCodec_MPEG4_VAAPI_DEC = 54,
    kCodec_H264_VAAPI_DEC  = 55,
    kCodec_VC1_VAAPI_DEC   = 56,
    kCodec_WMV3_VAAPI_DEC  = 57,
    kCodec_VP8_VAAPI_DEC   = 58,
    kCodec_VP9_VAAPI_DEC   = 59,
    kCodec_HEVC_VAAPI_DEC  = 60,
    kCodec_AV1_VAAPI_DEC   = 61,

    kCodec_VAAPI_DEC_END   = 62,

    kCodec_DXVA2_BEGIN = kCodec_VAAPI_DEC_END,

    kCodec_MPEG1_DXVA2 = 63,
    kCodec_MPEG2_DXVA2 = 64,
    kCodec_H263_DXVA2  = 65,
    kCodec_MPEG4_DXVA2 = 66,
    kCodec_H264_DXVA2  = 67,
    kCodec_VC1_DXVA2   = 68,
    kCodec_WMV3_DXVA2  = 69,
    kCodec_VP8_DXVA2   = 70,
    kCodec_VP9_DXVA2   = 71,
    kCodec_HEVC_DXVA2  = 72,
    kCodec_AV1_DXVA2   = 73,

    kCodec_DXVA2_END   = 74,

    kCodec_MEDIACODEC_BEGIN = kCodec_DXVA2_END,

    kCodec_MPEG1_MEDIACODEC = 75,
    kCodec_MPEG2_MEDIACODEC = 76,
    kCodec_H263_MEDIACODEC  = 77,
    kCodec_MPEG4_MEDIACODEC = 78,
    kCodec_H264_MEDIACODEC  = 79,
    kCodec_VC1_MEDIACODEC   = 80,
    kCodec_WMV3_MEDIACODEC  = 81,
    kCodec_VP8_MEDIACODEC   = 82,
    kCodec_VP9_MEDIACODEC   = 83,
    kCodec_HEVC_MEDIACODEC  = 84,
    kCodec_AV1_MEDIACODEC   = 85,

    kCodec_MEDIACODEC_END   = 86,

    kCodec_MEDIACODEC_DEC_BEGIN = kCodec_MEDIACODEC_END,

    kCodec_MPEG1_MEDIACODEC_DEC = 87,
    kCodec_MPEG2_MEDIACODEC_DEC = 88,
    kCodec_H263_MEDIACODEC_DEC  = 89,
    kCodec_MPEG4_MEDIACODEC_DEC = 90,
    kCodec_H264_MEDIACODEC_DEC  = 91,
    kCodec_VC1_MEDIACODEC_DEC   = 92,
    kCodec_WMV3_MEDIACODEC_DEC  = 93,
    kCodec_VP8_MEDIACODEC_DEC   = 94,
    kCodec_VP9_MEDIACODEC_DEC   = 95,
    kCodec_HEVC_MEDIACODEC_DEC  = 96,
    kCodec_AV1_MEDIACODEC_DEC   = 97,

    kCodec_MEDIACODEC_DEC_END   = 98,

    kCodec_NVDEC_BEGIN = kCodec_MEDIACODEC_DEC_END,

    kCodec_MPEG1_NVDEC =  99,
    kCodec_MPEG2_NVDEC = 100,
    kCodec_H263_NVDEC  = 101,
    kCodec_MPEG4_NVDEC = 102,
    kCodec_H264_NVDEC  = 103,
    kCodec_VC1_NVDEC   = 104,
    kCodec_WMV3_NVDEC  = 105,
    kCodec_VP8_NVDEC   = 106,
    kCodec_VP9_NVDEC   = 107,
    kCodec_HEVC_NVDEC  = 108,
    kCodec_AV1_NVDEC   = 109,

    kCodec_NVDEC_END   = 110,

    kCodec_NVDEC_DEC_BEGIN = kCodec_NVDEC_END,

    kCodec_MPEG1_NVDEC_DEC = 111,
    kCodec_MPEG2_NVDEC_DEC = 112,
    kCodec_H263_NVDEC_DEC  = 113,
    kCodec_MPEG4_NVDEC_DEC = 114,
    kCodec_H264_NVDEC_DEC  = 115,
    kCodec_VC1_NVDEC_DEC   = 116,
    kCodec_WMV3_NVDEC_DEC  = 117,
    kCodec_VP8_NVDEC_DEC   = 118,
    kCodec_VP9_NVDEC_DEC   = 119,
    kCodec_HEVC_NVDEC_DEC  = 120,
    kCodec_AV1_NVDEC_DEC   = 121,

    kCodec_NVDEC_DEC_END   = 122,

    kCodec_VTB_BEGIN = kCodec_NVDEC_DEC_END,

    kCodec_MPEG1_VTB = 123,
    kCodec_MPEG2_VTB = 124,
    kCodec_H263_VTB  = 125,
    kCodec_MPEG4_VTB = 126,
    kCodec_H264_VTB  = 127,
    kCodec_VC1_VTB   = 128,
    kCodec_WMV3_VTB  = 129,
    kCodec_VP8_VTB   = 130,
    kCodec_VP9_VTB   = 131,
    kCodec_HEVC_VTB  = 132,
    kCodec_AV1_VTB   = 133,

    kCodec_VTB_END   = 134,

    kCodec_VTB_DEC_BEGIN = kCodec_VTB_END,

    kCodec_MPEG1_VTB_DEC = 135,
    kCodec_MPEG2_VTB_DEC = 136,
    kCodec_H263_VTB_DEC  = 137,
    kCodec_MPEG4_VTB_DEC = 138,
    kCodec_H264_VTB_DEC  = 139,
    kCodec_VC1_VTB_DEC   = 140,
    kCodec_WMV3_VTB_DEC  = 141,
    kCodec_VP8_VTB_DEC   = 142,
    kCodec_VP9_VTB_DEC   = 143,
    kCodec_HEVC_VTB_DEC  = 144,
    kCodec_AV1_VTB_DEC   = 145,

    kCodec_VTB_DEC_END   = 146,

    kCodec_V4L2_BEGIN = kCodec_VTB_DEC_END,

    kCodec_MPEG1_V4L2 = 147,
    kCodec_MPEG2_V4L2 = 148,
    kCodec_H263_V4L2  = 149,
    kCodec_MPEG4_V4L2 = 150,
    kCodec_H264_V4L2  = 151,
    kCodec_VC1_V4L2   = 152,
    kCodec_WMV3_V4L2  = 153,
    kCodec_VP8_V4L2   = 154,
    kCodec_VP9_V4L2   = 155,
    kCodec_HEVC_V4L2  = 156,
    kCodec_AV1_V4L2   = 157,

    kCodec_V4L2_END   = 158,

    kCodec_V4L2_DEC_BEGIN = kCodec_V4L2_END,

    kCodec_MPEG1_V4L2_DEC = 159,
    kCodec_MPEG2_V4L2_DEC = 160,
    kCodec_H263_V4L2_DEC  = 161,
    kCodec_MPEG4_V4L2_DEC = 162,
    kCodec_H264_V4L2_DEC  = 163,
    kCodec_VC1_V4L2_DEC   = 164,
    kCodec_WMV3_V4L2_DEC  = 165,
    kCodec_VP8_V4L2_DEC   = 166,
    kCodec_VP9_V4L2_DEC   = 167,
    kCodec_HEVC_V4L2_DEC  = 168,
    kCodec_AV1_V4L2_DEC   = 169,

    kCodec_V4L2_DEC_END   = 170,

    kCodec_MMAL_BEGIN = kCodec_V4L2_DEC_END,

    kCodec_MPEG1_MMAL = 171,
    kCodec_MPEG2_MMAL = 172,
    kCodec_H263_MMAL  = 173,
    kCodec_MPEG4_MMAL = 174,
    kCodec_H264_MMAL  = 175,
    kCodec_VC1_MMAL   = 176,
    kCodec_WMV3_MMAL  = 177,
    kCodec_VP8_MMAL   = 178,
    kCodec_VP9_MMAL   = 179,
    kCodec_HEVC_MMAL  = 180,
    kCodec_AV1_MMAL   = 181,

    kCodec_MMAL_END   = 182,

    kCodec_MMAL_DEC_BEGIN = kCodec_MMAL_END,

    kCodec_MPEG1_MMAL_DEC = 183,
    kCodec_MPEG2_MMAL_DEC = 184,
    kCodec_H263_MMAL_DEC  = 185,
    kCodec_MPEG4_MMAL_DEC = 186,
    kCodec_H264_MMAL_DEC  = 187,
    kCodec_VC1_MMAL_DEC   = 188,
    kCodec_WMV3_MMAL_DEC  = 189,
    kCodec_VP8_MMAL_DEC   = 190,
    kCodec_VP9_MMAL_DEC   = 191,
    kCodec_HEVC_MMAL_DEC  = 192,
    kCodec_AV1_MMAL_DEC   = 193,

    kCodec_MMAL_DEC_END   = 194,

    kCodec_DRMPRIME_BEGIN = kCodec_MMAL_DEC_END,

    kCodec_MPEG1_DRMPRIME = 195,
    kCodec_MPEG2_DRMPRIME = 196,
    kCodec_H263_DRMPRIME  = 197,
    kCodec_MPEG4_DRMPRIME = 198,
    kCodec_H264_DRMPRIME  = 199,
    kCodec_VC1_DRMPRIME   = 200,
    kCodec_WMV3_DRMPRIME  = 201,
    kCodec_VP8_DRMPRIME   = 202,
    kCodec_VP9_DRMPRIME   = 203,
    kCodec_HEVC_DRMPRIME  = 204,
    kCodec_AV1_DRMPRIME   = 205,

    kCodec_DRMPRIME_END   = 206
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
