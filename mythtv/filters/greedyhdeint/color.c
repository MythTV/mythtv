/*
 * Taken from Xine Project (color.c) for mythtv-greedyHighMotion Deinterlacer
 * Copyright (C) 2000-2003 the xine project
 *
 * This file is part of xine, a free video player.
 *
 * xine is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * xine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 *
 */

#include "config.h"

#include <string.h>
#include <stdlib.h>
#if HAVE_STDINT_H
#include <stdint.h>
#endif
#include <math.h>

#include "libavutil/mem.h"
#include "libavcodec/dsputil.h"

#include "color.h"

#if HAVE_MMX
#include "ffmpeg-mmx.h"
#endif

void (*yv12_to_yuy2)
  (const unsigned char *y_src, int y_src_pitch, 
   const unsigned char *u_src, int u_src_pitch, 
   const unsigned char *v_src, int v_src_pitch, 
   unsigned char *yuy2_map, int yuy2_pitch,
   int width, int height, int progressive);

void (*yuy2_to_yv12)
  (const unsigned char *yuy2_map, int yuy2_pitch,
   unsigned char *y_dst, int y_dst_pitch, 
   unsigned char *u_dst, int u_dst_pitch, 
   unsigned char *v_dst, int v_dst_pitch, 
   int width, int height);

void (*vfilter_chroma_332_packed422_scanline)( uint8_t *output, int width, uint8_t *m, uint8_t *t, uint8_t *b );


#define C_YUV420_YUYV( )                                          \
    *p_line1++ = *p_y1++; *p_line2++ = *p_y2++;                   \
*p_line1++ = *p_u;    *p_line2++ = (*p_u++ + *p_u2++)>>1;     \
*p_line1++ = *p_y1++; *p_line2++ = *p_y2++;                   \
*p_line1++ = *p_v;    *p_line2++ = (*p_v++ + *p_v2++)>>1;

/*****************************************************************************
 * I420_YUY2: planar YUV 4:2:0 to packed YUYV 4:2:2
 * original conversion routine from Videolan project
 * changed to support interlaced frames and use simple mean interpolation [MF]
 *****************************************************************************/
static void yv12_to_yuy2_c
(const unsigned char *y_src, int y_src_pitch, 
 const unsigned char *u_src, int u_src_pitch, 
 const unsigned char *v_src, int v_src_pitch, 
 unsigned char *yuy2_map, int yuy2_pitch,
 int width, int height, int progressive) 
{

    uint8_t *p_line1, *p_line2 = yuy2_map;
    const uint8_t *p_y1, *p_y2 = y_src;
    const uint8_t *p_u = u_src;
    const uint8_t *p_v = v_src;
    const uint8_t *p_u2 = u_src + u_src_pitch;
    const uint8_t *p_v2 = v_src + v_src_pitch;

    int i_x, i_y;

    const int i_source_margin = y_src_pitch - width;
    const int i_source_u_margin = u_src_pitch - width/2;
    const int i_source_v_margin = v_src_pitch - width/2;
    const int i_dest_margin = yuy2_pitch - width*2;


    if ( progressive ) 
    {
        for ( i_y = height / 2 ; i_y-- ; )
        {
            p_line1 = p_line2;
            p_line2 += yuy2_pitch;

            p_y1 = p_y2;
            p_y2 += y_src_pitch;

            for ( i_x = width / 2 ; i_x-- ; )
            {
                C_YUV420_YUYV( );
            }

            p_y2 += i_source_margin;
            p_u += i_source_u_margin;
            p_v += i_source_v_margin;
            if ( i_y > 1 ) 
            {
                p_u2 += i_source_u_margin;
                p_v2 += i_source_v_margin;
            } 
            else 
            {
                p_u2 = p_u;
                p_v2 = p_v;
            }
            p_line2 += i_dest_margin;
        }
    }
    else 
    {
        p_u2 = u_src + 2*u_src_pitch;
        p_v2 = v_src + 2*v_src_pitch;
        for ( i_y = height / 4 ; i_y-- ; )
        {
            p_line1 = p_line2;
            p_line2 += 2 * yuy2_pitch;

            p_y1 = p_y2;
            p_y2 += 2 * y_src_pitch;

            for ( i_x = width / 2 ; i_x-- ; )
            {
                C_YUV420_YUYV( );
            }

            p_y2 += i_source_margin + y_src_pitch;
            p_u += i_source_u_margin + u_src_pitch;
            p_v += i_source_v_margin + v_src_pitch;
            if ( i_y > 1 ) 
            {
                p_u2 += i_source_u_margin + u_src_pitch;
                p_v2 += i_source_v_margin + v_src_pitch;
            } 
            else 
            {
                p_u2 = p_u;
                p_v2 = p_v;
            }
            p_line2 += i_dest_margin + yuy2_pitch;
        }

        p_line2 = yuy2_map + yuy2_pitch;
        p_y2 = y_src + y_src_pitch;
        p_u = u_src + u_src_pitch;
        p_v = v_src + v_src_pitch;
        p_u2 = u_src + 3*u_src_pitch;
        p_v2 = v_src + 3*v_src_pitch;

        for ( i_y = height / 4 ; i_y-- ; )
        {
            p_line1 = p_line2;
            p_line2 += 2 * yuy2_pitch;

            p_y1 = p_y2;
            p_y2 += 2 * y_src_pitch;

            for ( i_x = width / 2 ; i_x-- ; )
            {
                C_YUV420_YUYV( );
            }

            p_y2 += i_source_margin + y_src_pitch;
            p_u += i_source_u_margin + u_src_pitch;
            p_v += i_source_v_margin + v_src_pitch;
            if ( i_y > 1 ) 
            {
                p_u2 += i_source_u_margin + u_src_pitch;
                p_v2 += i_source_v_margin + v_src_pitch;
            } 
            else 
            {
                p_u2 = p_u;
                p_v2 = p_v;
            }
            p_line2 += i_dest_margin + yuy2_pitch;
        }
    }
}


