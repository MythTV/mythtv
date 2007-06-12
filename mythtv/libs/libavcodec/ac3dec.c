/*
 * AC-3 Audio Decoder.
 * This code was developed as part of the Google Summer of Code 2006 Program.
 *
 * Copyright (c) 2006 Kartikey Mahendra BHATT (bhattkm at gmail dot com).
 * Copyright (c) 2007 Justin Ruggles.
 *
 * This file is part of FFmpeg.
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

#include <stdio.h>
#include <stddef.h>
#include <math.h>
#include <string.h>

#include "avcodec.h"
#include "ac3.h"
#include "ac3mix.h"
#include "bitstream.h"
#include "dsputil.h"
#include "random.h"

static const uint8_t nfchans_tbl[8] = { 2, 1, 2, 3, 3, 4, 4, 5 };

static const uint8_t rematrix_band_tbl[5] = { 13, 25, 37, 61, 253 };

/** table for exponent to scale_factor mapping */
static float scale_factors[25];

/** tables for ungrouping mantissas */
static float b1_mantissas[ 32][3];
static float b2_mantissas[128][3];
static float b4_mantissas[128][2];

/** table for grouping exponents */
static uint8_t exp_ungroup_tbl[128][3];

/** quantization table: levels for symmetric. bits for asymmetric. */
static const uint8_t qntztab[16] = { 0, 3, 5, 7, 11, 15, 5, 6, 7, 8, 9, 10, 11, 12, 14, 16 };

/** dynamic range table. converts codes to scale factors. */
static float dynrng_tbl[256];

/** table for center mix levels */
static const float clevs[4] = { LEVEL_MINUS_3DB, LEVEL_MINUS_4POINT5DB,
                                LEVEL_MINUS_6DB, LEVEL_MINUS_4POINT5DB };

/** table for surround mix levels */
static const float slevs[4] = { LEVEL_MINUS_3DB, LEVEL_MINUS_6DB, LEVEL_ZERO,
                                LEVEL_MINUS_6DB };

/** output configurations. */
#define AC3_OUTPUT_UNMODIFIED 0x01
#define AC3_OUTPUT_MONO       0x02
#define AC3_OUTPUT_STEREO     0x04
#define AC3_OUTPUT_DOLBY      0x08
#define AC3_OUTPUT_LFEON      0x10

typedef struct {
    uint16_t crc1;          ///< CRC-16 for 1st 5/8 of frame
    uint8_t  fscod;         ///< sample rate code
    uint8_t  acmod;         ///< audio coding mode
    uint8_t  cmixlev;       ///< center mix level
    uint8_t  surmixlev;     ///< surround mix level

    uint8_t  blksw[AC3_MAX_CHANNELS];       ///< block switch flags
    uint8_t  dithflag[AC3_MAX_CHANNELS];    ///< dither flags
    uint8_t  cplinu;                        ///< coupling in use
    uint8_t  chincpl[AC3_MAX_CHANNELS];     ///< channel in coupling
    uint8_t  phsflginu;                     ///< phase flag in use
    uint8_t  cplbegf;                       ///< coupling start band code
    uint8_t  cplendf;                       ///< coupling end band code
    uint8_t  cplcoe;                        ///< coupling coeffs exist
    uint32_t cplbndstrc[18];                ///< coupling band structure
    uint8_t  nrematbnd;                     ///< number of rematrixing bands
    uint8_t  rematstr;                      ///< rematrixing strategy
    uint8_t  rematflg[4];                   ///< rematrixing flags
    uint8_t  expstr[AC3_MAX_CHANNELS+1];    ///< exponent strategies
    uint8_t  halfratecod;                   ///< rate shift (for DolbyNet)
    AC3BitAllocParameters bit_alloc_params; ///< bit allocation parameters
    uint8_t  csnroffst;                     ///< coarse snr offset
    uint8_t  fsnroffst[AC3_MAX_CHANNELS+1]; ///< fine snr offsets
    uint8_t  fgaincod[AC3_MAX_CHANNELS+1];  ///< fast gain codes
    uint8_t  deltbae[AC3_MAX_CHANNELS+1];       ///< delta bit alloc exists
    uint8_t  deltnseg[AC3_MAX_CHANNELS+1];      ///< number of delta segments
    uint8_t  deltoffst[AC3_MAX_CHANNELS+1][8];  ///< delta segment offsets
    uint8_t  deltlen[AC3_MAX_CHANNELS+1][8];    ///< delta segment lengths
    uint8_t  deltba[AC3_MAX_CHANNELS+1][8];     ///< bit allocation deltas

    uint8_t bit_alloc_stages[AC3_MAX_CHANNELS+1];

    /* audio info */
    int     sample_rate;    ///< sample rate in Hz
    int     bit_rate;       ///< bit rate in kbps
    int     frame_size;     ///< frame size in bytes
    int     nfchans;        ///< number of full-bandwidth channels
    int     nchans;         ///< number of total channels, including LFE
    int     lfeon;          ///< indicates if LFE channel is present
    int     lfe_channel;    ///< index of LFE channel
    int     cpl_channel;    ///< index of coupling channel
    int     blkoutput;      ///< output configuration for block
    int     out_channels;   ///< number of output channels
    float   downmix_scale_factors[AC3_MAX_CHANNELS];  ///< downmix gain for each channel

    /* decoded values */
    float   dynrng[2];                  ///< dynamic range scale factors
    float   cplco[FBW_CHANNELS][18];    ///< coupling coordinates
    float   cpl_coeffs[AC3_MAX_COEFS];  ///< coupling coefficients
    int     ncplbnd;                    ///< number of coupling bands
    int     ncplsubnd;                  ///< number of coupling sub-bands
    int     cplstrtmant;                ///< coupling start mantissa
    int     cplendmant;                 ///< coupling end mantissa
    int     endmant[AC3_MAX_CHANNELS];  ///< channel end mantissas
    uint8_t dexps[AC3_MAX_CHANNELS+1][AC3_MAX_COEFS];   ///< decoded exponents
    uint8_t bap[AC3_MAX_CHANNELS+1][AC3_MAX_COEFS];     ///< bit allocation pointers
    int16_t psd[AC3_MAX_CHANNELS+1][AC3_MAX_COEFS];     ///< scaled exponents
    int16_t bndpsd[AC3_MAX_CHANNELS+1][50];             ///< interpolated exponents
    int16_t mask[AC3_MAX_CHANNELS+1][50];               ///< masking values
    DECLARE_ALIGNED_16(float, transform_coeffs[AC3_MAX_CHANNELS][AC3_MAX_COEFS]);

    DECLARE_ALIGNED_16(float, output[AC3_MAX_CHANNELS][AC3_BLOCK_SIZE]);       ///< output after imdct transform and windowing
    DECLARE_ALIGNED_16(int16_t, int_output[AC3_MAX_CHANNELS][AC3_BLOCK_SIZE]); ///< final 16-bit integer output
    DECLARE_ALIGNED_16(float, delay[AC3_MAX_CHANNELS][AC3_BLOCK_SIZE]);        ///< delay - added to the next block
    DECLARE_ALIGNED_16(float, tmp_imdct[AC3_BLOCK_SIZE]);                      ///< temp storage for imdct transform
    DECLARE_ALIGNED_16(float, tmp_output[AC3_BLOCK_SIZE * 2]);                 ///< temp storage for output before windowing
    DECLARE_ALIGNED_16(float, window[AC3_BLOCK_SIZE]);                         ///< window coefficients

    MDCTContext imdct_512;      ///< for 512 sample imdct transform
    MDCTContext imdct_256;      ///< for 256 sample imdct transform
    DSPContext  dsp;            ///< for optimization
    float add_bias;             ///< offset for float_to_int16 conversion
    float mul_bias;             ///< scaling for float_to_int16 conversion
    GetBitContext gb;           ///< bit-wise reader
    AVRandomState dith_state;   ///< for dither generation
} AC3DecodeContext;

