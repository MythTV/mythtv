/*
 * AAC LATM decoder
 * Copyright (c) 2008-2010 Paul Kendall <paul@kcbbs.gen.nz>
 * Copyright (c) 2010      Janne Grunau <janne-ffmpeg@jannau.net>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * AAC LATM decoder
 * @author Paul Kendall <paul@kcbbs.gen.nz>
 * @author Janne Grunau <janne-ffmpeg@jannau.net>
 */

/*
    Note: This decoder filter is intended to decode LATM streams transferred
    in MPEG transport streams which only contain one program.
    To do a more complex LATM demuxing a separate LATM demuxer should be used.
*/

#include "get_bits.h"
#include "dsputil.h"

#include "aac.h"
#include "aacdectab.h"
#include "mpeg4audio.h"

#include <assert.h>

#define LOAS_SYNC_WORD   0x2b7       // 11 bits
#define MAX_SIZE         8*1024

struct LATMContext
{
    AACContext      aac_ctx;
    AVCodec        *aac_codec;
    uint8_t         initialized;

    // parser data
    uint8_t         audio_mux_version_A;
    uint8_t         same_time_framing;
    uint8_t         frame_length_type;
    uint32_t        frame_length;
};

static inline int64_t latm_get_value(GetBitContext *b)
{
    uint8_t bytesForValue = get_bits(b, 2);
    int64_t value = 0;
    int i;
    for (i=0; i<=bytesForValue; i++) {
        value <<= 8;
        value |= get_bits(b, 8);
    }
    return value;
}

// copied from libavcodec/mpeg4audio.c
static av_always_inline unsigned int copy_bits(PutBitContext *pb,
                                               GetBitContext *gb, int bits)
{
    unsigned int el = get_bits(gb, bits);
    put_bits(pb, bits, el);
    return el;
}

static void latm_read_ga_specific_config(int audio_object_type,
                                         MPEG4AudioConfig *c,
                                         GetBitContext *gb, PutBitContext *pb)
{
    int ext_flag;

    copy_bits(pb, gb, 1);                        // framelen_flag
    if (copy_bits(pb, gb, 1))                    // depends_on_coder
        copy_bits(pb, gb, 14);                   // delay
    ext_flag = copy_bits(pb, gb, 1);

    if (!c->chan_config)
        ff_copy_pce_data(pb, gb);                // program_config_element

    if (audio_object_type == AOT_AAC_SCALABLE ||
        audio_object_type == AOT_ER_AAC_SCALABLE)
        copy_bits(pb, gb, 3);                    // layer number

    if (!ext_flag)
        return;

    if (audio_object_type == AOT_ER_BSAC) {
        copy_bits(pb, gb, 5);                    // numOfSubFrame
        copy_bits(pb, gb, 11);                   // layer_length
    } else if (audio_object_type == AOT_ER_AAC_LC ||
               audio_object_type == AOT_ER_AAC_LTP ||
               audio_object_type == AOT_ER_AAC_SCALABLE ||
               audio_object_type == AOT_ER_AAC_LD)
        copy_bits(pb, gb, 3);                    // stuff
    copy_bits(pb, gb, 1);                        // extflag3
}

static int latm_read_audio_specific_config(GetBitContext *gb,
                                           PutBitContext *pb)
{
    int num_bits=0;
    int audio_object_type;

    MPEG4AudioConfig b, *c;
    c = &b;

    c->sbr = -1;

    audio_object_type = copy_bits(pb, gb, 5);
    if (audio_object_type == AOT_ESCAPE) {
        audio_object_type = AOT_ESCAPE + copy_bits(pb, gb, 6) + 1;
    }
    c->object_type = audio_object_type;

    c->sampling_index = copy_bits(pb, gb, 4);
    c->sample_rate = ff_mpeg4audio_sample_rates[c->sampling_index];
    if (c->sampling_index == 0x0f) {
        c->sample_rate = copy_bits(pb, gb, 24);
    }
    c->chan_config = copy_bits(pb, gb, 4);

    if (c->chan_config < FF_ARRAY_ELEMS(ff_mpeg4audio_channels))
        c->channels = ff_mpeg4audio_channels[c->chan_config];

    if (audio_object_type == AOT_AAC_MAIN ||
        audio_object_type == AOT_AAC_LC ||
        audio_object_type == AOT_AAC_SSR ||
        audio_object_type == AOT_AAC_LTP ||
        audio_object_type == AOT_AAC_SCALABLE ||
        audio_object_type == AOT_TWINVQ) {
        latm_read_ga_specific_config(audio_object_type, c, gb, pb);
    } else if (audio_object_type == AOT_SBR) {
        c->sbr = 1;
        c->ext_sampling_index = copy_bits(pb, gb, 4);
        c->ext_sample_rate = ff_mpeg4audio_sample_rates[c->ext_sampling_index];
        if (c->ext_sampling_index == 0x0f) {
            c->ext_sample_rate = copy_bits(pb, gb, 24);
        }
        c->object_type = copy_bits(pb, gb, 5);
    } else if (audio_object_type >= AOT_ER_AAC_LC) {
        latm_read_ga_specific_config(audio_object_type, c, gb, pb);
        copy_bits(pb, gb, 2);                   // epConfig
    }

    if (c->sbr == -1 && c->sample_rate <= 24000)
        c->sample_rate *= 2;

    // count the extradata
    num_bits = put_bits_count(pb);

    flush_put_bits(pb);
    return num_bits;
}