#if HAVE_MMX

#define MMXEXT_YUV420_YUYV( )                                                      \
    do {                                                                               \
        __asm__ __volatile__(".align 8 \n\t"                                            \
                "movq       (%0), %%mm0 \n\t"  /* Load 8 Y          y7 y6 y5 y4 y3 y2 y1 y0 */ \
                "movd       (%1), %%mm1 \n\t"  /* Load 4 Cb         00 00 00 00 u3 u2 u1 u0 */ \
                "movd       (%2), %%mm2 \n\t"  /* Load 4 Cr         00 00 00 00 v3 v2 v1 v0 */ \
                "punpcklbw %%mm2, %%mm1 \n\t"  /*                   v3 u3 v2 u2 v1 u1 v0 u0 */ \
                "movq      %%mm0, %%mm2 \n\t"  /*                   y7 y6 y5 y4 y3 y2 y1 y0 */ \
                "punpcklbw %%mm1, %%mm2 \n\t"  /*                   v1 y3 u1 y2 v0 y1 u0 y0 */ \
                :                                                                              \
                : "r" (p_y1), "r" (p_u), "r" (p_v) );                                          \
        __asm__ __volatile__(                                                           \
                "movd       (%0), %%mm3 \n\t"  /* Load 4 Cb         00 00 00 00 u3 u2 u1 u0 */ \
                "movd       (%1), %%mm4 \n\t"  /* Load 4 Cr         00 00 00 00 v3 v2 v1 v0 */ \
                "punpcklbw %%mm4, %%mm3 \n\t"  /*                   v3 u3 v2 u2 v1 u1 v0 u0 */ \
                "pavgb     %%mm1, %%mm3 \n\t"  /* (mean)            v3 u3 v2 u2 v1 u1 v0 u0 */ \
                :                                                                              \
                : "r" (p_u2), "r" (p_v2) );                                                    \
        __asm__ __volatile__(                                                           \
                "movntq    %%mm2, (%0)  \n\t"  /* Store low YUYV                            */ \
                "punpckhbw %%mm1, %%mm0 \n\t"  /*                   v3 y7 u3 y6 v2 y5 u2 y4 */ \
                "movntq    %%mm0, 8(%0) \n\t"  /* Store high YUYV                           */ \
                "movq       (%2), %%mm0 \n\t"  /* Load 8 Y          Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0 */ \
                "movq      %%mm0, %%mm2 \n\t"  /*                   Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0 */ \
                "punpcklbw %%mm3, %%mm2 \n\t"  /*                   v1 Y3 u1 Y2 v0 Y1 u0 Y0 */ \
                "movntq    %%mm2, (%1)  \n\t"  /* Store low YUYV                            */ \
                "punpckhbw %%mm3, %%mm0 \n\t"  /*                   v3 Y7 u3 Y6 v2 Y5 u2 Y4 */ \
                "movntq    %%mm0, 8(%1) \n\t"  /* Store high YUYV                           */ \
                :                                                                              \
                : "r" (p_line1),  "r" (p_line2),  "r" (p_y2) );                                \
        p_line1 += 16; p_line2 += 16; p_y1 += 8; p_y2 += 8; p_u += 4; p_v += 4;          \
        p_u2 += 4; p_v2 += 4;                                                            \
    } while(0)

