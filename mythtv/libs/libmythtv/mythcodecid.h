#ifndef _MYTH_CODEC_ID_H_
#define _MYTH_CODEC_ID_H_

#include <QString>
extern "C"
{
#include "libavcodec/avcodec.h"
}

typedef enum
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

    kCodec_VDPAU_END,

    kCodec_VAAPI_BEGIN = kCodec_VDPAU_END,

    kCodec_MPEG1_VAAPI,
    kCodec_MPEG2_VAAPI,
    kCodec_H263_VAAPI,
    kCodec_MPEG4_VAAPI,
    kCodec_H264_VAAPI,
    kCodec_VC1_VAAPI,
    kCodec_WMV3_VAAPI,
    kCodec_VP8_VAAPI,

    kCodec_VAAPI_END,

    kCodec_DXVA2_BEGIN = kCodec_VAAPI_END,

    kCodec_MPEG1_DXVA2,
    kCodec_MPEG2_DXVA2,
    kCodec_H263_DXVA2,
    kCodec_MPEG4_DXVA2,
    kCodec_H264_DXVA2,
    kCodec_VC1_DXVA2,
    kCodec_WMV3_DXVA2,
    kCodec_VP8_DXVA2,

    kCodec_DXVA2_END,
} MythCodecID;

// MythCodecID convenience functions
#define codec_is_std(id)      (id < kCodec_NORMAL_END)
#define codec_is_std_mpeg(id) (id == kCodec_MPEG1 || id == kCodec_MPEG2)
#define codec_is_vdpau(id)    ((id > kCodec_VDPAU_BEGIN) &&     \
                               (id < kCodec_VDPAU_END))
#define codec_is_vdpau_hw(id) ((codec_is_vdpau(id) &&           \
                                (id != kCodec_H263_VDPAU) &&    \
                                (id != kCodec_VP8_VDPAU)))
#define codec_is_vaapi(id)    ((id > kCodec_VAAPI_BEGIN) &&     \
                               (id < kCodec_VAAPI_END))
#define codec_is_vaapi_hw(id) (codec_is_vaapi(id) &&            \
                               (id != kCodec_VP8_VAAPI))
#define codec_is_dxva2(id)    ((id > kCodec_DXVA2_BEGIN) &&     \
                               (id < kCodec_DXVA2_END))
#define codec_is_dxva2_hw(id) (codec_is_dxva2(id) &&\
                               ((id == kCodec_H264_DXVA2)  ||   \
                                (id == kCodec_MPEG2_DXVA2) ||   \
                                (id == kCodec_VC1_DXVA2)))

QString get_encoding_type(MythCodecID codecid);
QString get_decoder_name(MythCodecID codec_id);
QString toString(MythCodecID codecid);
int myth2av_codecid(MythCodecID codec_id, bool &vdpau);
inline int myth2av_codecid(MythCodecID codec_id)
{
    bool vdpau;
    return myth2av_codecid(codec_id, vdpau);
}

// AV codec id convenience functions
int mpeg_version(int codec_id);
#define CODEC_IS_H264(id)     (mpeg_version(id) == 5)
#define CODEC_IS_MPEG(id)     (mpeg_version(id) && mpeg_version(id) <= 2)
#define CODEC_IS_FFMPEG_MPEG(id) (CODEC_IS_MPEG(id))
#ifdef USING_VDPAU
#define CODEC_IS_VDPAU(codec) (codec &&\
                               codec->capabilities & CODEC_CAP_HWACCEL_VDPAU)
#else
#define CODEC_IS_VDPAU(codec) (0)
#endif

#ifdef USING_VAAPI
#define CODEC_IS_VAAPI(codec, enc) (codec && IS_VAAPI_PIX_FMT(enc->pix_fmt))
#else
#define CODEC_IS_VAAPI(codec, enc) (0)
#endif

#ifdef USING_DXVA2
#define CODEC_IS_DXVA2(codec, enc) (codec && (enc->pix_fmt == PIX_FMT_DXVA2_VLD))
#else
#define CODEC_IS_DXVA2(codec, enc) (0)
#endif

#define CODEC_IS_HWACCEL(codec, enc) (CODEC_IS_VDPAU(codec)      ||     \
                                      CODEC_IS_VAAPI(codec, enc) ||     \
                                      CODEC_IS_DXVA2(codec, enc))

#endif // _MYTH_CODEC_ID_H_