static int16_t dither_int16(AVRandomState *state)
{
    uint32_t y = av_random(state);
    return (int16_t)(y ^ (y >> 16));
}

/**
 * Generates a Kaiser-Bessel Derived Window.
 */
static void ac3_window_init(float *window)
{
   int i, j;
   double sum = 0.0, bessel, tmp;
   double local_window[256];
   double alpha2 = (5.0 * M_PI / 256.0) * (5.0 * M_PI / 256.0);

   for (i = 0; i < 256; i++) {
       tmp = i * (256 - i) * alpha2;
       bessel = 1.0;
       for (j = 100; j > 0; j--) /* default to 100 iterations */
           bessel = bessel * tmp / (j * j) + 1;
       sum += bessel;
       local_window[i] = sum;
   }

   sum++;
   for (i = 0; i < 256; i++)
       window[i] = sqrt(local_window[i] / sum);
}

static inline float
symmetric_dequant(int code, int levels)
{
    float m = (code - (levels >> 1)) << 1;
    return (m / levels);
}

static void ac3_decoder_tables_init(void)
{
    int i;

    /* generate mantissa tables */
    for(i=0; i<32; i++) {
        b1_mantissas[i][0] = symmetric_dequant( i / 9     , 3);
        b1_mantissas[i][1] = symmetric_dequant((i % 9) / 3, 3);
        b1_mantissas[i][2] = symmetric_dequant((i % 9) % 3, 3);
    }
    for(i=0; i<128; i++) {
        b2_mantissas[i][0] = symmetric_dequant( i / 25     , 5);
        b2_mantissas[i][1] = symmetric_dequant((i % 25) / 5, 5);
        b2_mantissas[i][2] = symmetric_dequant((i % 25) % 5, 5);
    }
    for(i=0; i<128; i++) {
        b4_mantissas[i][0] = symmetric_dequant(i / 11, 11);
        b4_mantissas[i][1] = symmetric_dequant(i % 11, 11);
    }

    /* generate dynamic range table */
    for(i=0; i<256; i++) {
        int v = i >> 5;
        if(v > 3) v -= 7;
        else v++;
        dynrng_tbl[i] = powf(2.0f, v);
        v = (i & 0x1F) | 0x20;
        dynrng_tbl[i] *= v / 64.0f;
    }

    /* generate scale factors */
    for (i = 0; i < 25; i++)
        scale_factors[i] = pow(2.0, -i);

    /* generate exponent tables */
    for(i=0; i<128; i++) {
        exp_ungroup_tbl[i][0] =  i / 25;
        exp_ungroup_tbl[i][1] = (i % 25) / 5;
        exp_ungroup_tbl[i][2] = (i % 25) % 5;
    }
}

static int ac3_decode_init(AVCodecContext *avctx)
{
    AC3DecodeContext *ctx = avctx->priv_data;
    int ch;

    ac3_common_init();
    ac3_decoder_tables_init();
    ff_mdct_init(&ctx->imdct_256, 8, 1);
    ff_mdct_init(&ctx->imdct_512, 9, 1);
    ac3_window_init(ctx->window);
    av_init_random(0, &ctx->dith_state);

    dsputil_init(&ctx->dsp, avctx);
    if(ctx->dsp.float_to_int16 == ff_float_to_int16_c) {
        ctx->add_bias = 385.0f;
        ctx->mul_bias = 1.0f;
    } else {
        ctx->add_bias = 0.0f;
        ctx->mul_bias = 32767.0f;
    }

    for(ch=0; ch<AC3_MAX_CHANNELS; ch++) {
        memset(ctx->delay[ch], 0, sizeof(ctx->delay[ch]));
    }

    ctx->blkoutput = -1;

    return 0;
}

/**
 * Parses the 'sync info' and 'bit stream info' from the AC-3 bitstream.
 * GetBitContext within AC3DecodeContext must point to start of the
 * synchronized AC-3 bitstream.
 */
