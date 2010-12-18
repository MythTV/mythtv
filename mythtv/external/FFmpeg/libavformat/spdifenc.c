/*
 * IEC958 muxer
 * Copyright (c) 2009 Bartlomiej Wolowiec
 * Copyright (c) 2010 Anssi Hannula <anssi.hannula@iki.fi>
 * Copyright (c) 2010 Carl Eugen Hoyos
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
 * IEC-61937 encapsulation of various formats, used by S/PDIF
 * @author Bartlomiej Wolowiec
 * @author Anssi Hannula
 */

/*
 * Terminology used in specification:
 * data-burst - IEC958 frame, contains header and encapsuled frame
 * burst-preambule - IEC958 frame header, contains 16-bits words named Pa, Pb, Pc and Pd
 * burst-payload - encapsuled frame
 * Pa, Pb - syncword - 0xF872, 0x4E1F
 * Pc - burst-info, contains data-type (bits 0-6), error flag (bit 7), data-type-dependent info (bits 8-12)
 *      and bitstream number (bits 13-15)
 * data-type - determines type of encapsuled frames
 * Pd - length code (number of bits or bytes of encapsuled frame - according to data_type)
 *
 * IEC958 frames at normal usage start every specific count of bytes,
 *      dependent from data-type (spaces between packets are filled by zeros)
 */

#include "avformat.h"
#include "spdif.h"
#include "libavcodec/ac3.h"
#include "libavcodec/dca.h"
#include "libavcodec/dcadata.h"
#include "libavcodec/aacadtsdec.h"

typedef struct IEC958Context {
    enum IEC958DataType data_type;  ///< burst info - reference to type of payload of the data-burst
    int length_code;                ///< length code in bits or bytes, depending on data type
    int pkt_offset;                 ///< data burst repetition period in bytes
    uint8_t *buffer;                ///< allocated buffer, used for swap bytes
    int buffer_size;                ///< size of allocated buffer

    uint8_t *out_buf;               ///< pointer to the outgoing data before byte-swapping
    int out_bytes;                  ///< amount of outgoing bytes

    uint8_t *hd_buf;                ///< allocated buffer to concatenate hd audio frames
    int hd_buf_size;                ///< size of the hd audio buffer
    int hd_buf_count;               ///< number of frames in the hd audio buffer
    int hd_buf_filled;              ///< amount of bytes in the hd audio buffer

    int hd_skip;                    ///< counter used for skipping DTS-HD frames

    /// function, which generates codec dependent header information.
    /// Sets data_type and pkt_offset, and length_code, out_bytes, out_buf if necessary
    int (*header_info) (AVFormatContext *s, AVPacket *pkt);
} IEC958Context;


static int spdif_header_ac3(AVFormatContext *s, AVPacket *pkt)
{
    IEC958Context *ctx = s->priv_data;
    int bitstream_mode = pkt->data[6] & 0x7;

    ctx->data_type  = IEC958_AC3 | (bitstream_mode << 8);
    ctx->pkt_offset = AC3_FRAME_SIZE << 2;
    return 0;
}

