/*
 * TIFF image encoder
 * Copyright (c) 2007 Bartlomiej Wolowiec
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
 * TIFF image encoder
 * @author Bartlomiej Wolowiec
 */

#include "libavutil/log.h"
#include "libavutil/opt.h"

#include "avcodec.h"
#include "internal.h"
#if CONFIG_ZLIB
#include <zlib.h>
#endif
#include "libavutil/opt.h"
#include "bytestream.h"
#include "tiff.h"
#include "rle.h"
#include "lzw.h"
#include "put_bits.h"

#define TIFF_MAX_ENTRY 32

/** sizes of various TIFF field types (string size = 1)*/
static const uint8_t type_sizes2[6] = {
    0, 1, 1, 2, 4, 8
};

typedef struct TiffEncoderContext {
    AVClass *class;                     ///< for private options
    AVCodecContext *avctx;
    AVFrame picture;

    int width;                          ///< picture width
    int height;                         ///< picture height
    unsigned int bpp;                   ///< bits per pixel
    int compr;                          ///< compression level
    int bpp_tab_size;                   ///< bpp_tab size
    int photometric_interpretation;     ///< photometric interpretation
    int strips;                         ///< number of strips
    int rps;                            ///< row per strip
    uint8_t entries[TIFF_MAX_ENTRY*12]; ///< entires in header
    int num_entries;                    ///< number of entires
    uint8_t **buf;                      ///< actual position in buffer
    uint8_t *buf_start;                 ///< pointer to first byte in buffer
    int buf_size;                       ///< buffer size
    uint16_t subsampling[2];            ///< YUV subsampling factors
    struct LZWEncodeState *lzws;        ///< LZW Encode state
    uint32_t dpi;                       ///< image resolution in DPI
} TiffEncoderContext;


/**
 * Check free space in buffer
 * @param s Tiff context
 * @param need Needed bytes
 * @return 0 - ok, 1 - no free space
 */
static inline int check_size(TiffEncoderContext * s, uint64_t need)
{
    if (s->buf_size < *s->buf - s->buf_start + need) {
        *s->buf = s->buf_start + s->buf_size + 1;
        av_log(s->avctx, AV_LOG_ERROR, "Buffer is too small\n");
        return 1;
    }
    return 0;
}

/**
 * Put n values to buffer
 *
 * @param p Pointer to pointer to output buffer
 * @param n Number of values
 * @param val Pointer to values
 * @param type Type of values
 * @param flip =0 - normal copy, >0 - flip
 */
static void tnput(uint8_t ** p, int n, const uint8_t * val, enum TiffTypes type,
                  int flip)
{
    int i;
#if HAVE_BIGENDIAN
    flip ^= ((int[]) {0, 0, 0, 1, 3, 3})[type];
#endif
    for (i = 0; i < n * type_sizes2[type]; i++)
        *(*p)++ = val[i ^ flip];
}

/**
 * Add entry to directory in tiff header.
 * @param s Tiff context
 * @param tag Tag that identifies the entry
 * @param type Entry type
 * @param count The number of values
 * @param ptr_val Pointer to values
 */
static void add_entry(TiffEncoderContext * s,
                      enum TiffTags tag, enum TiffTypes type, int count,
                      const void *ptr_val)
{
    uint8_t *entries_ptr = s->entries + 12 * s->num_entries;

    assert(s->num_entries < TIFF_MAX_ENTRY);

    bytestream_put_le16(&entries_ptr, tag);
    bytestream_put_le16(&entries_ptr, type);
    bytestream_put_le32(&entries_ptr, count);

    if (type_sizes[type] * count <= 4) {
        tnput(&entries_ptr, count, ptr_val, type, 0);
    } else {
        bytestream_put_le32(&entries_ptr, *s->buf - s->buf_start);
        check_size(s, count * type_sizes2[type]);
        tnput(s->buf, count, ptr_val, type, 0);
    }

    s->num_entries++;
}

static void add_entry1(TiffEncoderContext * s,
                       enum TiffTags tag, enum TiffTypes type, int val){
    uint16_t w = val;
    uint32_t dw= val;
    add_entry(s, tag, type, 1, type == TIFF_SHORT ? (void *)&w : (void *)&dw);
}