#endif

#if HAVE_MMX
static void yv12_to_yuy2_mmxext
(const unsigned char *y_src, int y_src_pitch, 
 const unsigned char *u_src, int u_src_pitch, 
 const unsigned char *v_src, int v_src_pitch, 
 unsigned char *yuy2_map, int yuy2_pitch,
 int width, int height, int progressive ) 
{
    uint8_t *p_line1, *p_line2 = yuy2_map;
    const uint8_t *p_y1, *p_y2 = y_src;
    const uint8_t *p_u = u_src;
    const uint8_t *p_v = v_src;
    const uint8_t *p_u2 = u_src + u_src_pitch;
    const uint8_t *p_v2 = v_src + v_src_pitch;

    int i_x, i_y;

    const int i_source_margin = y_src_pitch - width;
    const int i_source_u_margin = u_src_pitch - width/2;
    const int i_source_v_margin = v_src_pitch - width/2;
    const int i_dest_margin = yuy2_pitch - width*2;

    if ( progressive ) 
    {
        for ( i_y = height / 2; i_y-- ; )
        {
            p_line1 = p_line2;
            p_line2 += yuy2_pitch;

            p_y1 = p_y2;
            p_y2 += y_src_pitch;

            for ( i_x = width / 8 ; i_x-- ; )
            {
                MMXEXT_YUV420_YUYV( );
            }
            for ( i_x = (width % 8) / 2 ; i_x-- ; )
            {
                C_YUV420_YUYV( );
            }

            p_y2 += i_source_margin;
            p_u += i_source_u_margin;
            p_v += i_source_v_margin;
            if ( i_y > 1 ) 
            {
                p_u2 += i_source_u_margin;
                p_v2 += i_source_v_margin;
            } 
            else 
            {
                p_u2 = p_u;
                p_v2 = p_v;
            }
            p_line2 += i_dest_margin;
        }
    } 
    else
    {
        p_u2 = u_src + 2*u_src_pitch;
        p_v2 = v_src + 2*v_src_pitch;
        for ( i_y = height / 4 ; i_y-- ; )
        {
            p_line1 = p_line2;
            p_line2 += 2 * yuy2_pitch;

            p_y1 = p_y2;
            p_y2 += 2 * y_src_pitch;

            for ( i_x = width / 8 ; i_x-- ; )
            {
                MMXEXT_YUV420_YUYV( );
            }
            for ( i_x = (width % 8) / 2 ; i_x-- ; )
            {
                C_YUV420_YUYV( );
            }

            p_y2 += i_source_margin + y_src_pitch;
            p_u += i_source_u_margin + u_src_pitch;
            p_v += i_source_v_margin + v_src_pitch;
            if ( i_y > 1 ) 
            {
                p_u2 += i_source_u_margin + u_src_pitch;
                p_v2 += i_source_v_margin + v_src_pitch;
            } 
            else 
            {
                p_u2 = p_u;
                p_v2 = p_v;
            }
            p_line2 += i_dest_margin + yuy2_pitch;
        }

        p_line2 = yuy2_map + yuy2_pitch;
        p_y2 = y_src + y_src_pitch;
        p_u = u_src + u_src_pitch;
        p_v = v_src + v_src_pitch;
        p_u2 = u_src + 3*u_src_pitch;
        p_v2 = v_src + 3*v_src_pitch;

        for ( i_y = height / 4 ; i_y-- ; )
        {
            p_line1 = p_line2;
            p_line2 += 2 * yuy2_pitch;

            p_y1 = p_y2;
            p_y2 += 2 * y_src_pitch;

            for ( i_x = width / 8 ; i_x-- ; )
            {
                MMXEXT_YUV420_YUYV( );
            }
            for ( i_x = (width % 8) / 2 ; i_x-- ; )
            {
                C_YUV420_YUYV( );
            }

            p_y2 += i_source_margin + y_src_pitch;
            p_u += i_source_u_margin + u_src_pitch;
            p_v += i_source_v_margin + v_src_pitch;
            if ( i_y > 1 ) 
            {
                p_u2 += i_source_u_margin + u_src_pitch;
                p_v2 += i_source_v_margin + v_src_pitch;
            } 
            else 
            {
                p_u2 = p_u;
                p_v2 = p_v;
            }
            p_line2 += i_dest_margin + yuy2_pitch;
        }
    }

    sfence();
    emms();
}
#endif