static int spdif_header_eac3(AVFormatContext *s, AVPacket *pkt)
{
    IEC958Context *ctx = s->priv_data;
    static const int eac3_rates[3] = {48000, 44100, 32000};
    static const uint8_t eac3_repeat[4] = {6, 3, 2, 1};
    int fscod = (pkt->data[4] & 0xc0) >> 6;
    int cod2 = (pkt->data[4] & 0x30) >> 4;
    int repeat = 1;
    int rate = 0;

    if (fscod != 0x11) { /* no fscod2 */
        rate = eac3_rates[fscod];
        repeat = eac3_repeat[cod2]; /* numblkscod */
    } else if (cod2 != 0x11) /* not reserved */
        rate = eac3_rates[cod2] / 2; /* fscod2 */

    /* output bitrate is 4x 16bit stereo at content sample rate */
    s->bit_rate = 4 * 16 * 2 * rate;

    ctx->hd_buf = av_fast_realloc(ctx->hd_buf, &ctx->hd_buf_size, ctx->hd_buf_filled + pkt->size);
    if (!ctx->hd_buf)
        return AVERROR(ENOMEM);

    memcpy(&ctx->hd_buf[ctx->hd_buf_filled], pkt->data, pkt->size);

    ctx->hd_buf_filled += pkt->size;
    if (++ctx->hd_buf_count < repeat){
        ctx->pkt_offset = 0;
        return 0;
    }
    ctx->data_type  = IEC958_EAC3;
    ctx->pkt_offset = 24576;
    ctx->out_buf    = ctx->hd_buf;
    ctx->out_bytes  = ctx->hd_buf_filled;
    ctx->length_code = ctx->hd_buf_filled;

    ctx->hd_buf_count = 0;
    ctx->hd_buf_filled = 0;
    return 0;
}

/*
 * DTS type IV (DTS-HD) can be transmitted with various frame repetition
 * periods; longer repetition periods allow for longer packets and therefore
 * higher bitrate. We don't have information about the maximum bitrate of the
 * incoming DTS-HD stream, so we use a repetition period which uses a stream
 * bitrate of 24.5 Mbps (which is the maximum allowed bitrate on bluray and
 * HDMI, 768 kHz IEC 60958 link) whenever possible.
 * The repetition period is measured in IEC 60958 frames (4 bytes).
 */
enum {
    DTS4_REP_PER_512   = 0x0,
    DTS4_REP_PER_1024  = 0x1,
    DTS4_REP_PER_2048  = 0x2,
    DTS4_REP_PER_4096  = 0x3,
    DTS4_REP_PER_8192  = 0x4,
    DTS4_REP_PER_16384 = 0x5,
};

static void spdif_dts4_set_type(IEC958Context *ctx, int max_offset)
{
    ctx->data_type = IEC958_DTSHD;

    if (max_offset >= 65536) {
        ctx->pkt_offset = 65536;
        ctx->data_type |= DTS4_REP_PER_16384 << 8;
    } else if (max_offset >= 32768) {
        ctx->pkt_offset = 32768;
        ctx->data_type |= DTS4_REP_PER_8192 << 8;
    } else if (max_offset >= 16384) {
        ctx->pkt_offset = 16384;
        ctx->data_type |= DTS4_REP_PER_4096 << 8;
    } else if (max_offset >= 8192) {
        ctx->pkt_offset = 8192;
        ctx->data_type |= DTS4_REP_PER_2048 << 8;
    } else if (max_offset >= 4096) {
        ctx->pkt_offset = 4096;
        ctx->data_type |= DTS4_REP_PER_1024 << 8;
    } else {
        ctx->pkt_offset = 2048;
        ctx->data_type |= DTS4_REP_PER_512 << 8;
    }
}

