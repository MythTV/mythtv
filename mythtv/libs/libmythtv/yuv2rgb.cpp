/*
 * yuv2rgb_mmx.c
 * Copyright (C) 2000-2001 Silicon Integrated System Corp.
 * All Rights Reserved.
 *
 * Author: Olie Lho <ollie@sis.com.tw>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <cstdio>
#include <cstdlib>
#include <inttypes.h>
#include "config.h"

#ifdef MMX
#include "libavcodec/i386/mmx.h"
#define CPU_MMXEXT 0
#define CPU_MMX 1
#endif

#ifdef HAVE_ALTIVEC
extern "C" int has_altivec(void);    // in libavcodec/ppc/dsputil_altivec.c
#ifdef HAVE_ALTIVEC_H
#include <altivec.h>
#else
#include <Accelerate/Accelerate.h>
#endif
#endif
#include "yuv2rgb.h"

/** \file yuv2rgb.cpp
 *  \brief Contains various YUV, VUY and RGBA colorspace conversion rutines.
 *
 */

static void yuv420_argb32_non_mmx(unsigned char *image, unsigned char *py,
                           unsigned char *pu, unsigned char *pv,
                           int h_size, int v_size, int rgb_stride,
                           int y_stride, int uv_stride, int alphaones);

/* CPU_MMXEXT/CPU_MMX adaptation layer */

#define movntq(src,dest)        \
do {                            \
    if (cpu == CPU_MMXEXT)      \
        movntq_r2m (src, dest); \
    else                        \
        movq_r2m (src, dest);   \
} while (0)