static int parse_header(AC3DecodeContext *ctx)
{
    AC3HeaderInfo hdr;
    GetBitContext *gb = &ctx->gb;
    int err, i;

    err = ff_ac3_parse_header(gb->buffer, &hdr);
    if(err)
        return err;

    /* get decoding parameters from header info */
    ctx->crc1 = hdr.crc1;
    ctx->fscod = hdr.fscod;
    ctx->bit_alloc_params.fscod = hdr.fscod;
    ctx->acmod = hdr.acmod;
    ctx->cmixlev = hdr.cmixlev;
    ctx->surmixlev = hdr.surmixlev;
    ctx->lfeon = hdr.lfeon;
    ctx->halfratecod = hdr.halfratecod;
    ctx->bit_alloc_params.halfratecod = hdr.halfratecod;
    ctx->sample_rate = hdr.sample_rate;
    ctx->bit_rate = hdr.bit_rate / 1000;
    ctx->nchans = hdr.channels;
    ctx->nfchans = ctx->nchans - ctx->lfeon;
    ctx->lfe_channel = ctx->nfchans;
    ctx->cpl_channel = ctx->lfe_channel+1;
    ctx->frame_size = hdr.frame_size;

    /* skip over portion of header which has already been read */
    skip_bits(gb, 16);
    skip_bits(gb, 16);
    skip_bits(gb, 8);
    skip_bits(gb, 11);
    if(ctx->acmod == AC3_CHANNEL_MODE_STEREO) {
        skip_bits(gb, 2);
    } else {
        if((ctx->acmod & 1) && ctx->acmod != AC3_CHANNEL_MODE_MONO)
            skip_bits(gb, 2);
        if(ctx->acmod & 4)
            skip_bits(gb, 2);
    }
    skip_bits1(gb);

    /* skip the rest of the bsi */
    /* FIXME: read & use the dialog normalization and compression */
    i = !(ctx->acmod);
    do {
        skip_bits(gb, 5); //skip dialog normalization
        if (get_bits1(gb))
            skip_bits(gb, 8); //skip compression
        if (get_bits1(gb))
            skip_bits(gb, 8); //skip language code
        if (get_bits1(gb))
            skip_bits(gb, 7); //skip audio production information
    } while (i--);

    skip_bits(gb, 2); //skip copyright bit and original bitstream bit

    /* FIXME: read & use the xbsi1 downmix levels */
    if (get_bits1(gb))
        skip_bits(gb, 14); //skip timecode1 or xbsi1
    if (get_bits1(gb))
        skip_bits(gb, 14); //skip timecode2 or xbsi2

    if (get_bits1(gb)) {
        i = get_bits(gb, 6); //additional bsi length
        do {
            skip_bits(gb, 8);
        } while(i--);
    }

    /* set some frame defaults */
    for(i=0; i<AC3_MAX_CHANNELS+1; i++) {
        ctx->deltbae[i] = DBA_NONE;
        ctx->deltnseg[i] = 0;
    }
    ctx->dynrng[0] = 1.0;
    ctx->dynrng[1] = 1.0;

    return 0;
}

/**
 * Decodes the grouped exponents.
 * This function decodes the coded exponents according to exponent strategy
 * and stores them in the decoded exponents buffer.
 */
static void decode_exponents(GetBitContext *gb, int expstr, int ngrps,
                             uint8_t absexp, uint8_t *dexps)
{
    int i, j, grp, grpsize;
    int dexp[256];
    int aexp[256];
    int expacc, prevexp;

    /* unpack groups */
    grpsize = expstr;
    if(expstr == EXP_D45) grpsize = 4;
    for(grp=0,i=0; grp<ngrps; grp++) {
        expacc = get_bits(gb, 7);
        dexp[i++] = exp_ungroup_tbl[expacc][0];
        dexp[i++] = exp_ungroup_tbl[expacc][1];
        dexp[i++] = exp_ungroup_tbl[expacc][2];
    }

    /* convert to absolute exps */
    prevexp = absexp;
    for(i=0; i<ngrps*3; i++) {
        prevexp += dexp[i]-2;
        prevexp = av_clip(prevexp, 0, 24);
        aexp[i] = prevexp;
    }

    /* expand to full array */
    for(i=0; i<ngrps*3; i++) {
        for(j=0; j<grpsize; j++) {
            dexps[(i*grpsize)+j] = aexp[i];
        }
    }
}

static void do_bit_allocation(AC3DecodeContext *ctx, int ch)
{
    int start=0, end=0;

    if(ch == ctx->cpl_channel) {
        start = ctx->cplstrtmant;
        end = ctx->cplendmant;
    } else {
        end = ctx->endmant[ch];
    }

    if(ctx->bit_alloc_stages[ch] > 2) {
        ff_ac3_bit_alloc_calc_psd((int8_t *)ctx->dexps[ch], start, end,
                                    ctx->psd[ch], ctx->bndpsd[ch]);
    }
    if(ctx->bit_alloc_stages[ch] > 1) {
        int fgain = ff_fgaintab[ctx->fgaincod[ch]];
        ff_ac3_bit_alloc_calc_mask(&ctx->bit_alloc_params,
                                     ctx->bndpsd[ch], start, end, fgain,
                                     (ch == ctx->lfe_channel),
                                     ctx->deltbae[ch], ctx->deltnseg[ch],
                                     ctx->deltoffst[ch], ctx->deltlen[ch],
                                     ctx->deltba[ch], ctx->mask[ch]);
    }
    if(ctx->bit_alloc_stages[ch] > 0) {
        int snroffst = (((ctx->csnroffst - 15) << 4) + ctx->fsnroffst[ch]) << 2;
        ff_ac3_bit_alloc_calc_bap(ctx->mask[ch], ctx->psd[ch], start, end,
                                  snroffst, ctx->bit_alloc_params.floor,
                                  ctx->bap[ch]);
    }
}

/**
 * Generates transform coefficients for each coupled channel in the coupling
 * range using the coupling coefficients and coupling coordinates.
 */
static void uncouple_channels(AC3DecodeContext *ctx)
{
    int i, ch, end, bnd, subbnd;

    subbnd = 0;
    for(i=ctx->cplstrtmant,bnd=0; i<ctx->cplendmant; bnd++) {
        /* get last bin in coupling band using coupling band structure */
        end = i + 12;
        while (ctx->cplbndstrc[subbnd]) {
            end += 12;
            subbnd++;
        }
        subbnd++;

        /* calculate transform coeffs */
        while(i < end) {
            for(ch=0; ch<ctx->nfchans; ch++) {
                if(ctx->chincpl[ch]) {
                    ctx->transform_coeffs[ch][i] = ctx->cpl_coeffs[i] *
                                                   ctx->cplco[ch][bnd];
                }
            }
            i++;
        }
    }
}