static int spdif_header_dts(AVFormatContext *s, AVPacket *pkt)
{
    IEC958Context *ctx = s->priv_data;
    uint32_t syncword_dts = AV_RB32(pkt->data);
    int blocks;
    int sample_rate;
    int core_size = 0;

    if (pkt->size < 16)
        return AVERROR_INVALIDDATA;

    switch (syncword_dts) {
    case DCA_MARKER_RAW_BE:
        blocks = (AV_RB16(pkt->data + 4) >> 2) & 0x7f;
        core_size = ((AV_RB24(pkt->data + 5) >> 4) & 0x3fff) + 1;
        sample_rate = dca_sample_rates[(pkt->data[8] >> 2) & 0x0f];
        break;
    case DCA_MARKER_RAW_LE:
        blocks = (AV_RL16(pkt->data + 4) >> 2) & 0x7f;
        core_size = (((pkt->data[4] & 0x03) << 12)
                    | (AV_RL16(pkt->data + 6) >> 4)) + 1;
        sample_rate = dca_sample_rates[(pkt->data[9] >> 2) & 0x0f];
        break;
    case DCA_MARKER_14B_BE:
        blocks =
            (((pkt->data[5] & 0x07) << 4) | ((pkt->data[6] & 0x3f) >> 2));
        core_size = (((AV_RB16(pkt->data + 6) & 0x3ff) << 4)
                    | ((pkt->data[8] & 0x3c) >> 2) + 1) * 8 / 7;
        sample_rate = dca_sample_rates[pkt->data[9] & 0x0f];
        break;
    case DCA_MARKER_14B_LE:
        blocks =
            (((pkt->data[4] & 0x07) << 4) | ((pkt->data[7] & 0x3f) >> 2));
        core_size = (((AV_RL16(pkt->data + 6) & 0x3ff) << 4)
                    | ((pkt->data[9] & 0x3c) >> 2) + 1) * 8 / 7;
        sample_rate = dca_sample_rates[pkt->data[8] & 0x0f];
        break;
    case DCA_HD_MARKER:
        /* DCA-HD frames are only valid when paired with DCA core */
        av_log(s, AV_LOG_DEBUG, "ignoring stray DTS-HD frame\n");
        ctx->pkt_offset = 0;
        return 0;
    default:
        av_log(s, AV_LOG_ERROR, "bad DTS syncword 0x%x\n", syncword_dts);
        return 0 /*-1*/;
    }

    if (s->mux_rate && s->mux_rate < sample_rate * 32) {
        av_log(s, AV_LOG_ERROR, "DTS bitrate exceeds specified limit\n");
        return -1;
    }

        //av_log(s, AV_LOG_ERROR, "SYNC MARK %x, SAMPLE RATE %d, CORE SIZE %d, PKTSIZE %d\n", syncword_dts, sample_rate, core_size, pkt->size);
    blocks++;
    switch (blocks) {
    case  512 >> 5: ctx->data_type = IEC958_DTS1; break;
    case 1024 >> 5: ctx->data_type = IEC958_DTS2; break;
    case 2048 >> 5: ctx->data_type = IEC958_DTS3; break;
    default:
        av_log(s, AV_LOG_ERROR, "%i samples in DTS frame not supported\n",
               blocks << 5);
        /* if such streams really exist, it might be possible to handle them
         * as low bitrate DTS type IV (i.e. like DTS-HD) */
        av_log_ask_for_sample(s, NULL);
        return -1;
    }

    ctx->pkt_offset = blocks << 7;
    s->bit_rate     = sample_rate * 32;

    /* discard any extra data or padding by default */
    if (core_size < pkt->size) {
        ctx->out_bytes = core_size;
        ctx->length_code = core_size << 3;
    }

    /* check for a DTS-HD stream */
    if (core_size + 3 < pkt->size
        && AV_RB32(pkt->data + core_size) == DCA_HD_MARKER) {
        const char dtshd_start_code[10] = { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0xfe };
        int pkt_size = pkt->size;
        int max_rate = s->mux_rate;

        if (!max_rate)
            max_rate = 768000 * 32; /* 768kHz IEC958 link, limit of HDMI v1.3a */

        /* check for a too low rate for reasonable DCA-HD streams,
         * e.g. in case of a non-HDMI S/PDIF link with less than 192kHz */
        if (max_rate < 192000 * 32)
            return 0; /* mux core only */

        /* determine the pkt_offset and DTS IV subtype so that we get as close
         * as possible to the requested rate while not exceeding it */
        spdif_dts4_set_type(ctx, max_rate / sample_rate * (blocks << 5) / 8);
        /* export the final bitrate in bit_rate */
        s->bit_rate = sample_rate * ctx->pkt_offset / (blocks << 5) * 8;

        /* If the bitrate is too high for transmitting at the selected
         * repetition period setting, strip DTS-HD until a good amount
         * of consecutive non-overflowing HD frames have been observed. */
        if (sizeof(dtshd_start_code) + 2 + pkt_size
                > ctx->pkt_offset - BURST_HEADER_SIZE) {
            if (!ctx->hd_skip)
                av_log(s, AV_LOG_WARNING, "DTS-HD bitrate too high, "
                                          "temporarily sending core only\n");
            ctx->hd_skip = sample_rate * 60 / (blocks << 5); /* 1 min */
        }
        if (ctx->hd_skip) {
            pkt_size = core_size;
            --ctx->hd_skip;
        }

        ctx->hd_buf = av_fast_realloc(ctx->hd_buf, &ctx->hd_buf_size,
                                      sizeof(dtshd_start_code) + 2 + pkt_size);
        if (!ctx->hd_buf)
            return AVERROR(ENOMEM);

        memcpy(ctx->hd_buf, dtshd_start_code, sizeof(dtshd_start_code));
        AV_WB16(ctx->hd_buf + sizeof(dtshd_start_code), pkt_size);
        memcpy(ctx->hd_buf + sizeof(dtshd_start_code) + 2, pkt->data, pkt_size);

        ctx->out_buf     = ctx->hd_buf;
        ctx->out_bytes   = sizeof(dtshd_start_code) + 2 + pkt_size;
        ctx->length_code = ctx->out_bytes;
    }

    return 0;
}