#ifdef MMX
static inline void mmx_yuv2rgb (uint8_t * py, uint8_t * pu, uint8_t * pv)
{
    static mmx_t mmx_80w = {0x0080008000800080LL};
    static mmx_t mmx_U_green = {0xf37df37df37df37dLL};
    static mmx_t mmx_U_blue = {0x4093409340934093LL};
    static mmx_t mmx_V_red = {0x3312331233123312LL};
    static mmx_t mmx_V_green = {0xe5fce5fce5fce5fcLL};
    static mmx_t mmx_10w = {0x1010101010101010LL};
    static mmx_t mmx_00ffw = {0x00ff00ff00ff00ffLL};
    static mmx_t mmx_Y_coeff = {0x253f253f253f253fLL};

    movd_m2r (*pu, mm0);                // mm0 = 00 00 00 00 u3 u2 u1 u0
    movd_m2r (*pv, mm1);                // mm1 = 00 00 00 00 v3 v2 v1 v0
    movq_m2r (*py, mm6);                // mm6 = Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0
    pxor_r2r (mm4, mm4);                // mm4 = 0
    /* XXX might do cache preload for image here */

    /*
     * Do the multiply part of the conversion for even and odd pixels
     * register usage:
     * mm0 -> Cblue, mm1 -> Cred, mm2 -> Cgreen even pixels
     * mm3 -> Cblue, mm4 -> Cred, mm5 -> Cgreen odd  pixels
     * mm6 -> Y even, mm7 -> Y odd
     */

    punpcklbw_r2r (mm4, mm0);           // mm0 = u3 u2 u1 u0
    punpcklbw_r2r (mm4, mm1);           // mm1 = v3 v2 v1 v0
    psubsw_m2r (mmx_80w, mm0);          // u -= 128
    psubsw_m2r (mmx_80w, mm1);          // v -= 128
    psllw_i2r (3, mm0);                 // promote precision
    psllw_i2r (3, mm1);                 // promote precision
    movq_r2r (mm0, mm2);                // mm2 = u3 u2 u1 u0
    movq_r2r (mm1, mm3);                // mm3 = v3 v2 v1 v0
    pmulhw_m2r (mmx_U_green, mm2);      // mm2 = u * u_green
    pmulhw_m2r (mmx_V_green, mm3);      // mm3 = v * v_green
    pmulhw_m2r (mmx_U_blue, mm0);       // mm0 = chroma_b
    pmulhw_m2r (mmx_V_red, mm1);        // mm1 = chroma_r
    paddsw_r2r (mm3, mm2);              // mm2 = chroma_g

    psubusb_m2r (mmx_10w, mm6);         // Y -= 16
    movq_r2r (mm6, mm7);                // mm7 = Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0
    pand_m2r (mmx_00ffw, mm6);          // mm6 =    Y6    Y4    Y2    Y0
    psrlw_i2r (8, mm7);                 // mm7 =    Y7    Y5    Y3    Y1
    psllw_i2r (3, mm6);                 // promote precision
    psllw_i2r (3, mm7);                 // promote precision
    pmulhw_m2r (mmx_Y_coeff, mm6);      // mm6 = luma_rgb even
    pmulhw_m2r (mmx_Y_coeff, mm7);      // mm7 = luma_rgb odd

    /*
     * Do the addition part of the conversion for even and odd pixels
     * register usage:
     * mm0 -> Cblue, mm1 -> Cred, mm2 -> Cgreen even pixels
     * mm3 -> Cblue, mm4 -> Cred, mm5 -> Cgreen odd  pixels
     * mm6 -> Y even, mm7 -> Y odd
     */

    movq_r2r (mm0, mm3);                // mm3 = chroma_b
    movq_r2r (mm1, mm4);                // mm4 = chroma_r
    movq_r2r (mm2, mm5);                // mm5 = chroma_g
    paddsw_r2r (mm6, mm0);              // mm0 = B6 B4 B2 B0
    paddsw_r2r (mm7, mm3);              // mm3 = B7 B5 B3 B1
    paddsw_r2r (mm6, mm1);              // mm1 = R6 R4 R2 R0
    paddsw_r2r (mm7, mm4);              // mm4 = R7 R5 R3 R1
    paddsw_r2r (mm6, mm2);              // mm2 = G6 G4 G2 G0
    paddsw_r2r (mm7, mm5);              // mm5 = G7 G5 G3 G1
    packuswb_r2r (mm0, mm0);            // saturate to 0-255
    packuswb_r2r (mm1, mm1);            // saturate to 0-255
    packuswb_r2r (mm2, mm2);            // saturate to 0-255
    packuswb_r2r (mm3, mm3);            // saturate to 0-255
    packuswb_r2r (mm4, mm4);            // saturate to 0-255
    packuswb_r2r (mm5, mm5);            // saturate to 0-255
    punpcklbw_r2r (mm3, mm0);           // mm0 = B7 B6 B5 B4 B3 B2 B1 B0
    punpcklbw_r2r (mm4, mm1);           // mm1 = R7 R6 R5 R4 R3 R2 R1 R0
    punpcklbw_r2r (mm5, mm2);           // mm2 = G7 G6 G5 G4 G3 G2 G1 G0
}

static inline void mmx_unpack_16rgb (uint8_t * image, int cpu)
{
    static mmx_t mmx_bluemask = {0xf8f8f8f8f8f8f8f8LL};
    static mmx_t mmx_greenmask = {0xfcfcfcfcfcfcfcfcLL};
    static mmx_t mmx_redmask = {0xf8f8f8f8f8f8f8f8LL};

    /*
     * convert RGB plane to RGB 16 bits
     * mm0 -> B, mm1 -> R, mm2 -> G
     * mm4 -> GB, mm5 -> AR pixel 4-7
     * mm6 -> GB, mm7 -> AR pixel 0-3
     */

    pand_m2r (mmx_bluemask, mm0);       // mm0 = b7b6b5b4b3______
    pand_m2r (mmx_greenmask, mm2);      // mm2 = g7g6g5g4g3g2____
    pand_m2r (mmx_redmask, mm1);        // mm1 = r7r6r5r4r3______
    psrlq_i2r (3, mm0);                 // mm0 = ______b7b6b5b4b3
    pxor_r2r (mm4, mm4);                // mm4 = 0
    movq_r2r (mm0, mm5);                // mm5 = ______b7b6b5b4b3
    movq_r2r (mm2, mm7);                // mm7 = g7g6g5g4g3g2____

    punpcklbw_r2r (mm4, mm2);
    punpcklbw_r2r (mm1, mm0);
    psllq_i2r (3, mm2);
    por_r2r (mm2, mm0);
    movntq (mm0, *image);

    punpckhbw_r2r (mm4, mm7);
    punpckhbw_r2r (mm1, mm5);
    psllq_i2r (3, mm7);
    por_r2r (mm7, mm5);
    movntq (mm5, *(image+8));
}