/* ungrouped mantissas */
typedef struct {
    float b1_mant[3];
    float b2_mant[3];
    float b4_mant[2];
    int b1ptr;
    int b2ptr;
    int b4ptr;
} mant_groups;

/** Gets the transform coefficients for particular channel */
static void get_transform_coeffs_ch(AC3DecodeContext *ctx, int ch,
                                    mant_groups *m)
{
    GetBitContext *gb = &ctx->gb;
    int i, gcode, tbap, dithflag, start, end;
    float *coeffs;

    if(ch == ctx->cpl_channel) {
        dithflag = 1;
        coeffs = ctx->cpl_coeffs;
        start = ctx->cplstrtmant;
        end = ctx->cplendmant;
    } else {
        dithflag = ctx->dithflag[ch];
        coeffs = ctx->transform_coeffs[ch];
        start = 0;
        end = ctx->endmant[ch];
    }

    for(i=start; i<end; i++) {
        tbap = ctx->bap[ch][i];
        switch (tbap) {
            case 0:
                if (!dithflag) {
                    coeffs[i] = 0;
                } else {
                    coeffs[i] = dither_int16(&ctx->dith_state) / 32768.0;
                    coeffs[i] *= LEVEL_MINUS_3DB;
                }
                break;

            case 1:
                if (m->b1ptr > 2) {
                    gcode = get_bits(gb, 5);
                    m->b1_mant[0] = b1_mantissas[gcode][0];
                    m->b1_mant[1] = b1_mantissas[gcode][1];
                    m->b1_mant[2] = b1_mantissas[gcode][2];
                    m->b1ptr = 0;
                }
                coeffs[i] = m->b1_mant[m->b1ptr++];
                break;

            case 2:
                if (m->b2ptr > 2) {
                    gcode = get_bits(gb, 7);
                    m->b2_mant[0] = b2_mantissas[gcode][0];
                    m->b2_mant[1] = b2_mantissas[gcode][1];
                    m->b2_mant[2] = b2_mantissas[gcode][2];
                    m->b2ptr = 0;
                }
                coeffs[i] = m->b2_mant[m->b2ptr++];
                break;

            case 3:
                coeffs[i] = symmetric_dequant(get_bits(gb, 3), qntztab[tbap]);
                break;

            case 4:
                if (m->b4ptr > 1) {
                    gcode = get_bits(gb, 7);
                    m->b4_mant[0] = b4_mantissas[gcode][0];
                    m->b4_mant[1] = b4_mantissas[gcode][1];
                    m->b4ptr = 0;
                }
                coeffs[i] = m->b4_mant[m->b4ptr++];
                break;

            case 5:
                coeffs[i] = symmetric_dequant(get_bits(gb, 4), qntztab[tbap]);
                break;

            default:
                /* asymmetric dequantization */
                coeffs[i] = get_sbits(gb, qntztab[tbap]) * scale_factors[qntztab[tbap]-1];
                break;
        }
        coeffs[i] *= scale_factors[ctx->dexps[ch][i]];
    }
}

/**
 * Gets the transform coefficients.
 * This function extracts the tranform coefficients from the AC-3 bitstream
 * using the previously decoded bit allocation pointers.  If coupling is in
 * use, coupled coefficients are also reconstructed here.
 */
static void get_transform_coeffs(AC3DecodeContext * ctx)
{
    int ch, end;
    int got_cplch = 0;
    mant_groups m;

    m.b1ptr = m.b2ptr = m.b4ptr = 3;

    for(ch=0; ch<ctx->nchans; ch++) {
        /* transform coefficients for individual channel */
        get_transform_coeffs_ch(ctx, ch, &m);

        /* tranform coefficients for coupling channel */
        if(ctx->chincpl[ch]) {
            if (!got_cplch) {
                get_transform_coeffs_ch(ctx, ctx->cpl_channel, &m);
                uncouple_channels(ctx);
                got_cplch = 1;
            }
            end = ctx->cplendmant;
        } else {
            end = ctx->endmant[ch];
        }
        do {
            ctx->transform_coeffs[ch][end] = 0;
        } while(++end < 256);
    }
}

/**
 * Performs stereo rematrixing.
 * Reconstruct left/right from mid/side
 */
static void do_rematrixing(AC3DecodeContext *ctx)
{
    int bnd, i;
    int end, bndend;
    float tmp0, tmp1;

    end = FFMIN(ctx->endmant[0], ctx->endmant[1]);

    for(bnd=0; bnd<ctx->nrematbnd; bnd++) {
        if(ctx->rematflg[bnd]) {
            bndend = FFMIN(end, rematrix_band_tbl[bnd+1]);
            for(i=rematrix_band_tbl[bnd]; i<bndend; i++) {
                tmp0 = ctx->transform_coeffs[0][i];
                tmp1 = ctx->transform_coeffs[1][i];
                ctx->transform_coeffs[0][i] = tmp0 + tmp1;
                ctx->transform_coeffs[1][i] = tmp0 - tmp1;
            }
        }
    }
}

/**
 * Sets downmixing gain for each channel based on input and output
 * channel configurations and mixing levels coded in the AC-3 bitstream.
 */