static const enum IEC958DataType mpeg_data_type[2][3] = {
    //     LAYER1                      LAYER2                  LAYER3
    { IEC958_MPEG2_LAYER1_LSF, IEC958_MPEG2_LAYER2_LSF, IEC958_MPEG2_LAYER3_LSF },  //MPEG2 LSF
    { IEC958_MPEG1_LAYER1,     IEC958_MPEG1_LAYER23,    IEC958_MPEG1_LAYER23 },     //MPEG1
};

static int spdif_header_mpeg(AVFormatContext *s, AVPacket *pkt)
{
    IEC958Context *ctx = s->priv_data;
    int version =      (pkt->data[1] >> 3) & 3;
    int layer   = 3 - ((pkt->data[1] >> 1) & 3);
    int extension = pkt->data[2] & 1;

    if (layer == 3 || version == 1) {
        av_log(s, AV_LOG_ERROR, "Wrong MPEG file format\n");
        return -1;
    }
    av_log(s, AV_LOG_DEBUG, "version: %i layer: %i extension: %i\n", version, layer, extension);
    if (version == 2 && extension) {
        ctx->data_type  = IEC958_MPEG2_EXT;
        ctx->pkt_offset = 4608;
    } else {
        ctx->data_type  = mpeg_data_type [version & 1][layer];
        ctx->pkt_offset = spdif_mpeg_pkt_offset[version & 1][layer];
    }
    // TODO Data type dependant info (normal/karaoke, dynamic range control)
    return 0;
}

static int spdif_header_aac(AVFormatContext *s, AVPacket *pkt)
{
    IEC958Context *ctx = s->priv_data;
    AACADTSHeaderInfo hdr;
    GetBitContext gbc;
    int ret;

    init_get_bits(&gbc, pkt->data, AAC_ADTS_HEADER_SIZE * 8);
    ret = ff_aac_parse_header(&gbc, &hdr);
    if (ret < 0) {
        av_log(s, AV_LOG_ERROR, "Wrong AAC file format\n");
        return -1;
    }

    ctx->pkt_offset = hdr.samples << 2;
    switch (hdr.num_aac_frames) {
    case 1:
        ctx->data_type = IEC958_MPEG2_AAC;
        break;
    case 2:
        ctx->data_type = IEC958_MPEG2_AAC_LSF_2048;
        break;
    case 4:
        ctx->data_type = IEC958_MPEG2_AAC_LSF_4096;
        break;
    default:
        av_log(s, AV_LOG_ERROR, "%i samples in AAC frame not supported\n",
               hdr.samples);
        return -1;
    }
    //TODO Data type dependent info (LC profile/SBR)
    return 0;
}