static int latm_decode_audio_specific_config(struct LATMContext *latmctx,
                                             GetBitContext *gb)
{
    PutBitContext pb;
    int32_t esize, bits_consumed;
    uint8_t extradata[32+FF_INPUT_BUFFER_PADDING_SIZE];
    AVCodecContext *avctx = latmctx->aac_ctx.avctx;

    init_put_bits(&pb, extradata, 32 * 8);

    bits_consumed = latm_read_audio_specific_config(gb, &pb);

    if (bits_consumed < 0)
        return AVERROR_INVALIDDATA;

    esize = (bits_consumed+7) / 8;

    if (avctx->extradata_size != esize) {
        av_free(avctx->extradata);
        avctx->extradata = av_malloc(esize + FF_INPUT_BUFFER_PADDING_SIZE);
        if (!avctx->extradata)
            return AVERROR(ENOMEM);

        avctx->extradata_size = esize;
        memcpy(avctx->extradata, extradata, esize);
        memset(avctx->extradata+esize, 0, FF_INPUT_BUFFER_PADDING_SIZE);
    }

    return bits_consumed;
}

static int read_stream_mux_config(struct LATMContext *latmctx,
                                  GetBitContext *gb)
{
    int ret, audio_mux_version = get_bits(gb, 1);

    latmctx->audio_mux_version_A = 0;
    if (audio_mux_version)
        latmctx->audio_mux_version_A = get_bits(gb, 1);

    if (!latmctx->audio_mux_version_A) {

        if (audio_mux_version)
            latm_get_value(gb);                 // taraFullness

        skip_bits(gb, 1);                       // allStreamSameTimeFraming
        skip_bits(gb, 6);                       // numSubFrames
        // numPrograms
        if (get_bits(gb, 4)) {                  // numPrograms
            av_log_missing_feature(latmctx->aac_ctx.avctx,
                                   "multiple programs are not supported\n", 1);
            return AVERROR_PATCHWELCOME;
        }

        // for each program (which there is only on in DVB)

        // for each layer (which there is only on in DVB)
        if (get_bits(gb, 3)) {                   // numLayer
            av_log_missing_feature(latmctx->aac_ctx.avctx,
                                   "multiple layers are not supported\n", 1);
            return AVERROR_PATCHWELCOME;
        }

        // for all but first stream: use_same_config = get_bits(gb, 1);
        if (!audio_mux_version) {
            ret = latm_decode_audio_specific_config(latmctx, gb);
            if (ret < 0)
                return ret;
        } else {
            int ascLen = latm_get_value(gb);
            ret = latm_decode_audio_specific_config(latmctx, gb);
            if (ret < 0)
                return ret;
            ascLen -= ret;
            skip_bits_long(gb, ascLen);
        }

        latmctx->frame_length_type = get_bits(gb, 3);
        switch (latmctx->frame_length_type) {
            case 0:
                skip_bits(gb, 8);       // latmBufferFullness
                break;
            case 1:
                latmctx->frame_length = get_bits(gb, 9);
                break;
            case 3:
            case 4:
            case 5:
                skip_bits(gb, 6);       // CELP frame length table index
                break;
            case 6:
            case 7:
                skip_bits(gb, 1);       // HVXC frame length table index
                break;
        }

        if (get_bits(gb, 1)) {                  // other data
            if (audio_mux_version) {
                latm_get_value(gb);             // other_data_bits
            } else {
                int esc;
                do {
                    esc = get_bits(gb, 1);
                    skip_bits(gb, 8);
                } while (esc);
            }
        }

        if (get_bits(gb, 1))                     // crc present
            skip_bits(gb, 8);                    // config_crc
    }

    return 0;
}