static void update_downmix_scaling(AC3DecodeContext *ctx)
{
    int ch;
    int from = ctx->acmod;
    int to = ctx->blkoutput;
    float cmix = clevs[ctx->cmixlev];
    float smix = slevs[ctx->surmixlev];

    for(ch=0; ch<ctx->nchans; ch++) {
        ctx->downmix_scale_factors[ch] = 1.0f;
    }

    if(to & AC3_OUTPUT_UNMODIFIED)
        return;

    switch (from) {
        case AC3_CHANNEL_MODE_DUALMONO:
            switch (to) {
                case AC3_OUTPUT_MONO:
                    scale_dualmono_to_mono(ctx->downmix_scale_factors);
                    break;
                case AC3_OUTPUT_STEREO:
                    scale_dualmono_to_stereo(ctx->downmix_scale_factors);
                    break;
            }
            break;
        case AC3_CHANNEL_MODE_MONO:
            switch (to) {
                case AC3_OUTPUT_STEREO:
                    scale_mono_to_stereo(ctx->downmix_scale_factors);
                    break;
            }
            break;
        case AC3_CHANNEL_MODE_STEREO:
            switch (to) {
                case AC3_OUTPUT_MONO:
                    scale_stereo_to_mono(ctx->downmix_scale_factors);
                    break;
            }
            break;
        case AC3_CHANNEL_MODE_3F:
            switch (to) {
                case AC3_OUTPUT_MONO:
                    scale_3f_to_mono(ctx->downmix_scale_factors, cmix);
                    break;
                case AC3_OUTPUT_STEREO:
                    scale_3f_to_stereo(ctx->downmix_scale_factors, cmix);
                    break;
            }
            break;
        case AC3_CHANNEL_MODE_2F_1R:
            switch (to) {
                case AC3_OUTPUT_MONO:
                    scale_2f_1r_to_mono(ctx->downmix_scale_factors, smix);
                    break;
                case AC3_OUTPUT_STEREO:
                    scale_2f_1r_to_stereo(ctx->downmix_scale_factors, smix);
                    break;
            }
            break;
        case AC3_CHANNEL_MODE_3F_1R:
            switch (to) {
                case AC3_OUTPUT_MONO:
                    scale_3f_1r_to_mono(ctx->downmix_scale_factors, cmix, smix);
                    break;
                case AC3_OUTPUT_STEREO:
                    scale_3f_1r_to_stereo(ctx->downmix_scale_factors, cmix, smix);
                    break;
            }
            break;
        case AC3_CHANNEL_MODE_2F_2R:
            switch (to) {
                case AC3_OUTPUT_MONO:
                    scale_2f_2r_to_mono(ctx->downmix_scale_factors, smix);
                    break;
                case AC3_OUTPUT_STEREO:
                    scale_2f_2r_to_stereo(ctx->downmix_scale_factors, smix);
                    break;
            }
            break;
        case AC3_CHANNEL_MODE_3F_2R:
            switch (to) {
                case AC3_OUTPUT_MONO:
                    scale_3f_2r_to_mono(ctx->downmix_scale_factors, cmix, smix);
                    break;
                case AC3_OUTPUT_STEREO:
                    scale_3f_2r_to_stereo(ctx->downmix_scale_factors, cmix, smix);
                    break;
            }
            break;
    }
}

/**
 * Downmixes the output.
 * This function downmixes the output when the number of input
 * channels is not equal to the number of output channels requested.
 */
static void do_downmix(AC3DecodeContext *ctx)
{
    int from = ctx->acmod;
    int to = ctx->blkoutput;

    if (to & AC3_OUTPUT_UNMODIFIED)
        return;

    switch (from) {
        case AC3_CHANNEL_MODE_DUALMONO:
            switch (to) {
                case AC3_OUTPUT_MONO:
                    mix_dualmono_to_mono(ctx->output);
                    break;
                case AC3_OUTPUT_STEREO:
                    mix_dualmono_to_stereo(ctx->output);
                    break;
            }
            break;
        case AC3_CHANNEL_MODE_MONO:
            switch (to) {
                case AC3_OUTPUT_STEREO:
                    mix_mono_to_stereo(ctx->output);
                    break;
            }
            break;
        case AC3_CHANNEL_MODE_STEREO:
            switch (to) {
                case AC3_OUTPUT_MONO:
                    mix_stereo_to_mono(ctx->output);
                    break;
            }
            break;
        case AC3_CHANNEL_MODE_3F:
            switch (to) {
                case AC3_OUTPUT_MONO:
                    mix_3f_to_mono(ctx->output);
                    break;
                case AC3_OUTPUT_STEREO:
                    mix_3f_to_stereo(ctx->output);
                    break;
            }
            break;
        case AC3_CHANNEL_MODE_2F_1R:
            switch (to) {
                case AC3_OUTPUT_MONO:
                    mix_2f_1r_to_mono(ctx->output);
                    break;
                case AC3_OUTPUT_STEREO:
                    mix_2f_1r_to_stereo(ctx->output);
                    break;
            }
            break;
        case AC3_CHANNEL_MODE_3F_1R:
            switch (to) {
                case AC3_OUTPUT_MONO:
                    mix_3f_1r_to_mono(ctx->output);
                    break;
                case AC3_OUTPUT_STEREO:
                    mix_3f_1r_to_stereo(ctx->output);
                    break;
            }
            break;
        case AC3_CHANNEL_MODE_2F_2R:
            switch (to) {
                case AC3_OUTPUT_MONO:
                    mix_2f_2r_to_mono(ctx->output);
                    break;
                case AC3_OUTPUT_STEREO:
                    mix_2f_2r_to_stereo(ctx->output);
                    break;
            }
            break;
        case AC3_CHANNEL_MODE_3F_2R:
            switch (to) {
                case AC3_OUTPUT_MONO:
                    mix_3f_2r_to_mono(ctx->output);
                    break;
                case AC3_OUTPUT_STEREO:
                    mix_3f_2r_to_stereo(ctx->output);
                    break;
            }
            break;
    }
}