/*
 * It seems Dolby TrueHD frames have to be encapsulated in MAT frames before
 * they can be encapsulated in IEC 61937.
 * Here we encapsulate 24 TrueHD frames in a single MAT frame, padding them
 * to achieve constant rate.
 * The actual format of a MAT frame is unknown, but the below seems to work.
 * However, it seems it is not actually necessary for the 24 TrueHD frames to
 * be in an exact alignment with the MAT frame.
 */
#define MAT_FRAME_SIZE          61424
#define TRUEHD_FRAME_OFFSET     2560
#define MAT_MIDDLE_CODE_OFFSET  -4

static int spdif_header_truehd(AVFormatContext *s, AVPacket *pkt)
{
    IEC958Context *ctx = s->priv_data;
    int mat_code_length = 0;
    const char mat_end_code[16] = { 0xC3, 0xC2, 0xC0, 0xC4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x97, 0x11 };

    if (!ctx->hd_buf_count) {
        const char mat_start_code[20] = { 0x07, 0x9E, 0x00, 0x03, 0x84, 0x01, 0x01, 0x01, 0x80, 0x00, 0x56, 0xA5, 0x3B, 0xF4, 0x81, 0x83, 0x49, 0x80, 0x77, 0xE0 };
        mat_code_length = sizeof(mat_start_code) + BURST_HEADER_SIZE;
        memcpy(ctx->hd_buf, mat_start_code, sizeof(mat_start_code));

    } else if (ctx->hd_buf_count == 12) {
        const char mat_middle_code[12] = { 0xC3, 0xC1, 0x42, 0x49, 0x3B, 0xFA, 0x82, 0x83, 0x49, 0x80, 0x77, 0xE0 };
        mat_code_length = sizeof(mat_middle_code) + MAT_MIDDLE_CODE_OFFSET;
        memcpy(&ctx->hd_buf[12 * TRUEHD_FRAME_OFFSET - BURST_HEADER_SIZE + MAT_MIDDLE_CODE_OFFSET],
               mat_middle_code, sizeof(mat_middle_code));
    }

    if (pkt->size > TRUEHD_FRAME_OFFSET - mat_code_length) {
        /* if such frames exist, we'd need some more complex logic to
         * distribute the TrueHD frames in the MAT frame */
        av_log(s, AV_LOG_ERROR, "TrueHD frame too big, %d bytes\n", pkt->size);
        av_log_ask_for_sample(s, NULL);
        return -1;
    }

    memcpy(&ctx->hd_buf[ctx->hd_buf_count * TRUEHD_FRAME_OFFSET - BURST_HEADER_SIZE + mat_code_length],
           pkt->data, pkt->size);
    memset(&ctx->hd_buf[ctx->hd_buf_count * TRUEHD_FRAME_OFFSET - BURST_HEADER_SIZE + mat_code_length + pkt->size],
           0, TRUEHD_FRAME_OFFSET - pkt->size - mat_code_length);

    if (++ctx->hd_buf_count < 24){
        ctx->pkt_offset = 0;
        return 0;
    }
    memcpy(&ctx->hd_buf[MAT_FRAME_SIZE - sizeof(mat_end_code)], mat_end_code, sizeof(mat_end_code));
    ctx->hd_buf_count = 0;

    ctx->data_type   = IEC958_TRUEHD;
    ctx->pkt_offset  = 61440;
    ctx->out_buf     = ctx->hd_buf;
    ctx->out_bytes   = MAT_FRAME_SIZE;
    ctx->length_code = MAT_FRAME_SIZE;
    return 0;
}