static inline void mmx_unpack_32rgb (uint8_t * image, int cpu, int alphaones)
{
    /*
     * convert RGB plane to RGB packed format,
     * mm0 -> B, mm1 -> R, mm2 -> G, mm3 -> 0,
     * mm4 -> GB, mm5 -> AR pixel 4-7,
     * mm6 -> GB, mm7 -> AR pixel 0-3
     */

    if (alphaones)
    {
        static mmx_t mmx_1s = {0xffffffffffffffffLL};
        movq_m2r (mmx_1s, mm3);
    }
    else
        pxor_r2r (mm3, mm3);

    movq_r2r (mm0, mm6);
    movq_r2r (mm1, mm7);
    movq_r2r (mm0, mm4);
    movq_r2r (mm1, mm5);
    punpcklbw_r2r (mm2, mm6);
    punpcklbw_r2r (mm3, mm7);
    punpcklwd_r2r (mm7, mm6);
    movntq (mm6, *image);
    movq_r2r (mm0, mm6);
    punpcklbw_r2r (mm2, mm6);
    punpckhwd_r2r (mm7, mm6);
    movntq (mm6, *(image+8));
    punpckhbw_r2r (mm2, mm4);
    punpckhbw_r2r (mm3, mm5);
    punpcklwd_r2r (mm5, mm4);
    movntq (mm4, *(image+16));
    movq_r2r (mm0, mm4);
    punpckhbw_r2r (mm2, mm4);
    punpckhwd_r2r (mm5, mm4);
    movntq (mm4, *(image+24));
}

static inline void yuv420_rgb16 (uint8_t * image,
                                 uint8_t * py, uint8_t * pu, uint8_t * pv,
                                 int width, int height,
                                 int rgb_stride, int y_stride, int uv_stride,
                                 int cpu, int alphaones)
{
    (void)alphaones;
    int i;

    rgb_stride -= 2 * width;
    y_stride -= width;
    uv_stride -= width >> 1;
    width >>= 3;

    do {
        i = width;
        do {
            mmx_yuv2rgb (py, pu, pv);
            mmx_unpack_16rgb (image, cpu);
            py += 8;
            pu += 4;
            pv += 4;
            image += 16;
        } while (--i);

        py += y_stride;
        image += rgb_stride;
        if (height & 1) {
            pu += uv_stride;
            pv += uv_stride;
        } else {
            pu -= 4 * width;
            pv -= 4 * width;
        }
    } while (--height);

        emms();
}

static inline void yuv420_argb32 (uint8_t * image, uint8_t * py,
                                  uint8_t * pu, uint8_t * pv,
                                  int width, int height,
                                  int rgb_stride, int y_stride, int uv_stride,
                                  int cpu, int alphaones)
{
    int i;

    rgb_stride -= 4 * width;
    y_stride -= width;
    uv_stride -= width >> 1;
    width >>= 3;

    do {
        i = width;
        do {
            mmx_yuv2rgb (py, pu, pv);
            mmx_unpack_32rgb (image, cpu, alphaones);
            py += 8;
            pu += 4;
            pv += 4;
            image += 32;
        } while (--i);

        py += y_stride;
        image += rgb_stride;
        if (height & 1) {
            pu += uv_stride;
            pv += uv_stride;
        } else {
            pu -= 4 * width;
            pv -= 4 * width;
        }
    } while (--height);

        emms();
}