/**
 * Encode one strip in tiff file
 *
 * @param s Tiff context
 * @param src Input buffer
 * @param dst Output buffer
 * @param n Size of input buffer
 * @param compr Compression method
 * @return Number of output bytes. If an output error is encountered, -1 returned
 */
static int encode_strip(TiffEncoderContext * s, const int8_t * src,
                        uint8_t * dst, int n, int compr)
{

    switch (compr) {
#if CONFIG_ZLIB
    case TIFF_DEFLATE:
    case TIFF_ADOBE_DEFLATE:
        {
            unsigned long zlen = s->buf_size - (*s->buf - s->buf_start);
            if (compress(dst, &zlen, src, n) != Z_OK) {
                av_log(s->avctx, AV_LOG_ERROR, "Compressing failed\n");
                return -1;
            }
            return zlen;
        }
#endif
    case TIFF_RAW:
        if (check_size(s, n))
            return -1;
        memcpy(dst, src, n);
        return n;
    case TIFF_PACKBITS:
        return ff_rle_encode(dst, s->buf_size - (*s->buf - s->buf_start), src, 1, n, 2, 0xff, -1, 0);
    case TIFF_LZW:
        return ff_lzw_encode(s->lzws, src, n);
    default:
        return -1;
    }
}

static void pack_yuv(TiffEncoderContext * s, uint8_t * dst, int lnum)
{
    AVFrame *p = &s->picture;
    int i, j, k;
    int w = (s->width - 1) / s->subsampling[0] + 1;
    uint8_t *pu = &p->data[1][lnum / s->subsampling[1] * p->linesize[1]];
    uint8_t *pv = &p->data[2][lnum / s->subsampling[1] * p->linesize[2]];
    if(s->width % s->subsampling[0] || s->height % s->subsampling[1]){
        for (i = 0; i < w; i++){
            for (j = 0; j < s->subsampling[1]; j++)
                for (k = 0; k < s->subsampling[0]; k++)
                    *dst++ = p->data[0][FFMIN(lnum + j, s->height-1) * p->linesize[0] +
                                        FFMIN(i * s->subsampling[0] + k, s->width-1)];
            *dst++ = *pu++;
            *dst++ = *pv++;
        }
    }else{
        for (i = 0; i < w; i++){
            for (j = 0; j < s->subsampling[1]; j++)
                for (k = 0; k < s->subsampling[0]; k++)
                    *dst++ = p->data[0][(lnum + j) * p->linesize[0] +
                                        i * s->subsampling[0] + k];
            *dst++ = *pu++;
            *dst++ = *pv++;
        }
    }
}