static void do_imdct_256(AC3DecodeContext *ctx, int ch)
{
    int k;
    float x[128];
    FFTComplex z[2][64];
    float *o_ptr = ctx->tmp_output;
    int i;

    for(i=0; i<2; i++) {
        /* de-interleave coefficients */
        for(k=0; k<128; k++) {
            x[k] = ctx->transform_coeffs[ch][2*k+i];
        }

        /* run standard IMDCT */
        ctx->imdct_256.fft.imdct_calc(&ctx->imdct_256, o_ptr, x, ctx->tmp_imdct);

        /* reverse the post-rotation & reordering from standard IMDCT */
        for(k=0; k<32; k++) {
            z[i][32+k].re = -o_ptr[128+2*k];
            z[i][32+k].im = -o_ptr[2*k];
            z[i][31-k].re =  o_ptr[2*k+1];
            z[i][31-k].im =  o_ptr[128+2*k+1];
        }
    }

    /* apply AC-3 post-rotation & reordering */
    for(k=0; k<64; k++) {
        o_ptr[    2*k  ] = -z[0][   k].im;
        o_ptr[    2*k+1] =  z[0][63-k].re;
        o_ptr[128+2*k  ] = -z[0][   k].re;
        o_ptr[128+2*k+1] =  z[0][63-k].im;
        o_ptr[256+2*k  ] = -z[1][   k].re;
        o_ptr[256+2*k+1] =  z[1][63-k].im;
        o_ptr[384+2*k  ] =  z[1][   k].im;
        o_ptr[384+2*k+1] = -z[1][63-k].re;
    }
}

/**
 * Performs Inverse MDCT transform
 */
static inline void do_imdct(AC3DecodeContext *ctx)
{
    int ch;

    for(ch=0; ch<ctx->nchans; ch++) {
        if(ctx->blksw[ch]) {
            /* 256-point IMDCT */
            do_imdct_256(ctx, ch);
        } else {
            /* 512-point IMDCT */
            ctx->imdct_512.fft.imdct_calc(&ctx->imdct_512, ctx->tmp_output,
                                          ctx->transform_coeffs[ch],
                                          ctx->tmp_imdct);
        }
        /* apply window function, overlap/add output, save delay */
        ctx->dsp.vector_fmul_add_add(ctx->output[ch], ctx->tmp_output,
                                     ctx->window, ctx->delay[ch], 0,
                                     AC3_BLOCK_SIZE, 1);
        ctx->dsp.vector_fmul_reverse(ctx->delay[ch], ctx->tmp_output+256,
                                     ctx->window, AC3_BLOCK_SIZE);
    }
}

/**
 * Parses the audio block from AC-3 bitstream.
 * This function extract the audio block from the AC-3 bitstream
 * and produces the output for the block. It must be called for each of the six
 * audio block in the AC-3 bitstream.
 */