static int read_payload_length_info(struct LATMContext *ctx, GetBitContext *gb)
{
    uint8_t tmp;

    if (ctx->frame_length_type == 0) {
        int mux_slot_length = 0;
        do {
            tmp = get_bits(gb, 8);
            mux_slot_length += tmp;
        } while (tmp == 255);
        return mux_slot_length;
    } else if (ctx->frame_length_type == 1) {
        return ctx->frame_length;
    } else if (ctx->frame_length_type == 3 ||
               ctx->frame_length_type == 5 ||
               ctx->frame_length_type == 7) {
        skip_bits(gb, 2);          // mux_slot_length_coded
    }
    return 0;
}

static int read_audio_mux_element(struct LATMContext *latmctx,
                                  GetBitContext *b,
                                  uint8_t *payload, int *payloadsize)
{
    uint8_t use_same_mux = get_bits(b, 1);
    if (!use_same_mux) {
        read_stream_mux_config(latmctx, b);
    } else if (!latmctx->aac_ctx.avctx->extradata) {
        av_log(latmctx->aac_ctx.avctx, AV_LOG_DEBUG,
               "no decoder config found\n");
        return AVERROR(EAGAIN);
    }
    if (latmctx->audio_mux_version_A == 0) {
        int j;
        int mux_slot_length_bytes = read_payload_length_info(latmctx, b);
        mux_slot_length_bytes = FFMIN(mux_slot_length_bytes, *payloadsize);
        for (j=0; j<mux_slot_length_bytes; j++) {
            *payload++ = get_bits(b, 8);
        }
        *payloadsize = mux_slot_length_bytes;
    }
    return 0;
}

static int read_audio_sync_stream(struct LATMContext *latmctx,
                                  GetBitContext *gb, int size,
                                  uint8_t *payload, int *payloadsize)
{
    int muxlength;

    if (get_bits(gb, 11) != LOAS_SYNC_WORD)
        return AVERROR_INVALIDDATA;

    muxlength = get_bits(gb, 13);
    // not enough data, the parser should have sorted this
    if (muxlength+3 > size)
        return AVERROR_INVALIDDATA;

    read_audio_mux_element(latmctx, gb, payload, payloadsize);

    return 0;
}


static int latm_decode_frame(AVCodecContext *avctx, void *out, int *out_size,
                             AVPacket *avpkt)
{
    struct LATMContext *latmctx = avctx->priv_data;
    uint8_t            *tmp, tmpbuf[MAX_SIZE];
    int                 ret, bufsize = MAX_SIZE;
    GetBitContext       gb;

    if(avpkt->size == 0)
        return 0;

    init_get_bits(&gb, avpkt->data, avpkt->size * 8);

    ret = read_audio_sync_stream(latmctx, &gb, avpkt->size, tmpbuf, &bufsize);
    if (ret < 0)
        return ret;

    if (!latmctx->initialized) {
        if (!avctx->extradata) {
            *out_size = 0;
            return avpkt->size;
        } else {
            assert(latmctx->aac_codec->init);
            ret = latmctx->aac_codec->init(avctx);
            if (ret < 0)
                return ret;
            latmctx->initialized = 1;
        }
    }

    tmp         = avpkt->data;
    avpkt->data = tmpbuf;
    avpkt->size = bufsize;

    assert(latmctx->aac_codec->decode);
    ret = latmctx->aac_codec->decode(avctx, out, out_size, avpkt);
    avpkt->data = tmp;
    return ret;
}

static int latm_decode_init(AVCodecContext *avctx)
{
    struct LATMContext *latmctx = avctx->priv_data;
    int ret;

    latmctx->aac_codec = avcodec_find_decoder_by_name("aac");
    if (!latmctx->aac_codec) {
        av_log(avctx, AV_LOG_ERROR, "AAC decoder is required by AAC LATM decoder.\n");
        return AVERROR(ENOSYS);
    }

    assert(latmctx->aac_codec->init);
    ret = latmctx->aac_codec->init(avctx);

    if (avctx->extradata_size > 0)
        latmctx->initialized = !ret;
    else
        latmctx->initialized = 0;

    return ret;
}

static int latm_decode_close(AVCodecContext *avctx)
{
    struct LATMContext *latmctx = avctx->priv_data;
    assert(latmctx->aac_codec->close);
    return latmctx->aac_codec->close(avctx);
}

AVCodec aac_latm_decoder = {
    .name = "aac_latm",
    .type = CODEC_TYPE_AUDIO,
    .id   = CODEC_ID_AAC_LATM,
    .priv_data_size = sizeof(struct LATMContext),
    .init   = latm_decode_init,
    .close  = latm_decode_close,
    .decode = latm_decode_frame,
    .long_name = NULL_IF_CONFIG_SMALL("AAC LATM (Advanced Audio Codec LATM "
                                      "syntax)"),
    .sample_fmts = (const enum SampleFormat[]) {
        SAMPLE_FMT_S16,SAMPLE_FMT_NONE
    },
    .channel_layouts = aac_channel_layout,
};