static int encode_frame(AVCodecContext * avctx, AVPacket *pkt,
                        const AVFrame *pict, int *got_packet)
{
    TiffEncoderContext *s = avctx->priv_data;
    AVFrame *const p = &s->picture;
    int i;
    uint8_t *ptr;
    uint8_t *offset;
    uint32_t strips;
    uint32_t *strip_sizes = NULL;
    uint32_t *strip_offsets = NULL;
    int bytes_per_row;
    uint32_t res[2] = { s->dpi, 1 };        // image resolution (72/1)
    uint16_t bpp_tab[] = { 8, 8, 8, 8 };
    int ret = -1;
    int is_yuv = 0;
    uint8_t *yuv_line = NULL;
    int shift_h, shift_v;

    s->avctx = avctx;

    *p = *pict;
    p->pict_type = AV_PICTURE_TYPE_I;
    p->key_frame = 1;
    avctx->coded_frame= &s->picture;

    s->width = avctx->width;
    s->height = avctx->height;
    s->subsampling[0] = 1;
    s->subsampling[1] = 1;

    switch (avctx->pix_fmt) {
    case PIX_FMT_RGBA64LE:
        s->bpp = 64;
        s->photometric_interpretation = 2;
        bpp_tab[0] = 16;
        bpp_tab[1] = 16;
        bpp_tab[2] = 16;
        bpp_tab[3] = 16;
        break;
    case PIX_FMT_RGB48LE:
        s->bpp = 48;
        s->photometric_interpretation = 2;
        bpp_tab[0] = 16;
        bpp_tab[1] = 16;
        bpp_tab[2] = 16;
        bpp_tab[3] = 16;
        break;
    case PIX_FMT_RGBA:
        avctx->bits_per_coded_sample =
        s->bpp = 32;
        s->photometric_interpretation = 2;
        break;
    case PIX_FMT_RGB24:
        avctx->bits_per_coded_sample =
        s->bpp = 24;
        s->photometric_interpretation = 2;
        break;
    case PIX_FMT_GRAY8:
        avctx->bits_per_coded_sample = 0x28;
        s->bpp = 8;
        s->photometric_interpretation = 1;
        break;
    case PIX_FMT_PAL8:
        avctx->bits_per_coded_sample =
        s->bpp = 8;
        s->photometric_interpretation = 3;
        break;
    case PIX_FMT_MONOBLACK:
    case PIX_FMT_MONOWHITE:
        avctx->bits_per_coded_sample =
        s->bpp = 1;
        s->photometric_interpretation = avctx->pix_fmt == PIX_FMT_MONOBLACK;
        bpp_tab[0] = 1;
        break;
    case PIX_FMT_YUV420P:
    case PIX_FMT_YUV422P:
    case PIX_FMT_YUV444P:
    case PIX_FMT_YUV410P:
    case PIX_FMT_YUV411P:
        s->photometric_interpretation = 6;
        avcodec_get_chroma_sub_sample(avctx->pix_fmt,
                &shift_h, &shift_v);
        s->bpp = 8 + (16 >> (shift_h + shift_v));
        s->subsampling[0] = 1 << shift_h;
        s->subsampling[1] = 1 << shift_v;
        s->bpp_tab_size = 3;
        is_yuv = 1;
        break;
    default:
        av_log(s->avctx, AV_LOG_ERROR,
               "This colors format is not supported\n");
        return -1;
    }
    if (!is_yuv)
        s->bpp_tab_size = (s->bpp >= 48) ? ((s->bpp + 7) >> 4):((s->bpp + 7) >> 3);

    if (s->compr == TIFF_DEFLATE || s->compr == TIFF_ADOBE_DEFLATE || s->compr == TIFF_LZW)
        //best choose for DEFLATE
        s->rps = s->height;
    else
        s->rps = FFMAX(8192 / (((s->width * s->bpp) >> 3) + 1), 1);     // suggest size of strip
    s->rps = ((s->rps - 1) / s->subsampling[1] + 1) * s->subsampling[1]; // round rps up

    strips = (s->height - 1) / s->rps + 1;

    if ((ret = ff_alloc_packet2(avctx, pkt, avctx->width * avctx->height * s->bpp * 2 +
                                  avctx->height * 4 + FF_MIN_BUFFER_SIZE)) < 0)
        return ret;
    ptr          = pkt->data;
    s->buf_start = pkt->data;
    s->buf       = &ptr;
    s->buf_size  = pkt->size;

    if (check_size(s, 8))
        goto fail;

    // write header
    bytestream_put_le16(&ptr, 0x4949);
    bytestream_put_le16(&ptr, 42);

    offset = ptr;
    bytestream_put_le32(&ptr, 0);

    strip_sizes = av_mallocz(sizeof(*strip_sizes) * strips);
    strip_offsets = av_mallocz(sizeof(*strip_offsets) * strips);

    bytes_per_row = (((s->width - 1)/s->subsampling[0] + 1) * s->bpp
                    * s->subsampling[0] * s->subsampling[1] + 7) >> 3;
    if (is_yuv){
        yuv_line = av_malloc(bytes_per_row);
        if (yuv_line == NULL){
            av_log(s->avctx, AV_LOG_ERROR, "Not enough memory\n");
            goto fail;
        }
    }

#if CONFIG_ZLIB
    if (s->compr == TIFF_DEFLATE || s->compr == TIFF_ADOBE_DEFLATE) {
        uint8_t *zbuf;
        int zlen, zn;
        int j;

        zlen = bytes_per_row * s->rps;
        zbuf = av_malloc(zlen);
        strip_offsets[0] = ptr - pkt->data;
        zn = 0;
        for (j = 0; j < s->rps; j++) {
            if (is_yuv){
                pack_yuv(s, yuv_line, j);
                memcpy(zbuf + zn, yuv_line, bytes_per_row);
                j += s->subsampling[1] - 1;
            }
            else
                memcpy(zbuf + j * bytes_per_row,
                       p->data[0] + j * p->linesize[0], bytes_per_row);
            zn += bytes_per_row;
        }
        ret = encode_strip(s, zbuf, ptr, zn, s->compr);
        av_free(zbuf);
        if (ret < 0) {
            av_log(s->avctx, AV_LOG_ERROR, "Encode strip failed\n");
            goto fail;
        }
        ptr += ret;
        strip_sizes[0] = ptr - pkt->data - strip_offsets[0];
    } else
#endif
    {
        if(s->compr == TIFF_LZW)
            s->lzws = av_malloc(ff_lzw_encode_state_size);
        for (i = 0; i < s->height; i++) {
            if (strip_sizes[i / s->rps] == 0) {
                if(s->compr == TIFF_LZW){
                    ff_lzw_encode_init(s->lzws, ptr, s->buf_size - (*s->buf - s->buf_start),
                                       12, FF_LZW_TIFF, put_bits);
                }
                strip_offsets[i / s->rps] = ptr - pkt->data;
            }
            if (is_yuv){
                 pack_yuv(s, yuv_line, i);
                 ret = encode_strip(s, yuv_line, ptr, bytes_per_row, s->compr);
                 i += s->subsampling[1] - 1;
            }
            else
                ret = encode_strip(s, p->data[0] + i * p->linesize[0],
                        ptr, bytes_per_row, s->compr);
            if (ret < 0) {
                av_log(s->avctx, AV_LOG_ERROR, "Encode strip failed\n");
                goto fail;
            }
            strip_sizes[i / s->rps] += ret;
            ptr += ret;
            if(s->compr == TIFF_LZW && (i==s->height-1 || i%s->rps == s->rps-1)){
                ret = ff_lzw_encode_flush(s->lzws, flush_put_bits);
                strip_sizes[(i / s->rps )] += ret ;
                ptr += ret;
            }
        }
        if(s->compr == TIFF_LZW)
            av_free(s->lzws);
    }

    s->num_entries = 0;

    add_entry1(s,TIFF_SUBFILE,           TIFF_LONG,             0);
    add_entry1(s,TIFF_WIDTH,             TIFF_LONG,             s->width);
    add_entry1(s,TIFF_HEIGHT,            TIFF_LONG,             s->height);

    if (s->bpp_tab_size)
    add_entry(s, TIFF_BPP,               TIFF_SHORT,    s->bpp_tab_size, bpp_tab);

    add_entry1(s,TIFF_COMPR,             TIFF_SHORT,            s->compr);
    add_entry1(s,TIFF_INVERT,            TIFF_SHORT,            s->photometric_interpretation);
    add_entry(s, TIFF_STRIP_OFFS,        TIFF_LONG,     strips, strip_offsets);

    if (s->bpp_tab_size)
    add_entry1(s,TIFF_SAMPLES_PER_PIXEL, TIFF_SHORT,            s->bpp_tab_size);

    add_entry1(s,TIFF_ROWSPERSTRIP,      TIFF_LONG,             s->rps);
    add_entry(s, TIFF_STRIP_SIZE,        TIFF_LONG,     strips, strip_sizes);
    add_entry(s, TIFF_XRES,              TIFF_RATIONAL, 1,      res);
    add_entry(s, TIFF_YRES,              TIFF_RATIONAL, 1,      res);
    add_entry1(s,TIFF_RES_UNIT,          TIFF_SHORT,            2);

    if(!(avctx->flags & CODEC_FLAG_BITEXACT))
    add_entry(s, TIFF_SOFTWARE_NAME,     TIFF_STRING,
              strlen(LIBAVCODEC_IDENT) + 1, LIBAVCODEC_IDENT);

    if (avctx->pix_fmt == PIX_FMT_PAL8) {
        uint16_t pal[256 * 3];
        for (i = 0; i < 256; i++) {
            uint32_t rgb = *(uint32_t *) (p->data[1] + i * 4);
            pal[i]       = ((rgb >> 16) & 0xff) * 257;
            pal[i + 256] = ((rgb >> 8 ) & 0xff) * 257;
            pal[i + 512] = ( rgb        & 0xff) * 257;
        }
        add_entry(s, TIFF_PAL, TIFF_SHORT, 256 * 3, pal);
    }
    if (is_yuv){
        /** according to CCIR Recommendation 601.1 */
        uint32_t refbw[12] = {15, 1, 235, 1, 128, 1, 240, 1, 128, 1, 240, 1};
        add_entry(s, TIFF_YCBCR_SUBSAMPLING, TIFF_SHORT,    2, s->subsampling);
        add_entry(s, TIFF_REFERENCE_BW,      TIFF_RATIONAL, 6, refbw);
    }
    bytestream_put_le32(&offset, ptr - pkt->data);    // write offset to dir

    if (check_size(s, 6 + s->num_entries * 12)) {
        ret = AVERROR(EINVAL);
        goto fail;
    }
    bytestream_put_le16(&ptr, s->num_entries);  // write tag count
    bytestream_put_buffer(&ptr, s->entries, s->num_entries * 12);
    bytestream_put_le32(&ptr, 0);

    pkt->size   = ptr - pkt->data;
    pkt->flags |= AV_PKT_FLAG_KEY;
    *got_packet = 1;

fail:
    av_free(strip_sizes);
    av_free(strip_offsets);
    av_free(yuv_line);
    return ret < 0 ? ret : 0;
}

