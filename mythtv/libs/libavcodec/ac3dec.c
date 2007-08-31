/*
 * AC-3 Audio Decoder
 * This code was developed as part of the Google Summer of Code 2006 Program.
 *
 * Copyright (c) 2006 Kartikey Mahendra BHATT (bhattkm at gmail dot com)
 * Copyright (c) 2007 Justin Ruggles
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

/**
 * @file ac3dec.c
 * AC-3 Audio Decoder
 * Reference: ATSC Standard: Digital Audio Compression (AC-3), Revision B
 */

#include <stdio.h>
#include <stddef.h>
#include <math.h>
#include <string.h>

#include "avcodec.h"
#include "ac3.h"
#include "parser.h"
#include "bitstream.h"
#include "dsputil.h"
#include "random.h"

/** override ac3.h to include coupling channel */
#undef AC3_MAX_CHANNELS
#define AC3_MAX_CHANNELS 7
#define CPL_CH 0

/**
 * Table which maps acmod to number of full bandwidth channels
 * reference: Table 5.8 Audio Coding Mode
 */
static const uint8_t nfchans_tbl[8] = { 2, 1, 2, 3, 3, 4, 4, 5 };

/**
 * Table of bin locations for rematrixing bands
 * reference: Section 7.5.2 Rematrixing : Frequency Band Definitions
 */
static const uint8_t rematrix_band_tbl[5] = { 13, 25, 37, 61, 253 };

/** table for exponent to scale_factor mapping */
static float scale_factors[25];

/** tables for ungrouping mantissas */
static float b1_mantissas[ 32][3];
static float b2_mantissas[128][3];
static float b3_mantissas[8];
static float b4_mantissas[128][2];
static float b5_mantissas[16];

/** table for grouping exponents */
static uint8_t exp_ungroup_tbl[128][3];

/**
 * Table for number of exponent groups
 * format: nexpgrp_tbl[cplinu][expstr-1][nmant]
 */
static uint8_t nexpgrp_tbl[2][3][256];

/**
 * Quantization table: levels for symmetric. bits for asymmetric.
 * reference: Table 7.18 Mapping of bap to Quantizer
 */
static const uint8_t qntztab[16] = {
    0, 3, 5, 7, 11, 15,
    5, 6, 7, 8, 9, 10, 11, 12, 14, 16
};

/** dynamic range table. converts codes to scale factors. */
static float dynrng_tbl[256];

/** dialogue normalization table */
static float dialnorm_tbl[32];

/** adjustments in dB gain */
#define LEVEL_ZERO              (0.0000000000000000f)
#define LEVEL_ONE               (1.0000000000000000f)
#define LEVEL_MINUS_3DB         (0.7071067811865476f) /* sqrt(2)/2 */
#define LEVEL_MINUS_4POINT5DB   (0.5946035575013605f) /* fourth-root(2)/2 */
#define LEVEL_MINUS_6DB         (0.5000000000000000f)
#define LEVEL_MINUS_9DB         (0.3535533905932738f) /* sqrt(2)/4 */

static const float gain_levels[6] = {
    LEVEL_ZERO,
    LEVEL_ONE,
    LEVEL_MINUS_3DB,
    LEVEL_MINUS_4POINT5DB,
    LEVEL_MINUS_6DB,
    LEVEL_MINUS_9DB
};

/**
 * Table for center mix levels
 * reference: Section 5.4.2.4 cmixlev
 */
static const uint8_t clevs[4] = { 2, 3, 4, 3 };

/**
 * Table for surround mix levels
 * reference: Section 5.4.2.5 surmixlev
 */
static const uint8_t slevs[4] = { 2, 4, 0, 4 };

/**
 * Table for default stereo downmixing coefficients
 * reference: Section 7.8.2 Downmixing Into Two Channels
 */
static const uint8_t ac3_default_coeffs[8][5][2] = {
    { { 1, 0 }, { 0, 1 },                               },
    { { 2, 2 },                                         },
    { { 1, 0 }, { 0, 1 },                               },
    { { 1, 0 }, { 3, 3 }, { 0, 1 },                     },
    { { 1, 0 }, { 0, 1 }, { 4, 4 },                     },
    { { 1, 0 }, { 3, 3 }, { 0, 1 }, { 5, 5 },           },
    { { 1, 0 }, { 0, 1 }, { 4, 0 }, { 0, 4 },           },
    { { 1, 0 }, { 3, 3 }, { 0, 1 }, { 4, 0 }, { 0, 4 }, },
};