#define C_YUYV_YUV420( )                                          \
    *p_y1++ = *p_line1++; *p_y2++ = *p_line2++;                   \
*p_u++ = (*p_line1++ + *p_line2++)>>1;                        \
*p_y1++ = *p_line1++; *p_y2++ = *p_line2++;                   \
*p_v++ = (*p_line1++ + *p_line2++)>>1;

static void yuy2_to_yv12_c
(const unsigned char *yuy2_map, int yuy2_pitch,
 unsigned char *y_dst, int y_dst_pitch, 
 unsigned char *u_dst, int u_dst_pitch, 
 unsigned char *v_dst, int v_dst_pitch, 
 int width, int height) 
{

    const uint8_t *p_line1, *p_line2 = yuy2_map;
    uint8_t *p_y1, *p_y2 = y_dst;
    uint8_t *p_u = u_dst;
    uint8_t *p_v = v_dst;

    int i_x, i_y;

    const int i_dest_margin = y_dst_pitch - width;
    const int i_dest_u_margin = u_dst_pitch - width/2;
    const int i_dest_v_margin = v_dst_pitch - width/2;
    const int i_source_margin = yuy2_pitch - width*2;


    for ( i_y = height / 2 ; i_y-- ; )
    {
        p_line1 = p_line2;
        p_line2 += yuy2_pitch;

        p_y1 = p_y2;
        p_y2 += y_dst_pitch;

        for ( i_x = width / 8 ; i_x-- ; )
        {
            C_YUYV_YUV420( );
            C_YUYV_YUV420( );
            C_YUYV_YUV420( );
            C_YUYV_YUV420( );
        }

        p_y2 += i_dest_margin;
        p_u += i_dest_u_margin;
        p_v += i_dest_v_margin;
        p_line2 += i_source_margin;
    }
}


#if HAVE_MMX

