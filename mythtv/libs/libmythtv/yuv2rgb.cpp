
/*
 * yuv2rgb_mmx.c, Software YUV to RGB coverter with Intel MMX "technology"
 *
 * Copyright (C) 2000, Silicon Integrated System Corp.
 * All Rights Reserved.
 *
 * Author: Olie Lho <ollie@sis.com.tw>
 *
 * This file is part of mpeg2dec, a free MPEG-2 video decoder
 *
 * mpeg2dec is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * mpeg2dec is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Make; see the file COPYING. If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <stdio.h>
#include <stdlib.h>

#include "mmx.h"
#include <inttypes.h>

#include "yuv2rgb.h"

/* hope these constant values are cache line aligned */
uint64_t mmx_80w = 0x0080008000800080LL;
uint64_t mmx_10w = 0x1010101010101010LL;
uint64_t mmx_00ffw = 0x00ff00ff00ff00ffLL;
uint64_t mmx_Y_coeff = 0x253f253f253f253fLL;

/* hope these constant values are cache line aligned */
uint64_t mmx_U_green = 0xf37df37df37df37dLL;
uint64_t mmx_U_blue = 0x4093409340934093LL;
uint64_t mmx_V_red = 0x3312331233123312LL;
uint64_t mmx_V_green = 0xe5fce5fce5fce5fcLL;

/* hope these constant values are cache line aligned */
uint64_t mmx_redmask = 0xf8f8f8f8f8f8f8f8LL;
uint64_t mmx_grnmask = 0xfcfcfcfcfcfcfcfcLL;
uint64_t mmx_grnshift = 0x03LL;
uint64_t mmx_blueshift = 0x03LL;

#if defined (HAVE_SSE) || defined (HAVE_3DNOW)
#define movntq "movntq" // use this for processors that have SSE or 3Dnow
#else
#define movntq "movq" // for MMX-only processors
#endif