static int ac3_parse_audio_block(AC3DecodeContext *ctx)
{
    GetBitContext *gb = &ctx->gb;
    uint8_t *dexps;
    int nfchans = ctx->nfchans;
    int acmod = ctx->acmod;
    int i, ch, bnd, seg;
    int got_cplch;

    memset(ctx->bit_alloc_stages, 0, sizeof(ctx->bit_alloc_stages));

    /*block switch flag */
    memset(ctx->blksw, 0, sizeof(ctx->blksw));
    for(ch=0; ch<nfchans; ch++)
        ctx->blksw[ch] = get_bits1(gb);

    /* dithering flag */
    memset(ctx->dithflag, 0, sizeof(ctx->dithflag));
    for(ch=0; ch<nfchans; ch++)
        ctx->dithflag[ch] = get_bits1(gb);

    /* dynamic range */
    if (get_bits1(gb)) {
        ctx->dynrng[0] = dynrng_tbl[get_bits(gb, 8)];
    }
    /* dynamic range 1+1 mode */
    if (acmod == AC3_CHANNEL_MODE_DUALMONO && get_bits1(gb)) {
        ctx->dynrng[1] = dynrng_tbl[get_bits(gb, 8)];
    }

    /* coupling strategy */
    if (get_bits1(gb)) {
        memset(ctx->bit_alloc_stages, 3, sizeof(ctx->bit_alloc_stages));
        ctx->cplinu = get_bits1(gb);
        memset(ctx->cplbndstrc, 0, sizeof(ctx->cplbndstrc));
        memset(ctx->chincpl, 0, sizeof(ctx->chincpl));
        if (ctx->cplinu) {
            for(ch=0; ch<nfchans; ch++)
                ctx->chincpl[ch] = get_bits1(gb);

            if (acmod == AC3_CHANNEL_MODE_STEREO)
                ctx->phsflginu = get_bits1(gb);

            ctx->cplbegf = get_bits(gb, 4);
            ctx->cplendf = get_bits(gb, 4);

            if (3 + ctx->cplendf - ctx->cplbegf < 0) {
                av_log(NULL, AV_LOG_ERROR, "cplendf = %d < cplbegf = %d\n",
                       ctx->cplendf, ctx->cplbegf);
                return -1;
            }

            ctx->ncplbnd = ctx->ncplsubnd = 3 + ctx->cplendf - ctx->cplbegf;
            ctx->cplstrtmant = ctx->cplbegf * 12 + 37;
            ctx->cplendmant = ctx->cplendf * 12 + 73;
            /* coupling band structure */
            for(bnd=0; bnd<ctx->ncplsubnd-1; bnd++) {
                if (get_bits1(gb)) {
                    ctx->cplbndstrc[bnd] = 1;
                    ctx->ncplbnd--;
                }
            }
        }
    }

    if (ctx->cplinu) {
        /* coupling coordinates */
        int mstrcplco, cplcoexp, cplcomant;
        ctx->cplcoe = 0;
        for(ch=0; ch<nfchans; ch++) {
            if (ctx->chincpl[ch]) {
                if (get_bits1(gb)) {
                    ctx->cplcoe |= 1 << ch;
                    mstrcplco = 3 * get_bits(gb, 2);
                    for (bnd = 0; bnd < ctx->ncplbnd; bnd++) {
                        cplcoexp = get_bits(gb, 4);
                        cplcomant = get_bits(gb, 4);
                        if (cplcoexp == 15)
                            ctx->cplco[ch][bnd] = cplcomant / 16.0f;
                        else
                            ctx->cplco[ch][bnd] = (cplcomant + 16.0f) / 32.0f;
                        ctx->cplco[ch][bnd] *= scale_factors[cplcoexp+mstrcplco];
                    }
                }
            }
        }
        /* phase flags */
        if (acmod == AC3_CHANNEL_MODE_STEREO && ctx->phsflginu &&
                (ctx->cplcoe & 1 || ctx->cplcoe & 2)) {
            for (bnd = 0; bnd < ctx->ncplbnd; bnd++) {
                if (get_bits1(gb))
                    ctx->cplco[1][bnd] = -ctx->cplco[1][bnd];
            }
        }
    }

    /* rematrixing */
    if (acmod == AC3_CHANNEL_MODE_STEREO) {
        memset(ctx->rematflg, 0, sizeof(ctx->rematflg));
        ctx->rematstr = get_bits1(gb);
        if (ctx->rematstr) {
            ctx->nrematbnd = 4;
            if(ctx->cplinu)
                ctx->nrematbnd -= (ctx->cplbegf==0) + (ctx->cplbegf<=2);
            for(bnd=0; bnd<ctx->nrematbnd; bnd++)
                ctx->rematflg[bnd] = get_bits1(gb);
        }
    }

    /* coupling and channel exponent strategies */
    got_cplch = !ctx->cplinu;
    for(ch=0; ch<nfchans; ch++) {
        if(!got_cplch) {
            ch = ctx->cpl_channel;
            got_cplch = 1;
        }
        ctx->expstr[ch] = get_bits(gb, 2);
        if(ctx->expstr[ch] != EXP_REUSE) {
            ctx->bit_alloc_stages[ch] = 3;
        }
        if(ch == ctx->cpl_channel)
            ch = -1;
    }
    /* lfe exponent strategy */
    if(ctx->lfeon) {
        ctx->expstr[ctx->lfe_channel] = get_bits1(gb);
        if(ctx->expstr[ctx->lfe_channel] != EXP_REUSE) {
            ctx->bit_alloc_stages[ctx->lfe_channel] = 3;
        }
    }

    /* channel bandwidth code */
    for(ch=0; ch<nfchans; ch++) {
        if(ctx->expstr[ch] != EXP_REUSE) {
            if(ctx->chincpl[ch]) {
                ctx->endmant[ch] = ctx->cplstrtmant;
            } else {
                int chbwcod = get_bits(gb, 6);
                if(chbwcod > 60) {
                    av_log(NULL, AV_LOG_ERROR, "chbwcod = %d > 60", chbwcod);
                    return -1;
                }
                ctx->endmant[ch] = chbwcod * 3 + 73;
            }
        }
    }
    if(ctx->lfeon) {
        ctx->endmant[ctx->lfe_channel] = 7;
    }

    /* coupling exponents */
    if (ctx->cplinu && ctx->expstr[ctx->cpl_channel] != EXP_REUSE) {
        int cplabsexp = get_bits(gb, 4) << 1;
        int grpsize = 3 << (ctx->expstr[ctx->cpl_channel] - 1);
        int ngrps = (ctx->cplendmant - ctx->cplstrtmant) / grpsize;
        dexps = ctx->dexps[ctx->cpl_channel];
        decode_exponents(gb, ctx->expstr[ctx->cpl_channel], ngrps, cplabsexp,
                         dexps + ctx->cplstrtmant);
    }
    /* fbw & lfe exponents */
    for(ch=0; ch<ctx->nchans; ch++) {
        if (ctx->expstr[ch] != EXP_REUSE) {
            int ngrps = 2;
            if(ch != ctx->lfe_channel) {
                int grpsize = 3 << (ctx->expstr[ch] - 1);
                ngrps = (ctx->endmant[ch] + grpsize - 4) / grpsize;
            }
            dexps = ctx->dexps[ch];
            dexps[0] = get_bits(gb, 4);
            decode_exponents(gb, ctx->expstr[ch], ngrps, dexps[0], dexps + 1);
            if(ch != ctx->lfe_channel)
                skip_bits(gb, 2); // skip gainrng
        }
    }

    /* bit allocation information */
    if (get_bits1(gb)) {
        ctx->bit_alloc_params.sdecay = ff_sdecaytab[get_bits(gb, 2)] >> ctx->halfratecod;
        ctx->bit_alloc_params.fdecay = ff_fdecaytab[get_bits(gb, 2)] >> ctx->halfratecod;
        ctx->bit_alloc_params.sgain = ff_sgaintab[get_bits(gb, 2)];
        ctx->bit_alloc_params.dbknee = ff_dbkneetab[get_bits(gb, 2)];
        ctx->bit_alloc_params.floor = ff_floortab[get_bits(gb, 3)];
        for(ch=0; ch<=AC3_MAX_CHANNELS; ch++) {
            ctx->bit_alloc_stages[ch] = FFMAX(ctx->bit_alloc_stages[ch], 2);
        }
    }

    /* snroffset */
    if (get_bits1(gb)) {
        /* coarse snr offset */
        ctx->csnroffst = get_bits(gb, 6);

        /* fine snr offsets and fast gain codes */
        got_cplch = !ctx->cplinu;
        for(ch=0; ch<ctx->nchans; ch++) {
            if(!got_cplch) {
                ch = ctx->cpl_channel;
                got_cplch = 1;
            }
            ctx->fsnroffst[ch] = get_bits(gb, 4);
            ctx->fgaincod[ch] = get_bits(gb, 3);
            if(ch == ctx->cpl_channel)
                ch = -1;
        }
        memset(ctx->bit_alloc_stages, 3, sizeof(ctx->bit_alloc_stages));
    }

    /* coupling leak information */
    if (ctx->cplinu && get_bits1(gb)) {
        ctx->bit_alloc_params.cplfleak = get_bits(gb, 3);
        ctx->bit_alloc_params.cplsleak = get_bits(gb, 3);
        ctx->bit_alloc_stages[ctx->cpl_channel] = FFMAX(ctx->bit_alloc_stages[ctx->cpl_channel], 2);
    }

    /* delta bit allocation information */
    if (get_bits1(gb)) {
        /* bit allocation exists (new/reuse/none) */
        if(ctx->cplinu) {
            ctx->deltbae[ctx->cpl_channel] = get_bits(gb, 2);
        }
        for(ch=0; ch<nfchans; ch++) {
            ctx->deltbae[ch] = get_bits(gb, 2);
        }

        /* delta offset, len and bit allocation */
        got_cplch = !ctx->cplinu;
        for(ch=0; ch<nfchans; ch++) {
            if(!got_cplch) {
                ch = ctx->cpl_channel;
                got_cplch = 1;
            }
            if (ctx->deltbae[ch] == DBA_NEW) {
                ctx->deltnseg[ch] = get_bits(gb, 3);
                for (seg = 0; seg <= ctx->deltnseg[ch]; seg++) {
                    ctx->deltoffst[ch][seg] = get_bits(gb, 5);
                    ctx->deltlen[ch][seg] = get_bits(gb, 4);
                    ctx->deltba[ch][seg] = get_bits(gb, 3);
                }
            }
            ctx->bit_alloc_stages[ch] = FFMAX(ctx->bit_alloc_stages[ch], 2);
            if(ch == ctx->cpl_channel)
                ch = -1;
        }
    }

    /* run bit allocation */
    if(ctx->cplinu && ctx->bit_alloc_stages[ctx->cpl_channel]) {
        do_bit_allocation(ctx, ctx->cpl_channel);
    }
    for(ch=0; ch<ctx->nchans; ch++) {
        if(ctx->bit_alloc_stages[ch]) {
            do_bit_allocation(ctx, ch);
        }
    }

    /* unused dummy data */
    if(get_bits1(gb)) {
        int skipl = get_bits(gb, 9);
        while(skipl--)
            skip_bits(gb, 8);
    }

    /* unpack the transform coefficients
       this also uncouples channels if coupling is in use. */
    get_transform_coeffs(ctx);

    /* recover left & right coefficients if rematrixing is in use */
    if(ctx->acmod == AC3_CHANNEL_MODE_STEREO)
        do_rematrixing(ctx);

    /* apply downmix and dynamic range scaling */
    update_downmix_scaling(ctx);
    for(ch=0; ch<ctx->nchans; ch++) {
        float gain = 1.0f;
        if(ctx->acmod == AC3_CHANNEL_MODE_DUALMONO) {
            gain = ctx->downmix_scale_factors[ch] * 2.0f * ctx->dynrng[ch];
        } else {
            gain = ctx->downmix_scale_factors[ch] * 2.0f * ctx->dynrng[0];
        }
        for(i=0; i<ctx->endmant[ch]; i++) {
            ctx->transform_coeffs[ch][i] *= gain;
        }
    }

    do_imdct(ctx);

    do_downmix(ctx);

    /* convert float to 16-bit integer */
    for(ch=0; ch<ctx->out_channels; ch++) {
        for(i=0; i<AC3_BLOCK_SIZE; i++) {
            ctx->output[ch][i] = ctx->output[ch][i] * ctx->mul_bias +
                                 ctx->add_bias;
        }
        ctx->dsp.float_to_int16(ctx->int_output[ch], ctx->output[ch],
                                AC3_BLOCK_SIZE);
    }

    return 0;
}