static void mmxext_rgb16 (uint8_t * image,
                          uint8_t * py, uint8_t * pu, uint8_t * pv,
                          int width, int height,
                          int rgb_stride, int y_stride, int uv_stride,
                          int alphaones)
{
    yuv420_rgb16 (image, py, pu, pv, width, height,
                  rgb_stride, y_stride, uv_stride, CPU_MMXEXT, alphaones);
}

static void mmxext_argb32 (uint8_t * image,
                           uint8_t * py, uint8_t * pu, uint8_t * pv,
                           int width, int height,
                           int rgb_stride, int y_stride, int uv_stride, 
                           int alphaones)
{
    yuv420_argb32 (image, py, pu, pv, width, height,
                   rgb_stride, y_stride, uv_stride, CPU_MMXEXT, alphaones);
}

static void mmx_rgb16 (uint8_t * image,
                       uint8_t * py, uint8_t * pu, uint8_t * pv,
                       int width, int height,
                       int rgb_stride, int y_stride, int uv_stride, 
                       int alphaones)
{
    yuv420_rgb16 (image, py, pu, pv, width, height,
                  rgb_stride, y_stride, uv_stride, CPU_MMX, alphaones);
}

static void mmx_argb32 (uint8_t * image,
                        uint8_t * py, uint8_t * pu, uint8_t * pv,
                        int width, int height,
                        int rgb_stride, int y_stride, int uv_stride, 
                        int alphaones)
{
    yuv420_argb32 (image, py, pu, pv, width, height,
                   rgb_stride, y_stride, uv_stride, CPU_MMX, alphaones);
}
#endif

/** \fn yuv2rgb_init_mmxext(int bpp, int mode)
 *  \brief This returns a yuv to rgba converter, using
 *          mmxext if MMX was compiled in.
 *
 *  \param mode must be MODE_RGB
 *  \param bpp must be 32
 *
 *  \return function pointer or NULL if converter could not be found.
 */
yuv2rgb_fun yuv2rgb_init_mmxext (int bpp, int mode)
{
#ifdef MMX
    if ((bpp == 16) && (mode == MODE_RGB))
        return mmxext_rgb16;
    else if ((bpp == 32) && (mode == MODE_RGB))
        return mmxext_argb32;
#else
    (void)bpp;
    (void)mode;
#endif

    return NULL; /* Fallback to C */
}

/** \fn yuv2rgb_init_mmx (int bpp, int mode)
 *  \brief This returns a yuv to rgba converter, using
 *         mmx if MMX was compiled in.
 *
 *  \param mode must be MODE_RGB
 *  \param bpp must be 32
 *
 *  \return function pointer or NULL if converter could not be found.
 */
yuv2rgb_fun yuv2rgb_init_mmx (int bpp, int mode)
{
#ifdef MMX
    if ((bpp == 16) && (mode == MODE_RGB))
        return mmx_rgb16;
    else if ((bpp == 32) && (mode == MODE_RGB))
        return mmx_argb32;
#endif

    if ((bpp == 32) && (mode == MODE_RGB))
        return yuv420_argb32_non_mmx;

    return NULL;
}

#define SCALE_BITS 10

#define C_Y  (76309 >> (16 - SCALE_BITS))
#define C_RV (117504 >> (16 - SCALE_BITS))
#define C_BU (138453 >> (16 - SCALE_BITS))
#define C_GU (13954 >> (16 - SCALE_BITS))
#define C_GV (34903 >> (16 - SCALE_BITS))

#define RGBOUT(r, g, b, y1)\
{\
    y = (y1 - 16) * C_Y;\
    r = (y + r_add) >> SCALE_BITS;\
    g = (y + g_add) >> SCALE_BITS;\
    b = (y + b_add) >> SCALE_BITS;\
}