typedef struct {
    int     acmod;                              ///< audio coding mode
    int     blksw[AC3_MAX_CHANNELS];            ///< block switch flags
    int     dithflag[AC3_MAX_CHANNELS];         ///< dither flags
    int     cplinu;                             ///< coupling in use
    int     chincpl[AC3_MAX_CHANNELS];          ///< channel in coupling
    int     phsflginu;                          ///< phase flag in use
    int     cplbegf;                            ///< coupling start band code
    int     cplendf;                            ///< coupling end band code
    int     cplcoe;                             ///< coupling coeffs exist
    int     cplbndstrc[18];                     ///< coupling band structure
    int     nrematbnd;                          ///< number of rematrixing bands
    int     rematstr;                           ///< rematrixing strategy
    int     rematflg[4];                        ///< rematrixing flags
    int     expstr[AC3_MAX_CHANNELS];           ///< exponent strategies

    AC3BitAllocParameters bit_alloc_params;     ///< bit allocation parameters
    int     sample_rate;                        ///< sample rate in Hz
    int     bit_rate;                           ///< bit rate in kbps
    int     frame_size;                         ///< frame size in bytes
    int     nfchans;                            ///< number of full-bandwidth channels
    int     nchans;                             ///< number of total channels, including LFE
    int     lfeon;                              ///< indicates if LFE channel is present
    int     lfe_channel;                        ///< index of LFE channel
    int     output_mode;                        ///< output channel configuration
    int     out_channels;                       ///< number of output channels

    float   dialnorm[2];                        ///< dialogue normalization
    float   downmix_coeffs[AC3_MAX_CHANNELS][2];///< stereo downmix coefficients
    float   dynrng[2];                          ///< dynamic range scale factors

    float   cplco[AC3_MAX_CHANNELS][18];        ///< coupling coordinates
    int     ncplbnd;                            ///< number of coupling bands
    int     ncplsubnd;                          ///< number of coupling sub-bands
    int     strtmant[AC3_MAX_CHANNELS];         ///< start mantissa
    int     endmant[AC3_MAX_CHANNELS];          ///< end mantissa
    int     nmant[AC3_MAX_CHANNELS];            ///< number of mantissas
    int     snroffst[AC3_MAX_CHANNELS];         ///< snr offsets
    int     fgain[AC3_MAX_CHANNELS];            ///< fast gain values
    int     bit_alloc_stages[AC3_MAX_CHANNELS]; ///< used to speed up bit allocation by eliminating unneeded stages

    uint8_t dexps[AC3_MAX_CHANNELS][256];       ///< decoded exponents
    uint8_t bap[AC3_MAX_CHANNELS][256];         ///< bit allocation pointers
    int16_t psd[AC3_MAX_CHANNELS][256];         ///< scaled exponents
    int16_t bndpsd[AC3_MAX_CHANNELS][50];       ///< interpolated exponents
    int16_t mask[AC3_MAX_CHANNELS][50];         ///< masking curve values
    int     deltbae[AC3_MAX_CHANNELS];          ///< delta bit alloc exists
    int     deltnseg[AC3_MAX_CHANNELS];         ///< number of delta segments
    uint8_t deltoffst[AC3_MAX_CHANNELS][8];     ///< delta segment offsets
    uint8_t deltlen[AC3_MAX_CHANNELS][8];       ///< delta segment lengths
    uint8_t deltba[AC3_MAX_CHANNELS][8];        ///< bit allocation deltas

    DECLARE_ALIGNED_16(float, transform_coeffs[AC3_MAX_CHANNELS][256]); ///< frequency domain coefficients
    DECLARE_ALIGNED_16(float, output[AC3_MAX_CHANNELS][256]);           ///< output after imdct transform and windowing
    DECLARE_ALIGNED_16(short, int_output[AC3_MAX_CHANNELS][256]);       ///< final 16-bit integer output
    DECLARE_ALIGNED_16(float, delay[AC3_MAX_CHANNELS][256]);            ///< delay - added to the next block
    DECLARE_ALIGNED_16(float, tmp_imdct[256]);                          ///< temp storage for imdct transform
    DECLARE_ALIGNED_16(float, tmp_output[512]);                         ///< temp storage for output before windowing
    DECLARE_ALIGNED_16(float, window[256]);                             ///< window coefficients

    MDCTContext     imdct_512;                  ///< for 512 sample imdct transform
    MDCTContext     imdct_256;                  ///< for 256 sample imdct transform
    DSPContext      dsp;                        ///< for optimization
    float           add_bias;                   ///< offset for float_to_int16 conversion
    float           mul_bias;                   ///< scaling for float_to_int16 conversion
    GetBitContext   gb;                         ///< bit-wise reader
    AVRandomState   dith_state;                 ///< for dither generation
    AVCodecContext *avctx;                      ///< reference to the parent codec context
} AC3DecodeContext;

/**
 * Generates a Kaiser-Bessel Derived Window.
 */
static void ac3_window_init(float *window)
{
   int i, j;
   double sum = 0.0, bessel, tmp;
   double local_window[256];
   double alpha2 = (5.0 * M_PI / 256.0) * (5.0 * M_PI / 256.0);

   for(i=0; i<256; i++) {
       tmp = i * (256 - i) * alpha2;
       bessel = 1.0;
       for(j=100; j>0; j--) /* default to 100 iterations */
           bessel = bessel * tmp / (j * j) + 1;
       sum += bessel;
       local_window[i] = sum;
   }

   sum++;
   for(i=0; i<256; i++)
       window[i] = sqrt(local_window[i] / sum);
}

static inline float
symmetric_dequant(int code, int levels)
{
    return (code - (levels >> 1)) * (2.0f / levels);
}

