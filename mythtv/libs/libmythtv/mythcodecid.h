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

    kCodec_NORMAL_END,

    kCodec_STD_XVMC_BEGIN = kCodec_NORMAL_END,

    kCodec_MPEG1_XVMC,
    kCodec_MPEG2_XVMC,
    kCodec_H263_XVMC,
    kCodec_MPEG4_XVMC,
    kCodec_H264_XVMC,

    kCodec_MPEG1_IDCT,
    kCodec_MPEG2_IDCT,
    kCodec_H263_IDCT,
    kCodec_MPEG4_IDCT,
    kCodec_H264_IDCT,

    kCodec_STD_XVMC_END,

    kCodec_VLD_BEGIN = kCodec_STD_XVMC_END,

    kCodec_MPEG1_VLD,
    kCodec_MPEG2_VLD,
    kCodec_H263_VLD,
    kCodec_MPEG4_VLD,
    kCodec_H264_VLD,

    kCodec_VLD_END,
    
    kCodec_VDPAU_BEGIN = kCodec_VLD_END,

    kCodec_MPEG1_VDPAU,
    kCodec_MPEG2_VDPAU,
    kCodec_H263_VDPAU,
    kCodec_MPEG4_VDPAU,
    kCodec_H264_VDPAU,
    kCodec_VC1_VDPAU,
    kCodec_WMV3_VDPAU,

    kCodec_VDPAU_END,

    kCodec_VAAPI_BEGIN = kCodec_VDPAU_END,

    kCodec_MPEG1_VAAPI,
    kCodec_MPEG2_VAAPI,
    kCodec_H263_VAAPI,
    kCodec_MPEG4_VAAPI,
    kCodec_H264_VAAPI,
    kCodec_VC1_VAAPI,
    kCodec_WMV3_VAAPI,

    kCodec_VAAPI_END,
} MythCodecID;

// MythCodecID convenience functions
#define codec_is_std(id)      (id < kCodec_NORMAL_END)
#define codec_is_std_mpeg(id) (id == kCodec_MPEG1 || id == kCodec_MPEG2)
#define codec_is_xvmc_std(id) (id > kCodec_STD_XVMC_BEGIN) &&\
                              (id < kCodec_STD_XVMC_END)
#define codec_is_xvmc_vld(id) (id > kCodec_VLD_BEGIN) &&\
                              (id < kCodec_VLD_END)
#define codec_is_xvmc(id)     (id > kCodec_STD_XVMC_BEGIN) &&\
                              (id < kCodec_VLD_END)
#define codec_is_vdpau(id)    (id > kCodec_VDPAU_BEGIN) &&\
                              (id < kCodec_VDPAU_END)
#define codec_is_vdpau_hw(id) (codec_is_vdpau(id) &&\
                              (id != kCodec_H263_VDPAU))
#define codec_is_vaapi(id)    (id > kCodec_VAAPI_BEGIN) &&\
                              (id < kCodec_VAAPI_END)

QString get_encoding_type(MythCodecID codecid);
QString get_decoder_name(MythCodecID codec_id);
QString toString(MythCodecID codecid);
int myth2av_codecid(MythCodecID codec_id, bool &vld, bool &idct, bool &mc,
                    bool &vdpau);
inline int myth2av_codecid(MythCodecID codec_id)
{
    bool vld, idct, mc, vdpau;
    return myth2av_codecid(codec_id, vld, idct, mc, vdpau);
}

// AV codec id convenience functions
int mpeg_version(int codec_id);
#define CODEC_IS_H264(id)     (mpeg_version(id) == 5)
#define CODEC_IS_MPEG(id)     (mpeg_version(id) && mpeg_version(id) <= 2)
#define CODEC_IS_FFMPEG_MPEG(id) (CODEC_IS_MPEG(id))
#define CODEC_IS_XVMC(codec) (codec && (codec->id == CODEC_ID_MPEG2VIDEO_XVMC ||\
                              codec->id == CODEC_ID_MPEG2VIDEO_XVMC_VLD))
#define CODEC_IS_VDPAU(codec) (codec &&\
                               codec->capabilities & CODEC_CAP_HWACCEL_VDPAU)
#define CODEC_IS_HWACCEL(codec)  (CODEC_IS_XVMC(codec)  ||\
                                  CODEC_IS_VDPAU(codec))

#endif // _MYTH_CODEC_ID_H_
