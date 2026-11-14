/*
 * Copyright (c) 2005 Michael Niedermayer <michaelni@gmx.at>
 * Copyright (c) 2014 Arwa Arif <arwaarif1994@gmail.com>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with FFmpeg; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef AVFILTER_PP7DSP_H
#define AVFILTER_PP7DSP_H

#include <stdint.h>

#include "config.h"

typedef struct PP7DSPContext {
    void (*dctB)(int16_t *restrict dst, const int16_t *restrict src);
} PP7DSPContext;

void ff_pp7dsp_init_x86(PP7DSPContext *pp7dsp);

static void dctB_c(int16_t *restrict dst, const int16_t *restrict src)
{
    for (int i = 0; i < 4; i++) {
        int s0 = src[0 * 4] + src[6 * 4];
        int s1 = src[1 * 4] + src[5 * 4];
        int s2 = src[2 * 4] + src[4 * 4];
        int s3 = src[3 * 4];
        int s = s3 + s3;
        s3 = s  - s0;
        s0 = s  + s0;
        s  = s2 + s1;
        s2 = s2 - s1;
        dst[0 * 4] = s0 + s;
        dst[2 * 4] = s0 - s;
        dst[1 * 4] = 2 * s3 +     s2;
        dst[3 * 4] =     s3 - 2 * s2;
        src++;
        dst++;
    }
}

static inline void ff_pp7dsp_init(PP7DSPContext *pp7dsp)
{
    pp7dsp->dctB = dctB_c;

#if ARCH_X86 && HAVE_X86ASM
    ff_pp7dsp_init_x86(pp7dsp);
#endif
}

#endif /* AVFILTER_PP7DSP_H */