static void ac3_decoder_tables_init(void)
{
    int i, expstr, cplinu;

    /* generate grouped mantissa tables
       reference: Section 7.3.5 Ungrouping of Mantissas */
    for(i=0; i<32; i++) {
        /* bap=1 mantissas */
        b1_mantissas[i][0] = symmetric_dequant( i / 9     , 3);
        b1_mantissas[i][1] = symmetric_dequant((i % 9) / 3, 3);
        b1_mantissas[i][2] = symmetric_dequant((i % 9) % 3, 3);
    }
    for(i=0; i<128; i++) {
        /* bap=2 mantissas */
        b2_mantissas[i][0] = symmetric_dequant( i / 25     , 5);
        b2_mantissas[i][1] = symmetric_dequant((i % 25) / 5, 5);
        b2_mantissas[i][2] = symmetric_dequant((i % 25) % 5, 5);

        /* bap=4 mantissas */
        b4_mantissas[i][0] = symmetric_dequant(i / 11, 11);
        b4_mantissas[i][1] = symmetric_dequant(i % 11, 11);
    }
    /* generate ungrouped mantissa tables
       reference: Tables 7.21 and 7.23 */
    for(i=0; i<7; i++) {
        /* bap=3 mantissas */
        b3_mantissas[i] = symmetric_dequant(i, 7);
    }
    for(i=0; i<15; i++) {
        /* bap=5 mantissas */
        b5_mantissas[i] = symmetric_dequant(i, 15);
    }

    /* generate dynamic range table
       reference: Section 7.7.1 Dynamic Range Control */
    for(i=0; i<256; i++) {
        int v = (i >> 5) - ((i >> 7) << 3) - 5;
        dynrng_tbl[i] = powf(2.0f, v) * ((i & 0x1F) | 0x20);
    }

    /* generate dialogue normalization table
       references: Section 5.4.2.8 dialnorm
                   Section 7.6 Dialogue Normalization */
    for(i=1; i<32; i++) {
        dialnorm_tbl[i] = expf((i-31) * M_LN10 / 20.0f);
    }
    dialnorm_tbl[0] = dialnorm_tbl[31];

    /* generate scale factors */
    for(i=0; i<25; i++)
        scale_factors[i] = pow(2.0, -i);

    /* generate exponent tables
       reference: Section 7.1.3 Exponent Decoding */
    for(i=0; i<128; i++) {
        exp_ungroup_tbl[i][0] =  i / 25;
        exp_ungroup_tbl[i][1] = (i % 25) / 5;
        exp_ungroup_tbl[i][2] = (i % 25) % 5;
    }

    /** generate table for number of exponent groups
        reference: Section 7.1.3 Exponent Decoding */
    for(cplinu=0; cplinu<=1; cplinu++) {
        for(expstr=EXP_D15; expstr<=EXP_D45; expstr++) {
            for(i=0; i<256; i++) {
                int grpsize = expstr + (expstr == EXP_D45);
                int ngrps = 0;
                if(cplinu) {
                    ngrps = i / (3 * grpsize);
                } else {
                    if(i == 7)
                        ngrps = 2;
                    else
                        ngrps = (i + (grpsize * 3) - 4) / (3 * grpsize);
                }
                nexpgrp_tbl[cplinu][expstr-1][i] = ngrps;
            }
        }
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

    /* initialize delay samples to 0 */
    for(ch=0; ch<AC3_MAX_CHANNELS; ch++) {
        memset(ctx->delay[ch], 0, sizeof(ctx->delay[ch]));
    }

    ctx->output_mode = AC3_ACMOD_STEREO;
    ctx->avctx = avctx;

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
    float cmixlev;      // center mix level
    float surmixlev;    // surround mix level
    int err, i;

    err = ff_ac3_parse_header(gb->buffer, &hdr);
    if(err)
        return err;

    /* get decoding parameters from header info */
    ctx->bit_alloc_params.fscod       = hdr.fscod;
    ctx->acmod                        = hdr.acmod;
    cmixlev                           = gain_levels[clevs[hdr.cmixlev]];
    surmixlev                         = gain_levels[slevs[hdr.surmixlev]];
    ctx->lfeon                        = hdr.lfeon;
    ctx->bit_alloc_params.halfratecod = hdr.halfratecod;
    ctx->sample_rate                  = hdr.sample_rate;
    ctx->bit_rate                     = hdr.bit_rate / 1000;
    ctx->nchans                       = hdr.channels;
    ctx->nfchans                      = ctx->nchans - ctx->lfeon;
    ctx->lfe_channel                  = ctx->nfchans+1;
    ctx->frame_size                   = hdr.frame_size;
    ctx->output_mode                  = nfchans_tbl[ctx->acmod];
    if(ctx->lfeon)
        ctx->output_mode |= AC3_LFEON;

    /* skip over portion of header which has already been read */
    skip_bits(gb, 16);
    skip_bits(gb, 16);
    skip_bits(gb, 8);
    skip_bits(gb, 11);
    if(ctx->acmod == AC3_ACMOD_STEREO) {
        skip_bits(gb, 2);
    } else {
        if((ctx->acmod & 1) && ctx->acmod != AC3_ACMOD_MONO)
            skip_bits(gb, 2);
        if(ctx->acmod & 4)
            skip_bits(gb, 2);
    }
    skip_bits1(gb);

    /* the rest of the bsi. read twice for dual mono mode. */
    for(i=0; i<=(ctx->acmod == AC3_ACMOD_DUALMONO); i++) {
        ctx->dialnorm[i] = dialnorm_tbl[get_bits(gb, 5)]; // dialogue normalization
        if(get_bits1(gb))
            skip_bits(gb, 8); // skip compression gain
        if(get_bits1(gb))
            skip_bits(gb, 8); // skip language code
        if(get_bits1(gb))
            skip_bits(gb, 7); // skip audio production information
    } while(i--);

    skip_bits(gb, 2); // skip copyright bit and original bitstream bit

    /* FIXME: read & use the xbsi1 downmix levels */
    if(get_bits1(gb))
        skip_bits(gb, 14); // skip timecode1 or xbsi1
    if(get_bits1(gb))
        skip_bits(gb, 14); // skip timecode2 or xbsi2

    if(get_bits1(gb)) {
        i = get_bits(gb, 6); // additional bsi length
        do {
            skip_bits(gb, 8);
        } while(i--);
    }

    /* set stereo downmixing coefficients
       reference: Section 7.8.2 Downmixing Into Two Channels */
    for(i=0; i<ctx->nfchans; i++) {
        ctx->downmix_coeffs[i][0] = gain_levels[ac3_default_coeffs[ctx->acmod][i][0]];
        ctx->downmix_coeffs[i][1] = gain_levels[ac3_default_coeffs[ctx->acmod][i][1]];
    }
    if(ctx->acmod > 1 && ctx->acmod & 1) {
        ctx->downmix_coeffs[1][0] = ctx->downmix_coeffs[1][1] = cmixlev;
    }
    if(ctx->acmod == AC3_ACMOD_2F1R || ctx->acmod == AC3_ACMOD_3F1R) {
        int nf = ctx->acmod - 2;
        ctx->downmix_coeffs[nf][0] = ctx->downmix_coeffs[nf][1] = surmixlev * LEVEL_MINUS_3DB;
    }
    if(ctx->acmod == AC3_ACMOD_2F2R || ctx->acmod == AC3_ACMOD_3F2R) {
        int nf = ctx->acmod - 4;
        ctx->downmix_coeffs[nf][0] = ctx->downmix_coeffs[nf+1][1] = surmixlev;
    }

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
    int expacc, prevexp;

    /* unpack groups */
    grpsize = expstr + (expstr == EXP_D45);
    for(grp=0,i=0; grp<ngrps; grp++) {
        expacc = get_bits(gb, 7);
        dexp[i++] = exp_ungroup_tbl[expacc][0];
        dexp[i++] = exp_ungroup_tbl[expacc][1];
        dexp[i++] = exp_ungroup_tbl[expacc][2];
    }

    /* convert to absolute exps and expand groups */
    prevexp = absexp;
    for(i=0; i<ngrps*3; i++) {
        prevexp = av_clip(prevexp + dexp[i]-2, 0, 24);
        for(j=0; j<grpsize; j++) {
            dexps[(i*grpsize)+j] = prevexp;
        }
    }
}

/**
 * Generates transform coefficients for each coupled channel in the coupling
 * range using the coupling coefficients and coupling coordinates.
 * reference: Section 7.4.3 Coupling Coordinate Format
 */
static void uncouple_channels(AC3DecodeContext *ctx)
{
    int i, j, ch, bnd, subbnd;

    subbnd = 0;
    i = ctx->strtmant[CPL_CH];
    for(bnd=0; bnd<ctx->ncplbnd; bnd++) {
        do {
            for(j=0; j<12; j++) {
                for(ch=1; ch<=ctx->nfchans; ch++) {
                    if(ctx->chincpl[ch]) {
                        ctx->transform_coeffs[ch][i] =
                                ctx->transform_coeffs[CPL_CH][i] *
                                ctx->cplco[ch][bnd] * 8.0f;
                    }
                }
                i++;
            }
        } while(ctx->cplbndstrc[subbnd++]);
    }
}

/** ungrouped mantissas */
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
    int i, gcode, dithflag;
    float *coeffs;

    dithflag = ctx->dithflag[ch];
    coeffs = ctx->transform_coeffs[ch];

    for(i=ctx->strtmant[ch]; i<ctx->endmant[ch]; i++) {
        int tbap = ctx->bap[ch][i];
        switch(tbap) {
            case 0:
                if(!dithflag) {
                    coeffs[i] = 0.0f;
                } else {
                    coeffs[i] = LEVEL_MINUS_3DB;
                }
                break;
            case 1:
                if(m->b1ptr > 2) {
                    gcode = get_bits(gb, 5);
                    m->b1_mant[0] = b1_mantissas[gcode][0];
                    m->b1_mant[1] = b1_mantissas[gcode][1];
                    m->b1_mant[2] = b1_mantissas[gcode][2];
                    m->b1ptr = 0;
                }
                coeffs[i] = m->b1_mant[m->b1ptr++];
                break;
            case 2:
                if(m->b2ptr > 2) {
                    gcode = get_bits(gb, 7);
                    m->b2_mant[0] = b2_mantissas[gcode][0];
                    m->b2_mant[1] = b2_mantissas[gcode][1];
                    m->b2_mant[2] = b2_mantissas[gcode][2];
                    m->b2ptr = 0;
                }
                coeffs[i] = m->b2_mant[m->b2ptr++];
                break;
            case 3:
                coeffs[i] = b3_mantissas[get_bits(gb, 3)];
                break;
            case 4:
                if(m->b4ptr > 1) {
                    gcode = get_bits(gb, 7);
                    m->b4_mant[0] = b4_mantissas[gcode][0];
                    m->b4_mant[1] = b4_mantissas[gcode][1];
                    m->b4ptr = 0;
                }
                coeffs[i] = m->b4_mant[m->b4ptr++];
                break;
            case 5:
                coeffs[i] = b5_mantissas[get_bits(gb, 4)];
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
 * Applies random dithering to coefficients
 * reference: Section 7.3.4 Dither for Zero Bit Mantissas (bap=0)
 */
static void apply_dithering(AC3DecodeContext *ctx) {
    int ch, i;
    int end=0;
    float *coeffs;

    for(ch=1; ch<=ctx->nfchans; ch++) {
        coeffs = ctx->transform_coeffs[ch];
        if(ctx->chincpl[ch])
            end = ctx->endmant[CPL_CH];
        else
            end = ctx->endmant[ch];
        if(ctx->dithflag[ch]) {
            for(i=0; i<end; i++) {
                if(ctx->bap[ch][i] == 0) {
                    coeffs[i] *= (av_random(&ctx->dith_state) & 0xFFFF) / 32768.0f;
                }
            }
        }
    }
}

/**
 * Gets the transform coefficients.
 * This function extracts the tranform coefficients from the AC-3 bitstream
 * using the previously decoded bit allocation pointers.  If coupling is in
 * use, coupled coefficients are also reconstructed here.
 */
static void get_transform_coeffs(AC3DecodeContext *ctx)
{
    int ch, end;
    int got_cplch = 0;
    mant_groups m;

    m.b1ptr = m.b2ptr = m.b4ptr = 3;

    for(ch=1; ch<=ctx->nchans; ch++) {
        /* transform coefficients for individual channel */
        get_transform_coeffs_ch(ctx, ch, &m);

        /* tranform coefficients for coupling channel */
        if(ctx->chincpl[ch]) {
            if(!got_cplch) {
                get_transform_coeffs_ch(ctx, CPL_CH, &m);
                uncouple_channels(ctx);
                got_cplch = 1;
            }
            end = ctx->endmant[CPL_CH];
        } else {
            end = ctx->endmant[ch];
        }
        memset(&ctx->transform_coeffs[ch][end], 0, (256-end)*sizeof(float));
    }
    apply_dithering(ctx);
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

    end = FFMIN(ctx->endmant[1], ctx->endmant[2]);

    for(bnd=0; bnd<ctx->nrematbnd; bnd++) {
        if(ctx->rematflg[bnd]) {
            bndend = FFMIN(end, rematrix_band_tbl[bnd+1]);
            for(i=rematrix_band_tbl[bnd]; i<bndend; i++) {
                tmp0 = ctx->transform_coeffs[1][i];
                tmp1 = ctx->transform_coeffs[2][i];
                ctx->transform_coeffs[1][i] = tmp0 + tmp1;
                ctx->transform_coeffs[2][i] = tmp0 - tmp1;
            }
        }
    }
}

/**
 * Downmixes the output to stereo.
 */
static void ac3_downmix(float samples[AC3_MAX_CHANNELS][256], int nfchans,
                        int output_mode, float coef[AC3_MAX_CHANNELS][2])
{
    int i, j;
    float v0, v1, s0, s1;

    for(i=0; i<256; i++) {
        v0 = v1 = s0 = s1 = 0.0f;
        for(j=0; j<nfchans; j++) {
            v0 += samples[j][i] * coef[j][0];
            v1 += samples[j][i] * coef[j][1];
            s0 += coef[j][0];
            s1 += coef[j][1];
        }
        // HACK: 5.1 downmixed to stereo is around 10 dB quieter than
        // alternate mp2 audio tracks. to be on the safe side
        // we multiply only by 8 which happens to be the sum of the
        // downmixing coefficients for 5.1
        // ffmpeg rev 10252 shows a different effect so this has to
        // be handled when we sync to this revision
        v0 /= s0 / 8;
        v1 /= s1 / 8;
        // END HACK
        if(output_mode == AC3_ACMOD_MONO) {
            samples[0][i] = (v0 + v1) * LEVEL_MINUS_3DB;
        } else if(output_mode == AC3_ACMOD_STEREO) {
            samples[0][i] = v0;
            samples[1][i] = v1;
        }
    }
}

static void do_imdct_256(AC3DecodeContext *ctx, int ch)
{
    int k;
    DECLARE_ALIGNED_16(float, x[128]);
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

    for(ch=1; ch<=ctx->nchans; ch++) {
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
        ctx->dsp.vector_fmul_add_add(ctx->output[ch-1], ctx->tmp_output,
                                     ctx->window, ctx->delay[ch-1], 0,
                                     256, 1);
        ctx->dsp.vector_fmul_reverse(ctx->delay[ch-1], ctx->tmp_output+256,
                                     ctx->window, 256);
    }
}

/**
 * Parses the audio block from AC-3 bitstream.
 * This function extract the audio block from the AC-3 bitstream
 * and produces the output for the block. It must be called for each of the six
 * audio block in the AC-3 bitstream.
 */
static int ac3_parse_audio_block(AC3DecodeContext *ctx, int blk)
{
    GetBitContext *gb = &ctx->gb;
    int i, ch, bnd, seg;

    memset(ctx->bit_alloc_stages, 0, sizeof(ctx->bit_alloc_stages));

    /* block switch flag */
    for(ch=1; ch<=ctx->nfchans; ch++)
        ctx->blksw[ch] = get_bits1(gb);

    /* dithering flag */
    ctx->dithflag[CPL_CH] = ctx->dithflag[ctx->lfe_channel] = 0;
    for(ch=1; ch<=ctx->nfchans; ch++)
        ctx->dithflag[ch] = get_bits1(gb);

    /* dynamic range */
    if(get_bits1(gb))
        ctx->dynrng[0] = dynrng_tbl[get_bits(gb, 8)];
    else if(blk == 0)
        ctx->dynrng[0] = 1.0f;
    /* dynamic range 1+1 mode */
    if(ctx->acmod == AC3_ACMOD_DUALMONO) {
        if(get_bits1(gb))
            ctx->dynrng[1] = dynrng_tbl[get_bits(gb, 8)];
        else if(blk == 0)
            ctx->dynrng[1] = 1.0f;
    }

    /* coupling strategy */
    if(get_bits1(gb)) {
        memset(ctx->bit_alloc_stages, 3, sizeof(ctx->bit_alloc_stages));
        ctx->cplinu = get_bits1(gb);
        if(ctx->cplinu) {
            int nchincpl = 0;

            /* determine which channels are coupled */
            for(ch=1; ch<=ctx->nfchans; ch++) {
                ctx->chincpl[ch] = get_bits1(gb);
                nchincpl++;
                /* if any coupled channels use dithering, dither the coupling channel */
                if(ctx->chincpl[ch] && ctx->dithflag[ch])
                    ctx->dithflag[CPL_CH] = 1;
            }
            if(nchincpl < 2) {
                av_log(ctx->avctx, AV_LOG_ERROR, "less than 2 coupled channels\n");
                return -1;
            }
            if(ctx->acmod == AC3_ACMOD_DUALMONO) {
                av_log(ctx->avctx, AV_LOG_ERROR, "cannot use coupling in dual mono mode\n");
                return -1;
            }

            /* are phase flags used for stereo coupling */
            if(ctx->acmod == AC3_ACMOD_STEREO)
                ctx->phsflginu = get_bits1(gb);

            /* coupling frequency range */
            ctx->cplbegf = get_bits(gb, 4);
            ctx->cplendf = get_bits(gb, 4);
            if(3 + ctx->cplendf - ctx->cplbegf < 0) {
                av_log(ctx->avctx, AV_LOG_ERROR, "cplendf=%d < cplbegf=%d\n",
                       ctx->cplendf, ctx->cplbegf);
                return -1;
            }
            ctx->ncplbnd = ctx->ncplsubnd = 3 + ctx->cplendf - ctx->cplbegf;
            ctx->strtmant[CPL_CH] = ctx->cplbegf * 12 + 37;
            ctx->endmant[CPL_CH] = ctx->cplendf * 12 + 73;
            ctx->nmant[CPL_CH] = ctx->endmant[CPL_CH] - ctx->strtmant[CPL_CH];

            /* coupling band structure */
            for(bnd=0; bnd<ctx->ncplsubnd-1; bnd++) {
                if (get_bits1(gb)) {
                    ctx->cplbndstrc[bnd] = 1;
                    ctx->ncplbnd--;
                }
            }
        } else {
            /* no channels are in coupling since coupling is not in use */
            memset(ctx->chincpl, 0, sizeof(ctx->chincpl));
        }
    } else if(blk == 0) {
        av_log(ctx->avctx, AV_LOG_ERROR, "no coupling strategy for block 0\n");
        return -1;
    }

    /* coupling coordinates
       reference: Section 7.4.3 Coupling Coordinate Format */
    if(ctx->cplinu) {
        int mstrcplco, cplcoexp, cplcomant;
        ctx->cplcoe = 0;
        for(ch=1; ch<=ctx->nfchans; ch++) {
            if(ctx->chincpl[ch]) {
                if(get_bits1(gb)) {
                    ctx->cplcoe |= 1 << ch;
                    mstrcplco = 3 * get_bits(gb, 2);
                    for(bnd=0; bnd<ctx->ncplbnd; bnd++) {
                        cplcoexp = get_bits(gb, 4);
                        cplcomant = get_bits(gb, 4);
                        if(cplcoexp == 15)
                            ctx->cplco[ch][bnd] = cplcomant / 16.0f;
                        else
                            ctx->cplco[ch][bnd] = (cplcomant + 16.0f) / 32.0f;
                        ctx->cplco[ch][bnd] *= scale_factors[cplcoexp+mstrcplco];
                    }
                }
            }
        }
        /* phase flags
           reference: Section 7.4.1 Channel Coupling : Overview */
        if(ctx->acmod == AC3_ACMOD_STEREO && ctx->phsflginu && (ctx->cplcoe & 3)) {
            for(bnd=0; bnd<ctx->ncplbnd; bnd++) {
                if(get_bits1(gb))
                    ctx->cplco[2][bnd] = -ctx->cplco[2][bnd];
            }
        }
    }

    /* rematrixing
       reference: Section 7.5.2 Rematrixing : Frequency Band Definitions */
    if(ctx->acmod == AC3_ACMOD_STEREO) {
        ctx->rematstr = get_bits1(gb);
        if(ctx->rematstr) {
            ctx->nrematbnd = 4;
            if(ctx->cplinu && ctx->cplbegf <= 2)
                ctx->nrematbnd -= 1 + (ctx->cplbegf == 0);
            for(bnd=0; bnd<ctx->nrematbnd; bnd++)
                ctx->rematflg[bnd] = get_bits1(gb);
        } else if(blk == 0) {
            av_log(ctx->avctx, AV_LOG_ERROR, "no rematrixing strategy for block 0\n");
            return -1;
        }
    }

    /* coupling and channel exponent strategies */
    for(ch=!ctx->cplinu; ch<=ctx->nchans; ch++) {
        if(ch == ctx->lfe_channel)
            ctx->expstr[ch] = get_bits1(gb);
        else
            ctx->expstr[ch] = get_bits(gb, 2);

        if(ctx->expstr[ch] != EXP_REUSE) {
            ctx->bit_alloc_stages[ch] = 3;
        } else if(blk == 0) {
            if(ctx->expstr[ch] == EXP_REUSE) {
                av_log(ctx->avctx, AV_LOG_ERROR, "cannot reuse exponents in block 0\n");
                return -1;
            }
        }
    }

    /* channel bandwidth code
       reference: Section 7.1.3 Exponent Decoding */
    for(ch=1; ch<=ctx->nfchans; ch++) {
        int prev = ctx->endmant[ch];
        ctx->strtmant[ch] = 0;
        if(ctx->chincpl[ch]) {
            ctx->endmant[ch] = ctx->strtmant[CPL_CH];
        } else if(ctx->expstr[ch] != EXP_REUSE) {
            int chbwcod = get_bits(gb, 6);
            if(chbwcod > 60) {
                av_log(ctx->avctx, AV_LOG_ERROR, "chbwcod=%d > 60\n", chbwcod);
                return -1;
            }
            ctx->endmant[ch] = chbwcod * 3 + 73;
        }
        if(ctx->expstr[ch] == EXP_REUSE && ctx->endmant[ch] != prev) {
            av_log(ctx->avctx, AV_LOG_ERROR, "cannot reuse exponents in first coupled block\n");
            return -1;
        }
        ctx->nmant[ch] = ctx->endmant[ch];
    }
    if(ctx->lfeon) {
        ctx->strtmant[ctx->lfe_channel] = 0;
        ctx->endmant[ctx->lfe_channel]  = 7;
        ctx->nmant[ctx->lfe_channel]    = 7;
    }

    /* exponents
       reference: Section 7.1.3 Exponent Decoding */
    for(ch=!ctx->cplinu; ch<=ctx->nchans; ch++) {
        if(ctx->expstr[ch] != EXP_REUSE) {
            int ngrps = nexpgrp_tbl[!ch][ctx->expstr[ch]-1][ctx->nmant[ch]];
            ctx->dexps[ch][0] = get_bits(gb, 4) << !ch;
            decode_exponents(gb, ctx->expstr[ch], ngrps, ctx->dexps[ch][0],
                             &ctx->dexps[ch][ctx->strtmant[ch]+!!ch]);
            if(ch != CPL_CH && ch != ctx->lfe_channel)
                skip_bits(gb, 2); // skip gainrng
        }
    }

    /* bit allocation information
       reference: Section 7.2.2 Parametric Bit Allocation */
    if(get_bits1(gb)) {
        ctx->bit_alloc_params.sdecay = ff_sdecaytab[get_bits(gb, 2)] >> ctx->bit_alloc_params.halfratecod;
        ctx->bit_alloc_params.fdecay = ff_fdecaytab[get_bits(gb, 2)] >> ctx->bit_alloc_params.halfratecod;
        ctx->bit_alloc_params.sgain = ff_sgaintab[get_bits(gb, 2)];
        ctx->bit_alloc_params.dbknee = ff_dbkneetab[get_bits(gb, 2)];
        ctx->bit_alloc_params.floor = ff_floortab[get_bits(gb, 3)];
        for(ch=0; ch<AC3_MAX_CHANNELS; ch++) {
            ctx->bit_alloc_stages[ch] = FFMAX(ctx->bit_alloc_stages[ch], 2);
        }
    } else if(blk == 0) {
        av_log(ctx->avctx, AV_LOG_ERROR, "no bit allocation info for block 0\n");
        return -1;
    }

    /* snroffset and fast gain codes
       reference: Section 7.2.2 Parametric Bit Allocation */
    if(get_bits1(gb)) {
        int csnr = (get_bits(gb, 6) - 15) << 4;
        for(ch=!ctx->cplinu; ch<=ctx->nchans; ch++) {
            ctx->snroffst[ch] = (csnr + get_bits(gb, 4)) << 2;
            ctx->fgain[ch]  = ff_fgaintab[get_bits(gb, 3)];
        }
        memset(ctx->bit_alloc_stages, 3, sizeof(ctx->bit_alloc_stages));
    } else if(blk == 0) {
        av_log(ctx->avctx, AV_LOG_ERROR, "no snroffset for block 0\n");
        return -1;
    }

    /* coupling leak information */
    if(ctx->cplinu && get_bits1(gb)) {
        ctx->bit_alloc_params.cplfleak = get_bits(gb, 3);
        ctx->bit_alloc_params.cplsleak = get_bits(gb, 3);
        ctx->bit_alloc_stages[CPL_CH] = FFMAX(ctx->bit_alloc_stages[CPL_CH], 2);
    }

    /* delta bit allocation information */
    if(get_bits1(gb)) {
        /* bit allocation exists (new/reuse/none) */
        for(ch=!ctx->cplinu; ch<=ctx->nfchans; ch++) {
            ctx->deltbae[ch] = get_bits(gb, 2);
        }

        /* delta offset, len and bit allocation */
        for(ch=!ctx->cplinu; ch<=ctx->nfchans; ch++) {
            if(ctx->deltbae[ch] == DBA_NEW) {
                ctx->deltnseg[ch] = get_bits(gb, 3);
                for(seg=0; seg<=ctx->deltnseg[ch]; seg++) {
                    ctx->deltoffst[ch][seg] = get_bits(gb, 5);
                    ctx->deltlen[ch][seg] = get_bits(gb, 4);
                    ctx->deltba[ch][seg] = get_bits(gb, 3);
                }
            }
            ctx->bit_alloc_stages[ch] = FFMAX(ctx->bit_alloc_stages[ch], 2);
        }
    } else if(blk == 0) {
        for(ch=!ctx->cplinu; ch<=ctx->nfchans; ch++) {
            ctx->deltbae[ch] = DBA_NONE;
        }
    }

    /* run bit allocation
       reference: Section 7.2.2 Parametric Bit Allocation
       The spec separates the process into 6 stages, but we group them
       together into only 3 stages. */
    for(ch=!ctx->cplinu; ch<=ctx->nchans; ch++) {
        if(ctx->bit_alloc_stages[ch] > 2) {
            /* Exponent mapping into PSD and PSD integration */
            ff_ac3_bit_alloc_calc_psd((int8_t *)ctx->dexps[ch],
                                      ctx->strtmant[ch], ctx->endmant[ch],
                                      ctx->psd[ch], ctx->bndpsd[ch]);
        }
        if(ctx->bit_alloc_stages[ch] > 1) {
            /* Compute excitation function, Compute masking curve, and
               Apply delta bit allocation */
            ff_ac3_bit_alloc_calc_mask(&ctx->bit_alloc_params, ctx->bndpsd[ch],
                                       ctx->strtmant[ch], ctx->endmant[ch],
                                       ctx->fgain[ch], (ch == ctx->lfe_channel),
                                       ctx->deltbae[ch], ctx->deltnseg[ch],
                                       ctx->deltoffst[ch], ctx->deltlen[ch],
                                       ctx->deltba[ch], ctx->mask[ch]);
        }
        if(ctx->bit_alloc_stages[ch] > 0) {
            /* Compute bit allocation */
            ff_ac3_bit_alloc_calc_bap(ctx->mask[ch], ctx->psd[ch],
                                      ctx->strtmant[ch], ctx->endmant[ch],
                                      ctx->snroffst[ch],
                                      ctx->bit_alloc_params.floor,
                                      ctx->bap[ch]);
        }
    }

    /* unused dummy data */
    if(get_bits1(gb)) {
        int skipl = get_bits(gb, 9);
        while(skipl--)
            skip_bits(gb, 8);
    }

    /* read mantissas and generate the transform coefficients.
       this also uncouples channels if coupling is in use. */
    get_transform_coeffs(ctx);

    /* recover left & right coefficients if rematrixing is in use */
    if(ctx->acmod == AC3_ACMOD_STEREO)
        do_rematrixing(ctx);

    /* apply scaling to coefficients (headroom, bias, dialnorm, dynrng) */
    for(ch=1; ch<=ctx->nchans; ch++) {
        float gain = 2.0f * ctx->mul_bias;
        if(ctx->acmod == AC3_ACMOD_DUALMONO) {
            gain *= ctx->dialnorm[ch-1] * ctx->dynrng[ch-1];
        } else {
            gain *= ctx->dialnorm[0] * ctx->dynrng[0];
        }
        for(i=0; i<ctx->endmant[ch]; i++) {
            ctx->transform_coeffs[ch][i] *= gain;
        }
    }

    do_imdct(ctx);

    /* downmix output if needed */
    if(ctx->nchans != ctx->out_channels) {
        ac3_downmix(ctx->output, ctx->nfchans, ctx->output_mode,
                    ctx->downmix_coeffs);
    }

    /* convert float to 16-bit integer */
    for(ch=0; ch<ctx->out_channels; ch++) {
        for(i=0; i<256; i++) {
            ctx->output[ch][i] += ctx->add_bias;
        }
        ctx->dsp.float_to_int16(ctx->int_output[ch], ctx->output[ch], 256);
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

    /* parse the AC-3 frame header */
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

    /* channel config */
    ctx->out_channels = ctx->nchans;
    if(avctx->channels == 0) {
        avctx->channels = ctx->out_channels;
    } else if(ctx->out_channels < avctx->channels) {
        av_log(avctx, AV_LOG_WARNING, "AC3 source channels are less than "
               "specified: output to %d channels.\n", ctx->out_channels);
        avctx->channels = ctx->out_channels;
    }
    if(avctx->channels == 2) {
        ctx->output_mode = AC3_ACMOD_STEREO;
    } else if(avctx->channels == 1) {
        ctx->output_mode = AC3_ACMOD_MONO;
    } else if(avctx->channels != ctx->out_channels) {
        av_log(avctx, AV_LOG_ERROR, "Cannot downmix AC3 to %d channels.\n",
               avctx->channels);
        return -1;
    }
    ctx->out_channels = avctx->channels;

    /* parse the audio blocks */
    for(i=0; i<NB_BLOCKS; i++) {
        if(ac3_parse_audio_block(ctx, i)) {
            av_log(avctx, AV_LOG_ERROR, "error parsing the audio block\n");
            *data_size = 0;
            return ctx->frame_size;
        }
        /* interleave output */
        for(k=0; k<256; k++) {
            for(j=0; j<ctx->out_channels; j++) {
                *(out_samples++) = ctx->int_output[j][k];
            }
        }
    }
    *data_size = AC3_FRAME_SIZE * ctx->out_channels * sizeof(int16_t);
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
    .priv_data_size = sizeof(AC3DecodeContext),
    .init = ac3_decode_init,
    .close = ac3_decode_end,
    .decode = ac3_decode_frame,
};