/**
 * Decodes an AC-3 frame.
 */
static int ac3_decode_frame(AVCodecContext *avctx, void *data, int *data_size,
                            uint8_t *buf, int buf_size)
{
    AC3DecodeContext *ctx = (AC3DecodeContext *)avctx->priv_data;
    int16_t *out_samples = (int16_t *)data;
    int i, j, k;
    int err;

    /* initialize the GetBitContext */
    init_get_bits(&(ctx->gb), buf, buf_size * 8);

    /* parse the AC-3 frame header
       If 'fscod' or 'bsid' is not valid the decoder shall mute as per the
       standard. */
    err = parse_header(ctx);
    if(err) {
        if(err == -2) {
            av_log(avctx, AV_LOG_ERROR, "E-AC-3 decoding not supported\n");
        } else {
            av_log(avctx, AV_LOG_ERROR, "invalid AC-3 frame\n");
        }
        *data_size = 0;
        return buf_size;
    }

    avctx->sample_rate = ctx->sample_rate;
    avctx->bit_rate = ctx->bit_rate;

    /* set output mode */
    ctx->blkoutput = 0;
    if (avctx->channels == 1) {
        ctx->blkoutput |= AC3_OUTPUT_MONO;
    } else if (avctx->channels == 2) {
        ctx->blkoutput |= AC3_OUTPUT_STEREO;
    } else {
        if (avctx->channels && avctx->channels < ctx->nchans)
            av_log(avctx, AV_LOG_INFO, "ac3_decoder: AC3 Source Channels Are Less Then Specified %d: Output to %d Channels\n",avctx->channels, ctx->nfchans + ctx->lfeon);
        ctx->blkoutput |= AC3_OUTPUT_UNMODIFIED;
        if (ctx->lfeon)
            ctx->blkoutput |= AC3_OUTPUT_LFEON;
        avctx->channels = ctx->nchans;
    }
    ctx->out_channels = avctx->channels;

    /* parse the audio blocks */
    for (i = 0; i < NB_BLOCKS; i++) {
        if (ac3_parse_audio_block(ctx)) {
            av_log(avctx, AV_LOG_ERROR, "error parsing the audio block\n");
            *data_size = 0;
            return ctx->frame_size;
        }
        /* interleave output */
        for (k = 0; k < AC3_BLOCK_SIZE; k++) {
            for (j = 0; j < avctx->channels; j++) {
                *(out_samples++) = ctx->int_output[j][k];
            }
        }
    }
    *data_size = AC3_FRAME_SIZE * avctx->channels * sizeof (int16_t);
    return ctx->frame_size;
}

/**
 * Uninitializes AC-3 decoder.
 */
static int ac3_decode_end(AVCodecContext *avctx)
{
    AC3DecodeContext *ctx = (AC3DecodeContext *)avctx->priv_data;
    ff_mdct_end(&ctx->imdct_512);
    ff_mdct_end(&ctx->imdct_256);

    return 0;
}

AVCodec ac3_decoder = {
    .name = "ac3",
    .type = CODEC_TYPE_AUDIO,
    .id = CODEC_ID_AC3,
    .priv_data_size = sizeof (AC3DecodeContext),
    .init = ac3_decode_init,
    .close = ac3_decode_end,
    .decode = ac3_decode_frame,
};
