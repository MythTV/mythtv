/**
 * AC3 mixing functions
 * copyright (c) 2007 Justin Ruggles.
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
 * @file ac3mix.h
 * AC3 mixing functions
 */

#include "ac3.h"

#define scale_dualmono_to_mono scale_stereo_to_mono

#define mix_dualmono_to_mono mix_stereo_to_mono

#define scale_dualmono_to_stereo scale_stereo_to_mono

static inline void mix_dualmono_to_stereo(float output[AC3_MAX_CHANNELS][AC3_BLOCK_SIZE])
{
    int i;
    for(i=0; i<AC3_BLOCK_SIZE; i++) {
        output[0][i] += output[1][i];
        output[1][i] = output[0][i];
    }
}

static inline void scale_mono_to_stereo(float mix_factors[AC3_MAX_CHANNELS])
{
    mix_factors[0] = LEVEL_MINUS_3DB;
}

static inline void mix_mono_to_stereo(float output[AC3_MAX_CHANNELS][AC3_BLOCK_SIZE])
{
    int i;
    for(i=0; i<AC3_BLOCK_SIZE; i++) {
        output[1][i] = output[0][i];
    }
}

static inline void scale_stereo_to_mono(float mix_factors[AC3_MAX_CHANNELS])
{
    mix_factors[0] = mix_factors[1] = 0.5f;
}

static inline void mix_stereo_to_mono(float output[AC3_MAX_CHANNELS][AC3_BLOCK_SIZE])
{
    int i;
    for(i=0; i<AC3_BLOCK_SIZE; i++) {
        output[0][i] += output[1][i];
    }
}

static inline void scale_3f_to_mono(float mix_factors[AC3_MAX_CHANNELS],
                                    float cmix)
{
    float nf = 0.5f / (LEVEL_MINUS_3DB + cmix * LEVEL_MINUS_3DB);
    mix_factors[0] = mix_factors[2] = nf * LEVEL_MINUS_3DB;
    mix_factors[1] = nf * cmix * LEVEL_PLUS_3DB;
}

static inline void mix_3f_to_mono(float output[AC3_MAX_CHANNELS][AC3_BLOCK_SIZE])
{
    int i;
    for(i=0; i<AC3_BLOCK_SIZE; i++) {
        output[0][i] += output[1][i] + output[2][i];
    }
}

static inline void scale_3f_to_stereo(float mix_factors[AC3_MAX_CHANNELS],
                                      float cmix)
{
    float nf = 1.0f / (1.0f + cmix);
    mix_factors[0] = mix_factors[2] = nf;
    mix_factors[1] = nf * cmix;
}

static inline void mix_3f_to_stereo(float output[AC3_MAX_CHANNELS][AC3_BLOCK_SIZE])
{
    int i;
    for(i=0; i<AC3_BLOCK_SIZE; i++) {
        output[0][i] += output[1][i];
        output[1][i] += output[2][i];
    }
}

static inline void scale_2f_1r_to_mono(float mix_factors[AC3_MAX_CHANNELS],
                                       float smix)
{
    float nf = 0.5f / (1.0f + smix * LEVEL_MINUS_3DB);
    mix_factors[0] = mix_factors[1] = nf;
    mix_factors[2] = nf * smix * LEVEL_PLUS_3DB;
}

#define mix_2f_1r_to_mono mix_3f_to_mono

static inline void scale_2f_1r_to_stereo(float mix_factors[AC3_MAX_CHANNELS],
                                         float smix)
{
    float nf = 1.0f / (1.0f + smix);
    mix_factors[0] = mix_factors[1] = nf;
    mix_factors[2] = nf * smix;
}

static inline void mix_2f_1r_to_stereo(float output[AC3_MAX_CHANNELS][AC3_BLOCK_SIZE])
{
    int i;
    for(i=0; i<AC3_BLOCK_SIZE; i++) {
        output[0][i] += output[2][i];
        output[1][i] += output[2][i];
    }
}