/* yuy2->yv12 with subsampling (some ideas from mplayer's yuy2toyv12) */
#define MMXEXT_YUYV_YUV420( )                                                      \
    do {                                                                               \
        __asm__ __volatile__(".align 8 \n\t"                                            \
                "movq       (%0), %%mm0 \n\t"  /* Load              v1 y3 u1 y2 v0 y1 u0 y0 */ \
                "movq      8(%0), %%mm1 \n\t"  /* Load              v3 y7 u3 y6 v2 y5 u2 y4 */ \
                "movq      %%mm0, %%mm2 \n\t"  /*                   v1 y3 u1 y2 v0 y1 u0 y0 */ \
                "movq      %%mm1, %%mm3 \n\t"  /*                   v3 y7 u3 y6 v2 y5 u2 y4 */ \
                "psrlw     $8, %%mm0    \n\t"  /*                   00 v1 00 u1 00 v0 00 u0 */ \
                "psrlw     $8, %%mm1    \n\t"  /*                   00 v3 00 u3 00 v2 00 u2 */ \
                "pand      %%mm7, %%mm2 \n\t"  /*                   00 y3 00 y2 00 y1 00 y0 */ \
                "pand      %%mm7, %%mm3 \n\t"  /*                   00 y7 00 y6 00 y5 00 y4 */ \
                "packuswb  %%mm1, %%mm0 \n\t"  /*                   v3 u3 v2 u2 v1 u1 v0 u0 */ \
                "packuswb  %%mm3, %%mm2 \n\t"  /*                   y7 y6 y5 y4 y3 y2 y1 y0 */ \
                "movntq    %%mm2, (%1)  \n\t"  /* Store YYYYYYYY line1                      */ \
                :                                                                              \
                : "r" (p_line1), "r" (p_y1) );                                                 \
        __asm__ __volatile__(".align 8 \n\t"                                            \
                "movq       (%0), %%mm1 \n\t"  /* Load              v1 y3 u1 y2 v0 y1 u0 y0 */ \
                "movq      8(%0), %%mm2 \n\t"  /* Load              v3 y7 u3 y6 v2 y5 u2 y4 */ \
                "movq      %%mm1, %%mm3 \n\t"  /*                   v1 y3 u1 y2 v0 y1 u0 y0 */ \
                "movq      %%mm2, %%mm4 \n\t"  /*                   v3 y7 u3 y6 v2 y5 u2 y4 */ \
                "psrlw     $8, %%mm1    \n\t"  /*                   00 v1 00 u1 00 v0 00 u0 */ \
                "psrlw     $8, %%mm2    \n\t"  /*                   00 v3 00 u3 00 v2 00 u2 */ \
                "pand      %%mm7, %%mm3 \n\t"  /*                   00 y3 00 y2 00 y1 00 y0 */ \
                "pand      %%mm7, %%mm4 \n\t"  /*                   00 y7 00 y6 00 y5 00 y4 */ \
                "packuswb  %%mm2, %%mm1 \n\t"  /*                   v3 u3 v2 u2 v1 u1 v0 u0 */ \
                "packuswb  %%mm4, %%mm3 \n\t"  /*                   y7 y6 y5 y4 y3 y2 y1 y0 */ \
                "movntq    %%mm3, (%1)  \n\t"  /* Store YYYYYYYY line2                      */ \
                :                                                                              \
                : "r" (p_line2), "r" (p_y2) );                                                 \
        __asm__ __volatile__(                                                           \
                "pavgb     %%mm1, %%mm0 \n\t"  /* (mean)            v3 u3 v2 u2 v1 u1 v0 u0 */ \
                "movq      %%mm0, %%mm1 \n\t"  /*                   v3 u3 v2 u2 v1 u1 v0 u0 */ \
                "psrlw     $8, %%mm0    \n\t"  /*                   00 v3 00 v2 00 v1 00 v0 */ \
                "packuswb  %%mm0, %%mm0 \n\t"  /*                               v3 v2 v1 v0 */ \
                "movd      %%mm0, (%0)  \n\t"  /* Store VVVV                                */ \
                "pand      %%mm7, %%mm1 \n\t"  /*                   00 u3 00 u2 00 u1 00 u0 */ \
                "packuswb  %%mm1, %%mm1 \n\t"  /*                               u3 u2 u1 u0 */ \
                "movd      %%mm1, (%1)  \n\t"  /* Store UUUU                                */ \
                :                                                                              \
                : "r" (p_v), "r" (p_u) );                                                      \
        p_line1 += 16; p_line2 += 16; p_y1 += 8; p_y2 += 8; p_u += 4; p_v += 4;          \
    } while(0)

#endif

#if HAVE_MMX
static void yuy2_to_yv12_mmxext
(const unsigned char *yuy2_map, int yuy2_pitch,
 unsigned char *y_dst, int y_dst_pitch, 
 unsigned char *u_dst, int u_dst_pitch, 
 unsigned char *v_dst, int v_dst_pitch, 
 int width, int height) 
{
    const uint8_t *p_line1, *p_line2 = yuy2_map;
    uint8_t *p_y1, *p_y2 = y_dst;
    uint8_t *p_u = u_dst;
    uint8_t *p_v = v_dst;

    int i_x, i_y;

    const int i_dest_margin = y_dst_pitch - width;
    const int i_dest_u_margin = u_dst_pitch - width/2;
    const int i_dest_v_margin = v_dst_pitch - width/2;
    const int i_source_margin = yuy2_pitch - width*2;

    __asm__ __volatile__(
            "pcmpeqw %mm7, %mm7           \n\t"
            "psrlw $8, %mm7               \n\t" /* 00 ff 00 ff 00 ff 00 ff */
            );

    for ( i_y = height / 2 ; i_y-- ; )
    {
        p_line1 = p_line2;
        p_line2 += yuy2_pitch;

        p_y1 = p_y2;
        p_y2 += y_dst_pitch;

        for ( i_x = width / 8 ; i_x-- ; )
        {
            MMXEXT_YUYV_YUV420( );
        }

        p_y2 += i_dest_margin;
        p_u += i_dest_u_margin;
        p_v += i_dest_v_margin;
        p_line2 += i_source_margin;
    }

    sfence();
    emms();
}
#endif