static void yuv420_argb32_mmx (uint8_t * image, uint8_t * py,
			       uint8_t * pu, uint8_t * pv,
			       int h_size, int v_size)
{
    int even = 1;
    int x = 0, y = 0;

    __asm__ (
	     ".align 8 \n\t"
	     "movd (%1), %%mm0 # Load 4 Cb 00 00 00 00 u3 u2 u1 u0\n\t"
	     //"movl $0, (%3) # cache preload for image\n\t"

	     "movd (%2), %%mm1 # Load 4 Cr 00 00 00 00 v3 v2 v1 v0\n\t"
	     "pxor %%mm4, %%mm4 # zero mm4\n\t"

	     "movq (%0), %%mm6 # Load 8 Y Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0\n\t"
	     : : "r" (py), "r" (pu), "r" (pv), "r" (image));

    do {
	do {
	    /* this mmx assembly code deals with SINGLE scan line at a time, it convert 8
	       pixels in each iteration */
	    __asm__ (
		     ".align 8 \n\t"
		     /* Do the multiply part of the conversion for even and odd pixels,
			register usage:
			mm0 -> Cblue, mm1 -> Cred, mm2 -> Cgreen even pixels,
			mm3 -> Cblue, mm4 -> Cred, mm5 -> Cgreen odd pixels,
			mm6 -> Y even, mm7 -> Y odd */

		     /* convert the chroma part */
		     "punpcklbw %%mm4, %%mm0 # scatter 4 Cb 00 u3 00 u2 00 u1 00 u0\n\t"
		     "punpcklbw %%mm4, %%mm1 # scatter 4 Cr 00 v3 00 v2 00 v1 00 v0\n\t"

		     "psubsw mmx_80w, %%mm0 # Cb -= 128\n\t"
		     "psubsw mmx_80w, %%mm1 # Cr -= 128\n\t"

		     "psllw $3, %%mm0 # Promote precision\n\t"
		     "psllw $3, %%mm1 # Promote precision\n\t"

		     "movq %%mm0, %%mm2 # Copy 4 Cb 00 u3 00 u2 00 u1 00 u0\n\t"
		     "movq %%mm1, %%mm3 # Copy 4 Cr 00 v3 00 v2 00 v1 00 v0\n\t"

		     "pmulhw mmx_U_green, %%mm2# Mul Cb with green coeff -> Cb green\n\t"
		     "pmulhw mmx_V_green, %%mm3# Mul Cr with green coeff -> Cr green\n\t"

		     "pmulhw mmx_U_blue, %%mm0 # Mul Cb -> Cblue 00 b3 00 b2 00 b1 00 b0\n\t"
		     "pmulhw mmx_V_red, %%mm1 # Mul Cr -> Cred 00 r3 00 r2 00 r1 00 r0\n\t"

		     "paddsw %%mm3, %%mm2 # Cb green + Cr green -> Cgreen\n\t"

		     /* convert the luma part */
		     "psubusb mmx_10w, %%mm6 # Y -= 16\n\t"

		     "movq %%mm6, %%mm7 # Copy 8 Y Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0\n\t"
		     "pand mmx_00ffw, %%mm6 # get Y even 00 Y6 00 Y4 00 Y2 00 Y0\n\t"

		     "psrlw $8, %%mm7 # get Y odd 00 Y7 00 Y5 00 Y3 00 Y1\n\t"

		     "psllw $3, %%mm6 # Promote precision\n\t"
		     "psllw $3, %%mm7 # Promote precision\n\t"

		     "pmulhw mmx_Y_coeff, %%mm6# Mul 4 Y even 00 y6 00 y4 00 y2 00 y0\n\t"
		     "pmulhw mmx_Y_coeff, %%mm7# Mul 4 Y odd 00 y7 00 y5 00 y3 00 y1\n\t"

		     /* Do the addition part of the conversion for even and odd pixels,
			register usage:
			mm0 -> Cblue, mm1 -> Cred, mm2 -> Cgreen even pixels,
			mm3 -> Cblue, mm4 -> Cred, mm5 -> Cgreen odd pixels,
			mm6 -> Y even, mm7 -> Y odd */

		     "movq %%mm0, %%mm3 # Copy Cblue\n\t"
		     "movq %%mm1, %%mm4 # Copy Cred\n\t"
		     "movq %%mm2, %%mm5 # Copy Cgreen\n\t"

		     "paddsw %%mm6, %%mm0 # Y even + Cblue 00 B6 00 B4 00 B2 00 B0\n\t"
		     "paddsw %%mm7, %%mm3 # Y odd + Cblue 00 B7 00 B5 00 B3 00 B1\n\t"

		     "paddsw %%mm6, %%mm1 # Y even + Cred 00 R6 00 R4 00 R2 00 R0\n\t"
		     "paddsw %%mm7, %%mm4 # Y odd + Cred 00 R7 00 R5 00 R3 00 R1\n\t"

		     "paddsw %%mm6, %%mm2 # Y even + Cgreen 00 G6 00 G4 00 G2 00 G0\n\t"
		     "paddsw %%mm7, %%mm5 # Y odd + Cgreen 00 G7 00 G5 00 G3 00 G1\n\t"

		     /* Limit RGB even to 0..255 */
		     "packuswb %%mm0, %%mm0 # B6 B4 B2 B0 B6 B4 B2 B0\n\t"
		     "packuswb %%mm1, %%mm1 # R6 R4 R2 R0 R6 R4 R2 R0\n\t"
		     "packuswb %%mm2, %%mm2 # G6 G4 G2 G0 G6 G4 G2 G0\n\t"

		     /* Limit RGB odd to 0..255 */
		     "packuswb %%mm3, %%mm3 # B7 B5 B3 B1 B7 B5 B3 B1\n\t"
		     "packuswb %%mm4, %%mm4 # R7 R5 R3 R1 R7 R5 R3 R1\n\t"
		     "packuswb %%mm5, %%mm5 # G7 G5 G3 G1 G7 G5 G3 G1\n\t"

		     /* Interleave RGB even and odd */
		     "punpcklbw %%mm3, %%mm0 # B7 B6 B5 B4 B3 B2 B1 B0\n\t"
		     "punpcklbw %%mm4, %%mm1 # R7 R6 R5 R4 R3 R2 R1 R0\n\t"
		     "punpcklbw %%mm5, %%mm2 # G7 G6 G5 G4 G3 G2 G1 G0\n\t"

		     /* convert RGB plane to RGB packed format, 
			mm0 -> B, mm1 -> R, mm2 -> G, mm3 -> 0,
			mm4 -> GB, mm5 -> AR pixel 4-7,
			mm6 -> GB, mm7 -> AR pixel 0-3 */
		     "pxor %%mm3, %%mm3 # zero mm3\n\t"

		     "movq %%mm0, %%mm6 # B7 B6 B5 B4 B3 B2 B1 B0\n\t"
		     "movq %%mm1, %%mm7 # R7 R6 R5 R4 R3 R2 R1 R0\n\t"

		     "movq %%mm0, %%mm4 # B7 B6 B5 B4 B3 B2 B1 B0\n\t"
		     "movq %%mm1, %%mm5 # R7 R6 R5 R4 R3 R2 R1 R0\n\t"

		     "punpcklbw %%mm2, %%mm6 # G3 B3 G2 B2 G1 B1 G0 B0\n\t"
		     "punpcklbw %%mm3, %%mm7 # 00 R3 00 R2 00 R1 00 R0\n\t"

		     "punpcklwd %%mm7, %%mm6 # 00 R1 B1 G1 00 R0 B0 G0\n\t"
		     movntq " %%mm6, (%3) # Store ARGB1 ARGB0\n\t"

		     "movq %%mm0, %%mm6 # B7 B6 B5 B4 B3 B2 B1 B0\n\t"
		     "punpcklbw %%mm2, %%mm6 # G3 B3 G2 B2 G1 B1 G0 B0\n\t"

		     "punpckhwd %%mm7, %%mm6 # 00 R3 G3 B3 00 R2 B3 G2\n\t"
		     movntq " %%mm6, 8 (%3) # Store ARGB3 ARGB2\n\t"

		     "punpckhbw %%mm2, %%mm4 # G7 B7 G6 B6 G5 B5 G4 B4\n\t"
		     "punpckhbw %%mm3, %%mm5 # 00 R7 00 R6 00 R5 00 R4\n\t"
				
		     "punpcklwd %%mm5, %%mm4 # 00 R5 B5 G5 00 R4 B4 G4\n\t"
		     movntq " %%mm4, 16 (%3) # Store ARGB5 ARGB4\n\t"

		     "movq %%mm0, %%mm4 # B7 B6 B5 B4 B3 B2 B1 B0\n\t"
		     "punpckhbw %%mm2, %%mm4 # G7 B7 G6 B6 G5 B5 G4 B4\n\t"

		     "punpckhwd %%mm5, %%mm4 # 00 R7 G7 B7 00 R6 B6 G6\n\t"
		     movntq " %%mm4, 24 (%3) # Store ARGB7 ARGB6\n\t"

		     "movd 4 (%1), %%mm0 # Load 4 Cb 00 00 00 00 u3 u2 u1 u0\n\t"
		     "movd 4 (%2), %%mm1 # Load 4 Cr 00 00 00 00 v3 v2 v1 v0\n\t"

		     "pxor %%mm4, %%mm4 # zero mm4\n\t"
		     "movq 8 (%0), %%mm6 # Load 8 Y Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0\n\t"

		     : : "r" (py), "r" (pu), "r" (pv), "r" (image));

	    py += 8;
	    pu += 4;
	    pv += 4;
	    image += 32;
	    x += 8;
	} while (x < h_size);

	if (even) {
	    pu -= h_size/2;
	    pv -= h_size/2;
	}

	/* load data for start of next scan line */
	__asm__ 
	    (
	     ".align 8 \n\t"
	     "movd (%1), %%mm0 # Load 4 Cb 00 00 00 00 u3 u2 u1 u0\n\t"
	     "movd (%2), %%mm1 # Load 4 Cr 00 00 00 00 v3 v2 v1 v0\n\t"

	     //"movl $0, (%3) # cache preload for image\n\t"
	     "movq (%0), %%mm6 # Load 8 Y Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0\n\t"
	     : : "r" (py), "r" (pu), "r" (pv), "r" (image)
	     );


	x = 0;
	y += 1;
	even = (!even);
    } while ( y < v_size) ;

    __asm__ ("emms\n\t");
}