static inline void scale_3f_1r_to_mono(float mix_factors[AC3_MAX_CHANNELS],
                                       float cmix, float smix)
{
    float nf = 0.5f / (1.0f + cmix + LEVEL_MINUS_3DB * smix);
    mix_factors[0] = mix_factors[2] = nf;
    mix_factors[1] = 2.0f * nf * cmix;
    mix_factors[3] = nf * smix * LEVEL_PLUS_3DB;
}

static inline void mix_3f_1r_to_mono(float output[AC3_MAX_CHANNELS][AC3_BLOCK_SIZE])
{
    int i;
    for(i=0; i<AC3_BLOCK_SIZE; i++) {
        output[0][i] += output[1][i] + output[2][i] + output[3][i];
    }
}

static inline void scale_3f_1r_to_stereo(float mix_factors[AC3_MAX_CHANNELS],
                                         float cmix, float smix)
{
    float nf = 1.0f / (1.0f + cmix + LEVEL_MINUS_3DB * smix);
    mix_factors[0] = mix_factors[2] = nf;
    mix_factors[1] = nf * cmix;
    mix_factors[3] = nf * smix * LEVEL_MINUS_3DB;
}

static inline void mix_3f_1r_to_stereo(float output[AC3_MAX_CHANNELS][AC3_BLOCK_SIZE])
{
    int i;
    for(i=0; i<AC3_BLOCK_SIZE; i++) {
        output[0][i] += output[1][i] + output[3][i];
        output[1][i] += output[2][i] + output[3][i];
    }
}

static inline void scale_2f_2r_to_mono(float mix_factors[AC3_MAX_CHANNELS],
                                       float smix)
{
    float nf = 0.5f / (1.0f + smix);
    mix_factors[0] = mix_factors[1] = nf;
    mix_factors[2] = mix_factors[3] = nf * smix;
}

#define mix_2f_2r_to_mono mix_3f_1r_to_mono

static inline void scale_2f_2r_to_stereo(float mix_factors[AC3_MAX_CHANNELS],
                                         float smix)
{
    float nf = 1.0f / (1.0f + smix);
    mix_factors[0] = mix_factors[1] = nf;
    mix_factors[2] = mix_factors[3] = nf * smix;
}

static inline void mix_2f_2r_to_stereo(float output[AC3_MAX_CHANNELS][AC3_BLOCK_SIZE])
{
    int i;
    for(i=0; i<AC3_BLOCK_SIZE; i++) {
        output[0][i] += output[2][i];
        output[1][i] += output[3][i];
    }
}

static inline void scale_3f_2r_to_mono(float mix_factors[AC3_MAX_CHANNELS],
                                       float cmix, float smix)
{
    float nf = 0.5f / (1.0f + cmix + smix);
    mix_factors[0] = mix_factors[2] = nf;
    mix_factors[1] = 2.0f * nf * cmix;
    mix_factors[3] = mix_factors[4] = nf * smix;
}

static inline void mix_3f_2r_to_mono(float output[AC3_MAX_CHANNELS][AC3_BLOCK_SIZE])
{
    int i;
    for(i=0; i<AC3_BLOCK_SIZE; i++) {
        output[0][i] += output[1][i] + output[2][i] +
                        output[3][i] + output[4][i];
    }
}

static inline void scale_3f_2r_to_stereo(float mix_factors[AC3_MAX_CHANNELS],
                                         float cmix, float smix)
{
    float nf = 1.0f / (1.0f + cmix + smix);
    mix_factors[0] = mix_factors[2] = nf;
    mix_factors[1] = nf * cmix;
    mix_factors[3] = mix_factors[4] = nf * smix;
}

static inline void mix_3f_2r_to_stereo(float output[AC3_MAX_CHANNELS][AC3_BLOCK_SIZE])
{
    int i;
    for(i=0; i<AC3_BLOCK_SIZE; i++) {
        output[0][i] += output[1][i] + output[3][i];
        output[1][i] += output[2][i] + output[4][i];
    }
}