#define OFFSET(x) offsetof(TiffEncoderContext, x)
#define VE AV_OPT_FLAG_VIDEO_PARAM | AV_OPT_FLAG_ENCODING_PARAM
static const AVOption options[] = {
    {"dpi", "set the image resolution (in dpi)", OFFSET(dpi), AV_OPT_TYPE_INT, {.dbl = 72}, 1, 0x10000, AV_OPT_FLAG_VIDEO_PARAM|AV_OPT_FLAG_ENCODING_PARAM},
    { "compression_algo", NULL, OFFSET(compr), AV_OPT_TYPE_INT, {TIFF_PACKBITS}, TIFF_RAW, TIFF_DEFLATE, VE, "compression_algo" },
    { "packbits", NULL, 0, AV_OPT_TYPE_CONST, {TIFF_PACKBITS}, 0, 0, VE, "compression_algo" },
    { "raw",      NULL, 0, AV_OPT_TYPE_CONST, {TIFF_RAW},      0, 0, VE, "compression_algo" },
    { "lzw",      NULL, 0, AV_OPT_TYPE_CONST, {TIFF_LZW},      0, 0, VE, "compression_algo" },
#if CONFIG_ZLIB
    { "deflate",  NULL, 0, AV_OPT_TYPE_CONST, {TIFF_DEFLATE},  0, 0, VE, "compression_algo" },
#endif
    { NULL },
};

static const AVClass tiffenc_class = {
    .class_name = "TIFF encoder",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
};

AVCodec ff_tiff_encoder = {
    .name           = "tiff",
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = CODEC_ID_TIFF,
    .priv_data_size = sizeof(TiffEncoderContext),
    .encode2        = encode_frame,
    .pix_fmts       = (const enum PixelFormat[]) {
        PIX_FMT_RGB24, PIX_FMT_PAL8, PIX_FMT_GRAY8,
        PIX_FMT_MONOBLACK, PIX_FMT_MONOWHITE,
        PIX_FMT_YUV420P, PIX_FMT_YUV422P, PIX_FMT_YUV444P,
        PIX_FMT_YUV410P, PIX_FMT_YUV411P, PIX_FMT_RGB48LE,
        PIX_FMT_RGBA, PIX_FMT_RGBA64LE,
        PIX_FMT_NONE
    },
    .long_name      = NULL_IF_CONFIG_SMALL("TIFF image"),
    .priv_class     = &tiffenc_class,
};
