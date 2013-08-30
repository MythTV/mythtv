/*
 * yuv2rgb.h
 * Copyright (C) 2000-2001 Michel Lespinasse <walken@zoy.org>
 * Copyright (C) 1999-2000 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * This file is part of mpeg2dec, a free MPEG-2 video stream decoder.
 * See http://libmpeg2.sourceforge.net/ for updates.
 *
 * mpeg2dec is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpeg2dec is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef MYTH_YUV2RGB_H
#define MYTH_YUV2RGB_H

#include <inttypes.h>

#define MODE_RGB  0x1
#define MODE_BGR  0x2

typedef void (* yuv2rgb_fun) (uint8_t * image, uint8_t * py,
                              uint8_t * pu, uint8_t * pv,
                              int h_size, int v_size,
                              int rgb_stride, int y_stride, int uv_stride,
                              int alphaones);

extern yuv2rgb_fun yuv2rgb;

void yuv2rgb_init (int bpp, int mode);
yuv2rgb_fun yuv2rgb_init_mmxext (int bpp, int mode);
yuv2rgb_fun yuv2rgb_init_mmx (int bpp, int mode);
//yuv2rgb_fun yuv2rgb_init_mlib (int bpp, int mode);

// actually does to i420
void rgb32_to_yuv420p(unsigned char *lum, unsigned char *cb, unsigned char *cr,
                      unsigned char *alpha, unsigned char *src, 
                      int width, int height, int srcwidth);


// These are used to help speed up playback by QuickTime on OS X

typedef void (*conv_i420_2vuy_fun) (
    uint8_t *image, int vuy_stride,
    const uint8_t *py, const uint8_t *pu, const uint8_t *pv,
    int y_stride, int u_stride, int v_stride,
    int h_size, int v_size);


conv_i420_2vuy_fun get_i420_2vuy_conv(void);

typedef void (*conv_2vuy_i420_fun) (
    uint8_t *py, uint8_t *pu, uint8_t *pv,
    int y_stride, int u_stride, int v_stride,
    const uint8_t *image, int vuy_stride,
    int h_size, int v_size);

conv_2vuy_i420_fun get_2vuy_i420_conv(void);

#endif /* MYTH_YUV2RGB_H */