static int spdif_write_header(AVFormatContext *s)
{
    IEC958Context *ctx = s->priv_data;
    int min_mux_rate = 0;

    switch (s->streams[0]->codec->codec_id) {
    case CODEC_ID_AC3:
        ctx->header_info = spdif_header_ac3;
        break;
    case CODEC_ID_EAC3:
        ctx->header_info = spdif_header_eac3;
        /* 4 * 16kHz IEC 60958 link */
        min_mux_rate = 4 * 16000 * 32;
        break;
    case CODEC_ID_MP1:
    case CODEC_ID_MP2:
    case CODEC_ID_MP3:
        ctx->header_info = spdif_header_mpeg;
        break;
    case CODEC_ID_DTS:
        ctx->header_info = spdif_header_dts;
        break;
    case CODEC_ID_AAC:
        ctx->header_info = spdif_header_aac;
        break;
    case CODEC_ID_TRUEHD:
        ctx->header_info = spdif_header_truehd;
        ctx->hd_buf = av_malloc(MAT_FRAME_SIZE);
        if (!ctx->hd_buf)
            return AVERROR(ENOMEM);
        /* 705.6kHz IEC 60958 link */
        min_mux_rate = 705600 * 32;
        break;
    default:
        av_log(s, AV_LOG_ERROR, "codec not supported\n");
        return -1;
    }

    if (s->mux_rate && s->mux_rate < min_mux_rate) {
        av_log(s, AV_LOG_ERROR, "stream bitrate exceeds specified limit\n");
        return AVERROR(EINVAL);
    }

    return 0;
}

static int spdif_write_trailer(AVFormatContext *s)
{
    IEC958Context *ctx = s->priv_data;
    av_freep(&ctx->buffer);
    av_freep(&ctx->hd_buf);
    return 0;
}

static int spdif_write_packet(struct AVFormatContext *s, AVPacket *pkt)
{
    IEC958Context *ctx = s->priv_data;
    int ret, padding;

    ctx->out_buf = pkt->data;
    ctx->out_bytes = pkt->size;
    ctx->length_code = FFALIGN(pkt->size, 2) << 3;

    ret = ctx->header_info(s, pkt);
    if (ret < 0)
        return -1;
    if (!ctx->pkt_offset)
        return 0;

    padding = (ctx->pkt_offset - BURST_HEADER_SIZE - ctx->out_bytes) >> 1;
    if (padding < 0) {
        av_log(s, AV_LOG_ERROR, "bitrate is too high\n");
        return -1;
    }

    put_le16(s->pb, SYNCWORD1);      //Pa
    put_le16(s->pb, SYNCWORD2);      //Pb
    put_le16(s->pb, ctx->data_type); //Pc
    put_le16(s->pb, ctx->length_code);//Pd

#if HAVE_BIGENDIAN
    put_buffer(s->pb, ctx->out_buf, ctx->out_bytes & ~1);
#else
    av_fast_malloc(&ctx->buffer, &ctx->buffer_size, ctx->out_bytes + FF_INPUT_BUFFER_PADDING_SIZE);
    if (!ctx->buffer)
        return AVERROR(ENOMEM);
    ff_spdif_bswap_buf16((uint16_t *)ctx->buffer, (uint16_t *)ctx->out_buf, ctx->out_bytes >> 1);
    put_buffer(s->pb, ctx->buffer, ctx->out_bytes & ~1);
#endif

    if (ctx->out_bytes & 1)
        put_be16(s->pb, ctx->out_buf[ctx->out_bytes - 1]);

    for (; padding > 0; padding--)
        put_be16(s->pb, 0);

    av_log(s, AV_LOG_DEBUG, "type=%x len=%i pkt_offset=%i\n",
           ctx->data_type, ctx->out_bytes, ctx->pkt_offset);

    put_flush_packet(s->pb);
    return 0;
}

AVOutputFormat spdif_muxer = {
    "spdif",
    NULL_IF_CONFIG_SMALL("IEC958 - S/PDIF (IEC-61937)"),
    NULL,
    "spdif",
    sizeof(IEC958Context),
    CODEC_ID_AC3,
    CODEC_ID_NONE,
    spdif_write_header,
    spdif_write_packet,
    spdif_write_trailer,
};