static void yuv420_argb32_non_mmx(unsigned char *image, unsigned char *py,
                           unsigned char *pu, unsigned char *pv,
                           int h_size, int v_size, int rgb_stride,
                           int y_stride, int uv_stride, int alphaones)
{
    unsigned char *y1_ptr, *y2_ptr, *cb_ptr, *cr_ptr, *d, *d1, *d2;
    int w, y, cb, cr, r_add, g_add, b_add, width2;
    int dstwidth;

// byte indices
#ifdef WORDS_BIGENDIAN
#define R_OI  1
#define G_OI  2
#define B_OI  3
#define A_OI  0
#else
#define R_OI  0
#define G_OI  1
#define B_OI  2
#define A_OI  3
#endif

    // squelch a warning
    rgb_stride = y_stride = uv_stride;
    
    d = image;
    y1_ptr = py;
    cb_ptr = pu;
    cr_ptr = pv;
    dstwidth = h_size * 4;
    width2 = h_size / 2;
    
    for(;v_size > 0; v_size -= 2) {
        d1 = d;
        d2 = d + h_size * 4;
        y2_ptr = y1_ptr + h_size;
        for(w = width2; w > 0; w--) {
            cb = cb_ptr[0] - 128;
            cr = cr_ptr[0] - 128;
            r_add = C_RV * cr + (1 << (SCALE_BITS - 1));
            g_add = - C_GU * cb - C_GV * cr + (1 << (SCALE_BITS - 1));
            b_add = C_BU * cb + (1 << (SCALE_BITS - 1));

            /* output 4 pixels */
            RGBOUT(d1[R_OI],   d1[G_OI],   d1[B_OI],   y1_ptr[0]);
            RGBOUT(d1[R_OI+4], d1[G_OI+4], d1[B_OI+4], y1_ptr[1]);
            RGBOUT(d2[R_OI],   d2[G_OI],   d2[B_OI],   y2_ptr[0]);
            RGBOUT(d2[R_OI+4], d2[G_OI+4], d2[B_OI+4], y2_ptr[1]);

            if (alphaones)
                d1[A_OI] = d1[A_OI+4] = d2[A_OI] = d2[A_OI+4] = 0xff;
            else
                d1[A_OI] = d1[A_OI+4] = d2[A_OI] = d2[A_OI+4] = 0;

            d1 += 8;
            d2 += 8;
            y1_ptr += 2;
            y2_ptr += 2;
            cb_ptr++;
            cr_ptr++;
        }
        d += 2 * dstwidth;
        y1_ptr += h_size;
    }
}

#define SCALEBITS 8
#define ONE_HALF  (1 << (SCALEBITS - 1)) 
#define FIX(x)          ((int) ((x) * (1L<<SCALEBITS) + 0.5))

