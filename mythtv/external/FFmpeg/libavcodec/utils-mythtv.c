/*
 * utils for libavcodec
 * Copyright (c) 2001 Fabrice Bellard.
 * Copyright (c) 2003 Michel Bardiaux for the av_log API
 * Copyright (c) 2002-2004 Michael Niedermayer <michaelni@gmx.at>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
 
/**
 * @file myth_utils.c
 * myth_utils.
 */
 
#include "avcodec.h"

/** \fn codec_id_string(enum CodecID)
 *  returns a human readable string for the CodecID enum.
 */
const char *ff_codec_id_string(enum CodecID codec_id)
{
    switch (codec_id)
    {
        case AV_CODEC_ID_NONE:             return "NONE";
        case AV_CODEC_ID_MPEG1VIDEO:       return "MPEG1VIDEO";
        case AV_CODEC_ID_MPEG2VIDEO:       return "MPEG2VIDEO";
        case AV_CODEC_ID_MPEG2VIDEO_XVMC:  return "MPEG2VIDEO_XVMC";
        case AV_CODEC_ID_H261:             return "H261";
        case AV_CODEC_ID_H263:             return "H263";
        case AV_CODEC_ID_RV10:             return "RV10";
        case AV_CODEC_ID_RV20:             return "RV20";
        case AV_CODEC_ID_MJPEG:            return "MJPEG";
        case AV_CODEC_ID_MJPEGB:           return "MJPEGB";
        case AV_CODEC_ID_LJPEG:            return "LJPEG";
        case AV_CODEC_ID_SP5X:             return "SP5X";
        case AV_CODEC_ID_MPEG4:            return "MPEG4";
        case AV_CODEC_ID_RAWVIDEO:         return "RAWVIDEO";
        case AV_CODEC_ID_MSMPEG4V1:        return "MSMPEG4V1";
        case AV_CODEC_ID_MSMPEG4V2:        return "MSMPEG4V2";
        case AV_CODEC_ID_MSMPEG4V3:        return "MSMPEG4V3";
        case AV_CODEC_ID_WMV1:             return "WMV1";
        case AV_CODEC_ID_WMV2:             return "WMV2";
        case AV_CODEC_ID_H263P:            return "H263P";
        case AV_CODEC_ID_H263I:            return "H263I";
        case AV_CODEC_ID_FLV1:             return "FLV1";
        case AV_CODEC_ID_SVQ1:             return "SVQ1";
        case AV_CODEC_ID_SVQ3:             return "SVQ3";
        case AV_CODEC_ID_DVVIDEO:          return "DVVIDEO";
        case AV_CODEC_ID_HUFFYUV:          return "HUFFYUV";
        case AV_CODEC_ID_CYUV:             return "CYUV";
        case AV_CODEC_ID_H264:             return "H264";
        case AV_CODEC_ID_INDEO3:           return "INDEO3";
        case AV_CODEC_ID_VP3:              return "VP3";
        case AV_CODEC_ID_THEORA:           return "THEORA";
        case AV_CODEC_ID_ASV1:             return "ASV1";
        case AV_CODEC_ID_ASV2:             return "ASV2";
        case AV_CODEC_ID_FFV1:             return "FFV1";
        case AV_CODEC_ID_4XM:              return "4XM";
        case AV_CODEC_ID_VCR1:             return "VCR1";
        case AV_CODEC_ID_CLJR:             return "CLJR";
        case AV_CODEC_ID_MDEC:             return "MDEC";
        case AV_CODEC_ID_ROQ:              return "ROQ";
        case AV_CODEC_ID_INTERPLAY_VIDEO:  return "INTERPLAY_VIDEO";
        case AV_CODEC_ID_XAN_WC3:          return "XAN_WC3";
        case AV_CODEC_ID_XAN_WC4:          return "XAN_WC4";
        case AV_CODEC_ID_RPZA:             return "RPZA";
        case AV_CODEC_ID_CINEPAK:          return "CINEPAK";
        case AV_CODEC_ID_WS_VQA:           return "WS_VQA";
        case AV_CODEC_ID_MSRLE:            return "MSRLE";
        case AV_CODEC_ID_MSVIDEO1:         return "MSVIDEO1";
        case AV_CODEC_ID_IDCIN:            return "IDCIN";
        case AV_CODEC_ID_8BPS:             return "8BPS";
        case AV_CODEC_ID_SMC:              return "SMC";
        case AV_CODEC_ID_FLIC:             return "FLIC";
        case AV_CODEC_ID_TRUEMOTION1:      return "TRUEMOTION1";
        case AV_CODEC_ID_VMDVIDEO:         return "VMDVIDEO";
        case AV_CODEC_ID_MSZH:             return "MSZH";
        case AV_CODEC_ID_ZLIB:             return "ZLIB";
        case AV_CODEC_ID_QTRLE:            return "QTRLE";
        case AV_CODEC_ID_SNOW:             return "SNOW";
        case AV_CODEC_ID_TSCC:             return "TSCC";
        case AV_CODEC_ID_ULTI:             return "ULTI";
        case AV_CODEC_ID_QDRAW:            return "QDRAW";
        case AV_CODEC_ID_VIXL:             return "VIXL";
        case AV_CODEC_ID_QPEG:             return "QPEG";
#if 0
        case AV_CODEC_ID_XVID:             return "XVID";
#endif
        case AV_CODEC_ID_PNG:              return "PNG";
        case AV_CODEC_ID_PPM:              return "PPM";
        case AV_CODEC_ID_PBM:              return "PBM";
        case AV_CODEC_ID_PGM:              return "PGM";
        case AV_CODEC_ID_PGMYUV:           return "PGMYUV";
        case AV_CODEC_ID_PAM:              return "PAM";
        case AV_CODEC_ID_FFVHUFF:          return "FFVHUFF";
        case AV_CODEC_ID_RV30:             return "RV30";
        case AV_CODEC_ID_RV40:             return "RV40";
        case AV_CODEC_ID_VC1:              return "VC1";
        case AV_CODEC_ID_WMV3:             return "WMV3";
        case AV_CODEC_ID_LOCO:             return "LOCO";
        case AV_CODEC_ID_WNV1:             return "WNV1";
        case AV_CODEC_ID_AASC:             return "AASC";
        case AV_CODEC_ID_INDEO2:           return "INDEO2";
        case AV_CODEC_ID_FRAPS:            return "FRAPS";
        case AV_CODEC_ID_TRUEMOTION2:      return "TRUEMOTION2";
        case AV_CODEC_ID_BMP:              return "BMP";
        case AV_CODEC_ID_VP8:              return "VP8";

            /* various pcm "codecs" */
        case AV_CODEC_ID_PCM_S16LE:        return "PCM_S16LE";
        case AV_CODEC_ID_PCM_S16BE:        return "PCM_S16BE";
        case AV_CODEC_ID_PCM_U16LE:        return "PCM_U16LE";
        case AV_CODEC_ID_PCM_U16BE:        return "PCM_U16BE";
        case AV_CODEC_ID_PCM_S8:           return "PCM_S8";
        case AV_CODEC_ID_PCM_U8:           return "PCM_U8";
        case AV_CODEC_ID_PCM_MULAW:        return "PCM_MULAW";
        case AV_CODEC_ID_PCM_ALAW:         return "PCM_ALAW";
        case AV_CODEC_ID_PCM_S32LE:        return "PCM_S32LE";
        case AV_CODEC_ID_PCM_S32BE:        return "PCM_S32BE";
        case AV_CODEC_ID_PCM_U32LE:        return "PCM_U32LE";
        case AV_CODEC_ID_PCM_U32BE:        return "PCM_U32BE";
        case AV_CODEC_ID_PCM_S24LE:        return "PCM_S24LE";
        case AV_CODEC_ID_PCM_S24BE:        return "PCM_S24BE";
        case AV_CODEC_ID_PCM_U24LE:        return "PCM_U24LE";
        case AV_CODEC_ID_PCM_U24BE:        return "PCM_U24BE";
        case AV_CODEC_ID_PCM_S24DAUD:      return "PCM_S24DAUD";
        case AV_CODEC_ID_PCM_DVD:          return "PCM_DVD";
        case AV_CODEC_ID_PCM_BLURAY:       return "PCM_BLURAY";

            /* various adpcm codecs */
        case AV_CODEC_ID_ADPCM_IMA_QT:     return "ADPCM_IMA_QT";
        case AV_CODEC_ID_ADPCM_IMA_WAV:    return "ADPCM_IMA_WAV";
        case AV_CODEC_ID_ADPCM_IMA_DK3:    return "ADPCM_IMA_DK3";
        case AV_CODEC_ID_ADPCM_IMA_DK4:    return "ADPCM_IMA_DK4";
        case AV_CODEC_ID_ADPCM_IMA_WS:     return "ADPCM_IMA_WS";
        case AV_CODEC_ID_ADPCM_IMA_SMJPEG: return "ADPCM_IMA_SMJPEG";
        case AV_CODEC_ID_ADPCM_MS:         return "ADPCM_MS";
        case AV_CODEC_ID_ADPCM_4XM:        return "ADPCM_4XM";
        case AV_CODEC_ID_ADPCM_XA:         return "ADPCM_XA";
        case AV_CODEC_ID_ADPCM_ADX:        return "ADPCM_ADX";
        case AV_CODEC_ID_ADPCM_EA:         return "ADPCM_EA";
        case AV_CODEC_ID_ADPCM_G726:       return "ADPCM_G726";
        case AV_CODEC_ID_ADPCM_CT:         return "ADPCM_CT";
        case AV_CODEC_ID_ADPCM_SWF:        return "ADPCM_SWF";
        case AV_CODEC_ID_ADPCM_YAMAHA:     return "ADPCM_YAMAHA";

            /* AMR */
        case AV_CODEC_ID_AMR_NB:           return "AMR_NB";
        case AV_CODEC_ID_AMR_WB:           return "AMR_WB";

            /* RealAudio codecs*/
        case AV_CODEC_ID_RA_144:           return "RA_144";
        case AV_CODEC_ID_RA_288:           return "RA_288";

            /* various DPCM codecs */
        case AV_CODEC_ID_ROQ_DPCM:         return "ROQ_DPCM";
        case AV_CODEC_ID_INTERPLAY_DPCM:   return "INTERPLAY_DPCM";
        case AV_CODEC_ID_XAN_DPCM:         return "XAN_DPCM";
        case AV_CODEC_ID_SOL_DPCM:         return "SOL_DPCM";

        case AV_CODEC_ID_MP2:              return "MP2";
        case AV_CODEC_ID_MP3:              return "MP3";
        case AV_CODEC_ID_AAC:              return "AAC";
        case AV_CODEC_ID_AAC_LATM:         return "AAC/LATM";
        case AV_CODEC_ID_AC3:              return "AC3";
        case AV_CODEC_ID_DTS:              return "DTS";
        case AV_CODEC_ID_VORBIS:           return "VORBIS";
        case AV_CODEC_ID_DVAUDIO:          return "DVAUDIO";
        case AV_CODEC_ID_WMAV1:            return "WMAV1";
        case AV_CODEC_ID_WMAV2:            return "WMAV2";
        case AV_CODEC_ID_MACE3:            return "MACE3";
        case AV_CODEC_ID_MACE6:            return "MACE6";
        case AV_CODEC_ID_VMDAUDIO:         return "VMDAUDIO";
        case AV_CODEC_ID_SONIC:            return "SONIC";
        case AV_CODEC_ID_SONIC_LS:         return "SONIC_LS";
        case AV_CODEC_ID_FLAC:             return "FLAC";
        case AV_CODEC_ID_MP3ADU:           return "MP3ADU";
        case AV_CODEC_ID_MP3ON4:           return "MP3ON4";
        case AV_CODEC_ID_SHORTEN:          return "SHORTEN";
        case AV_CODEC_ID_ALAC:             return "ALAC";
        case AV_CODEC_ID_WESTWOOD_SND1:    return "WESTWOOD_SND1";
        case AV_CODEC_ID_GSM:              return "GSM";
        case AV_CODEC_ID_QDM2:             return "QDM2";
        case AV_CODEC_ID_MLP:              return "MLP";
        case AV_CODEC_ID_TRUEHD:           return "TRUEHD";
        case AV_CODEC_ID_EAC3:             return "E-AC3";
        case AV_CODEC_ID_WMAPRO:           return "WMAPRO";

            /* subtitle codecs */
        case AV_CODEC_ID_DVD_SUBTITLE:     return "DVD_SUBTITLE";
        case AV_CODEC_ID_DVB_SUBTITLE:     return "DVB_SUBTITLE";
        case AV_CODEC_ID_TEXT:             return "TEXT SUBTITLE";
        case AV_CODEC_ID_XSUB:             return "XSUB SUBTITLE";
        case AV_CODEC_ID_SSA:              return "SSA SUBTITLE";
        case AV_CODEC_ID_HDMV_PGS_SUBTITLE: return "HDMV_PGS_SUBTITLE";
        case AV_CODEC_ID_DVB_TELETEXT:     return "DVB_TELETEXT";

        case AV_CODEC_ID_MPEG2VBI:         return "MPEG2VBI";
        case AV_CODEC_ID_DVB_VBI:          return "DVB_VBI";

        case AV_CODEC_ID_DSMCC_B:          return "DSMCC_B";

        case AV_CODEC_ID_MPEG2TS:          return "MPEG2TS";

            /* Attachment codecs */
        case AV_CODEC_ID_TTF:              return "TTF font";

        case AV_CODEC_ID_PROBE:            return "PROBE";
    }
    return "Unknown Codec ID";
}

/** \fn codec_type_string(enum CodecType)
 *  returns a human readable string for the CodecType enum.
 */
const char *ff_codec_type_string(enum AVMediaType codec_type)
{
    switch (codec_type)
    {
        case AVMEDIA_TYPE_UNKNOWN:       return "Unknown";
        case AVMEDIA_TYPE_VIDEO:         return "Video";
        case AVMEDIA_TYPE_AUDIO:         return "Audio";
        case AVMEDIA_TYPE_DATA:          return "Data";
        case AVMEDIA_TYPE_SUBTITLE:      return "Subtitle";
        case AVMEDIA_TYPE_ATTACHMENT:    return "Attachment";
    }
    return "Invalid Codec Type";
};