#if HAVE_MMX
static void vfilter_chroma_332_packed422_scanline_mmx( uint8_t *output, int width,
        uint8_t *m, uint8_t *t, uint8_t *b )
{
    int i;
    const mmx_t ymask = { 0x00ff00ff00ff00ffULL };
    const mmx_t cmask = { 0xff00ff00ff00ff00ULL };

    // Get width in bytes.
    width *= 2; 
    i = width / 8;
    width -= i * 8;

    movq_m2r( ymask, mm7 );
    movq_m2r( cmask, mm6 );

    while ( i-- ) 
    {
        movq_m2r( *t, mm0 );
        movq_m2r( *b, mm1 );
        movq_m2r( *m, mm2 );

        movq_r2r ( mm2, mm3 );
        pand_r2r ( mm7, mm3 );

        pand_r2r ( mm6, mm0 );
        pand_r2r ( mm6, mm1 );
        pand_r2r ( mm6, mm2 );

        psrlq_i2r( 8, mm0 );
        psrlq_i2r( 7, mm1 );
        psrlq_i2r( 8, mm2 );

        movq_r2r ( mm0, mm4 );
        psllw_i2r( 1, mm4 );
        paddw_r2r( mm4, mm0 );

        movq_r2r ( mm2, mm4 );
        psllw_i2r( 1, mm4 );
        paddw_r2r( mm4, mm2 );

        paddw_r2r( mm0, mm2 );
        paddw_r2r( mm1, mm2 );

        psllw_i2r( 5, mm2 );
        pand_r2r( mm6, mm2 );

        por_r2r ( mm3, mm2 );

        movq_r2m( mm2, *output );
        output += 8;
        t += 8;
        b += 8;
        m += 8;
    }
    output++; t++; b++; m++;
    while ( width-- ) 
    {
        *output = (3 * *t + 3 * *m + 2 * *b) >> 3;
        output +=2; t+=2; b+=2; m+=2;
    }

    emms();
}
#endif

static void vfilter_chroma_332_packed422_scanline_c( uint8_t *output, int width,
        uint8_t *m, uint8_t *t, uint8_t *b )
{
    output++; t++; b++; m++;
    while ( width-- ) 
    {
        *output = (3 * *t + 3 * *m + 2 * *b) >> 3;
        output +=2; t+=2; b+=2; m+=2;
    }
}
/*
 * init_yuv_conversion
 *
 * This function precalculates all of the tables used for converting RGB
 * values to YUV values. This function also decides which conversion
 * functions to use.
 */
void init_yuv_conversion(void) 
{
    /* determine best YUV444 -> YUY2 converter to use */
    /* determine best YV12 -> YUY2 converter to use */
    /* determine best YV12 -> YUY2 converter to use */

#ifdef MMX
    if (av_get_cpu_flags() & AV_CPU_FLAG_MMX2)
    {
        yv12_to_yuy2 = yv12_to_yuy2_mmxext;
        yuy2_to_yv12 = yuy2_to_yv12_mmxext;
        vfilter_chroma_332_packed422_scanline = vfilter_chroma_332_packed422_scanline_mmx;
    }
    else
#endif
    {
        yv12_to_yuy2 = yv12_to_yuy2_c;
        yuy2_to_yv12 = yuy2_to_yv12_c;
        vfilter_chroma_332_packed422_scanline = vfilter_chroma_332_packed422_scanline_c;
    }
}

void apply_chroma_filter( uint8_t *data, int stride, int width, int height )
{
    int i;

    /* ok, using linearblend inplace is a bit weird: the result of a scanline
     * interpolation will affect the next scanline. this might not be a problem
     * at all, we just want a kind of filter here.
     */
    for ( i = 0; i < height; i++, data += stride ) 
    {
        vfilter_chroma_332_packed422_scanline( data, width,
                data, 
                (i) ? (data - stride) : data,
                (i < height-1) ? (data + stride) : data );
    }
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