// actually does to i420
void rgb32_to_yuv420p(unsigned char *lum, unsigned char *cb, unsigned char *cr,
                      unsigned char *alpha, unsigned char *src, 
                      int width, int height, int srcwidth)
{           
    int wrap, wrap4, x, y;
    int r, g, b, r1, g1, b1;
    unsigned char *p;
                      
// byte indices
#ifdef WORDS_BIGENDIAN
#define R_II  3
#define G_II  2
#define B_II  1
#define A_II  0
#else
#define R_II  0
#define G_II  1
#define B_II  2
#define A_II  3
#endif

    wrap = width;
    wrap4 = srcwidth * 4;
    p = src;
    for(y=0;y<height;y+=2) {
        for(x=0;x<width;x+=2) {
            r = p[R_II];
            g = p[G_II];
            b = p[B_II];
            r1 = r;
            g1 = g;
            b1 = b;
            lum[0] = (FIX(0.29900) * r + FIX(0.58700) * g +
                      FIX(0.11400) * b + ONE_HALF) >> SCALEBITS;
            alpha[0] = p[A_II];

            r = p[R_II+4];
            g = p[G_II+4];
            b = p[B_II+4];
            r1 += r;
            g1 += g;
            b1 += b;
            lum[1] = (FIX(0.29900) * r + FIX(0.58700) * g +
                      FIX(0.11400) * b + ONE_HALF) >> SCALEBITS;
            alpha[1] = p[A_II+4];

            p += wrap4;
            lum += wrap;
            alpha += wrap;

            r = p[R_II];
            g = p[G_II];
            b = p[B_II];
            r1 += r;
            g1 += g;
            b1 += b;
            lum[0] = (FIX(0.29900) * r + FIX(0.58700) * g +
                      FIX(0.11400) * b + ONE_HALF) >> SCALEBITS;
            alpha[0] = p[A_II];

            r = p[R_II+4];
            g = p[G_II+4];
            b = p[B_II+4];
            r1 += r;
            g1 += g;
            b1 += b;
            lum[1] = (FIX(0.29900) * r + FIX(0.58700) * g +
                      FIX(0.11400) * b + ONE_HALF) >> SCALEBITS;
            alpha[1] = p[A_II+4];

            cr[0] = ((- FIX(0.16874) * r1 - FIX(0.33126) * g1 +
                    FIX(0.50000) * b1 + 4 * ONE_HALF - 1) >> (SCALEBITS + 2)) +
                    128;
            cb[0] = ((FIX(0.50000) * r1 - FIX(0.41869) * g1 -
                    FIX(0.08131) * b1 + 4 * ONE_HALF - 1) >> (SCALEBITS + 2)) +
                    128;

            cb++;
            cr++;
            p += -wrap4 + 2 * 4;
            lum += -wrap + 2;
            alpha += -wrap + 2;
        }
        p += wrap4;
        lum += wrap;
        alpha += wrap;
    }
}

static void non_vec_yuv420_2vuy (uint8_t * image, uint8_t * py,
                                 uint8_t * pu, uint8_t * pv,
                                 int h_size, int v_size,
                                 int vuy_stride, int y_stride, int uv_stride)
{
    uint8_t *pi1, *pi2 = image;
    uint8_t *py1, *py2 = py;

    int x, y;

    for (y = v_size / 2; y--; )
    {
        pi1  = pi2;
        pi2 += h_size * 2;
        py1  = py2;
        py2 += h_size;

        for (x = h_size / 2; x--; )
        {
            *(pi1)++ =            *(pi2)++ = *(pu)++;
            *(pi1)++ = *(py1)++;  *(pi2)++ = *(py2)++;
            *(pi1)++ =            *(pi2)++ = *(pv)++;
            *(pi1)++ = *(py1)++;  *(pi2)++ = *(py2)++;
        }

        py1 += y_stride;
        py2 += y_stride;
        pu  += uv_stride;
        pv  += uv_stride;
        pi1 += vuy_stride;
        pi2 += vuy_stride;
    }
}

#ifdef HAVE_ALTIVEC
// Altivec code adapted from VLC's i420_yuv2.c (thanks to Titer and Paul Jara) 

#define VEC_NEXT_LINES()                                                    \
    pi1  = pi2;                                                             \
    pi2 += h_size * 2;                                                      \
    py1  = py2;                                                             \
    py2 += h_size;

#define VEC_LOAD_UV()                                                       \
    u_vec = vec_ld(0, pu); pu += 16;                                        \
    v_vec = vec_ld(0, pv); pv += 16;

#define VEC_MERGE(a)                                                        \
    uv_vec = a(u_vec, v_vec);                                               \
    y_vec = vec_ld(0, py1); py1 += 16;                                      \
    vec_st(vec_mergeh(uv_vec, y_vec), 0, pi1); pi1 += 16;                   \
    vec_st(vec_mergel(uv_vec, y_vec), 0, pi1); pi1 += 16;                   \
    y_vec = vec_ld(0, py2); py2 += 16;                                      \
    vec_st(vec_mergeh(uv_vec, y_vec), 0, pi2); pi2 += 16;                   \
    vec_st(vec_mergel(uv_vec, y_vec), 0, pi2); pi2 += 16;