yuv2rgb_fun yuv2rgb_init_mmx (int bpp, int mode)
{
//    if (bpp == 15 || bpp == 16) {
    if (bpp == 32 && mode == MODE_RGB) return yuv420_argb32_mmx;
    return NULL; // Fallback to C.
}

#define SCALEBITS 8
#define ONE_HALF  (1 << (SCALEBITS - 1)) 
#define FIX(x)          ((int) ((x) * (1L<<SCALEBITS) + 0.5))

void rgb32_to_yuv420p(unsigned char *lum, unsigned char *cb, unsigned char *cr,
                      unsigned char *src, int width, int height)
{           
    int wrap, wrap4, x, y;
    int r, g, b, r1, g1, b1;
    unsigned char *p;
                      
    wrap = width;
    wrap4 = width * 4;
    p = src;
    for(y=0;y<height;y+=2) {
        for(x=0;x<width;x+=2) {
            r = p[0];
            g = p[1];
            b = p[2];
            r1 = r;
            g1 = g;
            b1 = b;
            lum[0] = (FIX(0.29900) * r + FIX(0.58700) * g +
                      FIX(0.11400) * b + ONE_HALF) >> SCALEBITS;
            r = p[4];
            g = p[5];
            b = p[6];
            r1 += r;
            g1 += g;
            b1 += b;
            lum[1] = (FIX(0.29900) * r + FIX(0.58700) * g +
                      FIX(0.11400) * b + ONE_HALF) >> SCALEBITS;
            p += wrap4;
            lum += wrap;

            r = p[0];
            g = p[1];
            b = p[2];
            r1 += r;
            g1 += g;
            b1 += b;
            lum[0] = (FIX(0.29900) * r + FIX(0.58700) * g +
                      FIX(0.11400) * b + ONE_HALF) >> SCALEBITS;
            r = p[4];
            g = p[5];
            b = p[6];
            r1 += r;
            g1 += g;
            b1 += b;
            lum[1] = (FIX(0.29900) * r + FIX(0.58700) * g +
                      FIX(0.11400) * b + ONE_HALF) >> SCALEBITS;

            cb[0] = ((- FIX(0.16874) * r1 - FIX(0.33126) * g1 +
                      FIX(0.50000) * b1 + 4 * ONE_HALF - 1) >> (SCALEBITS + 2)) + 128;
            cr[0] = ((FIX(0.50000) * r1 - FIX(0.41869) * g1 -
                     FIX(0.08131) * b1 + 4 * ONE_HALF - 1) >> (SCALEBITS + 2)) + 128;

            cb++;
            cr++;
            p += -wrap4 + 2 * 4;
            lum += -wrap + 2;
        }
        p += wrap4;
        lum += wrap;
    }
}