static void altivec_yuv420_2vuy (uint8_t * image, uint8_t * py,
                                 uint8_t * pu, uint8_t * pv,
                                 int h_size, int v_size,
                                 int vuy_stride, int y_stride, int uv_stride)
{
    uint8_t *pi1, *pi2 = image;
    uint8_t *py1, *py2 = py;
        
    int x, y;

    vector unsigned char u_vec;
    vector unsigned char v_vec;
    vector unsigned char uv_vec;
    vector unsigned char y_vec;

    if (!((h_size % 32) | (v_size % 2)))
    {
        // Width is a multiple of 32, process 2 lines at a time
        for (y = v_size / 2; y--; )
        {
            VEC_NEXT_LINES();
            for (x = h_size / 32; x--; )
            {
                VEC_LOAD_UV();
                VEC_MERGE(vec_mergeh);
                VEC_MERGE(vec_mergel);
            }
        }
    
    }
    else if (!((h_size % 16) | (v_size % 4)))
    {
        // Width is a multiple of 16, process 4 lines at a time
        for (y = v_size / 4; y--; )
        {
            // Lines 1-2, pixels 0 to (width - 16)
            VEC_NEXT_LINES();
            for (x = h_size / 32; x--; )
            {
                VEC_LOAD_UV();
                VEC_MERGE(vec_mergeh);
                VEC_MERGE(vec_mergel);
            }
            
            // Lines 1-2, pixels (width - 16) to width
            VEC_LOAD_UV();
            VEC_MERGE(vec_mergeh);
            
            // Lines 3-4, pixels 0-16
            VEC_NEXT_LINES();
            VEC_MERGE(vec_mergel);
            
            // Lines 3-4, pixels 16 to width
            for (x = h_size / 32; x--; )
            {
                VEC_LOAD_UV();
                VEC_MERGE(vec_mergeh);
                VEC_MERGE(vec_mergel);
            }
        }
    }
    else
    {
        // Fall back to C version
        non_vec_yuv420_2vuy(image, py, pu, pv, h_size, v_size,
                            vuy_stride, y_stride, uv_stride);
    }
}

yuv2vuy_fun yuv2vuy_init_altivec (void)
{
    if (has_altivec())
        return altivec_yuv420_2vuy;
    else
        return non_vec_yuv420_2vuy; /* Fallback to C */
}
#endif // HAVE_ALTIVEC

static void non_vec_2vuy_yuv420 (uint8_t * image, uint8_t * py,
                                 uint8_t * pu, uint8_t * pv,
                                 int h_size, int v_size,
                                 int vuy_stride, int y_stride, int uv_stride)
{
    uint8_t *pi1, *pi2 = image;
    uint8_t *py1, *py2 = py;

    int x, y;

    for (y = v_size / 2; y--; )
    {
        pi1  = pi2;
        pi2 += h_size * 2;
        py1  = py2;
        py2 += h_size;

        for (x = h_size / 2; x--; )
        {
            *(pu)++  = (*(pi1)++ +            *(pi2)++) / 2;
            *(py1)++ =  *(pi1)++;  *(py2)++ = *(pi2)++;
            *(pv)++  = (*(pi1)++ +            *(pi2)++) / 2;
            *(py1)++ =  *(pi1)++;  *(py2)++ = *(pi2)++;
        }

        py1 += y_stride;
        py2 += y_stride;
        pu  += uv_stride;
        pv  += uv_stride;
        pi1 += vuy_stride;
        pi2 += vuy_stride;
    }
}

#ifdef HAVE_ALTIVEC
// Altivec code adapted from VLC's i420_yuv2.c (thanks to Titer and Paul Jara) 

#define VEC_READ_LINE(ptr, y, uv)                                           \
    pa_vec = vec_ld(0, ptr); ptr += 16;                                     \
    pb_vec = vec_ld(0, ptr); ptr += 16;                                     \
    vec_st(vec_pack((vector unsigned short)pa_vec,                          \
                    (vector unsigned short)pb_vec),                         \
           0, y); y += 16;                                                  \
    uv = vec_pack(vec_sr((vector unsigned short)pa_vec, eight_vec),         \
                  vec_sr((vector unsigned short)pb_vec, eight_vec));

#define VEC_SPLIT(a)                                                        \
    VEC_READ_LINE(pi1, py1, uv1_vec);                                       \
    VEC_READ_LINE(pi2, py2, uv2_vec);                                       \
    a = vec_avg(uv1_vec, uv2_vec);

#define VEC_STORE_UV()                                                      \
    vec_st(vec_pack((vector unsigned short)uva_vec,                         \
                    (vector unsigned short)uvb_vec),                        \
           0, pv); pv += 16;                                                \
    vec_st(vec_pack(vec_sr((vector unsigned short)uva_vec, eight_vec),      \
                    vec_sr((vector unsigned short)uvb_vec, eight_vec)),     \
           0, pu); pu += 16;

    
static void altivec_2vuy_yuv420 (uint8_t * image, uint8_t * py,
                                 uint8_t * pu, uint8_t * pv,
                                 int h_size, int v_size,
                                 int vuy_stride, int y_stride, int uv_stride)
{
    uint8_t *pi1, *pi2 = image;
    uint8_t *py1, *py2 = py;
        
    int x, y;
    
    vector unsigned short eight_vec = vec_splat_u16(8);
    vector unsigned char pa_vec, pb_vec,
                         uv1_vec, uv2_vec,
                         uva_vec, uvb_vec;

    if (!((h_size % 32) | (v_size % 2)))
    {
        // Width is a multiple of 32, process 2 lines at a time
        for (y = v_size / 2; y--; )
        {
            VEC_NEXT_LINES();
            for (x = h_size / 32; x--; )
            {
                VEC_SPLIT(uva_vec);
                VEC_SPLIT(uvb_vec);
                VEC_STORE_UV();
            }
        }
    
    }
    else if (!((h_size % 16) | (v_size % 4)))
    {
        // Width is a multiple of 16, process 4 lines at a time
        for (y = v_size / 4; y--; )
        {
            // Lines 1-2, pixels 0 to (width - 16)
            VEC_NEXT_LINES();
            for (x = h_size / 32; x--; )
            {
                VEC_SPLIT(uva_vec);
                VEC_SPLIT(uvb_vec);
                VEC_STORE_UV();
            }
            
            // Lines 1-2, pixels (width - 16) to width
            VEC_SPLIT(uva_vec);
            
            // Lines 3-4, pixels 0-16
            VEC_NEXT_LINES();
            VEC_SPLIT(uvb_vec);
            VEC_STORE_UV();
            
            // Lines 3-4, pixels 16 to width
            for (x = h_size / 32; x--; )
            {
                VEC_SPLIT(uva_vec);
                VEC_SPLIT(uvb_vec);
                VEC_STORE_UV();
            }
        }
    }
    else
    {
        // Fall back to C version
        non_vec_2vuy_yuv420(image, py, pu, pv, h_size, v_size,
                            vuy_stride, y_stride, uv_stride);
    }
}

/** \fn vuy2yuv_init_altivec (void)
 *  \brief This returns a vuy to yuv converter, using
 *         Altivec if it was compiled in.
 *
 *  \return function pointer or NULL if converter could not be found.
 */
vuy2yuv_fun vuy2yuv_init_altivec (void)
{
    if (has_altivec())
        return altivec_2vuy_yuv420;
    else
        return non_vec_2vuy_yuv420; /* Fallback to C */
}
#endif // HAVE_ALTIVEC
