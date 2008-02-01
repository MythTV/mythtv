/*
 * Yadif 
 * 
 * Original taken from mplayer (vf_yadif.c)
    Copyright (C) 2006 Michael Niedermayer <michaelni@gmx.at>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * converted for myth by Markus Schulz <msc@antzsystem.de>
 * */
#include <stdlib.h>
#include <stdio.h>

#include "config.h"
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <inttypes.h>

#include <string.h>
#include <math.h>

#include "filter.h"
#include "frame.h"

#define MIN(a,b) ((a) > (b) ? (b) : (a))
#define MAX(a,b) ((a) < (b) ? (b) : (a))
#define ABS(a) ((a) > 0 ? (a) : (-(a)))

#define MIN3(a,b,c) MIN(MIN(a,b),c)
#define MAX3(a,b,c) MAX(MAX(a,b),c)

#ifdef MMX
#include "dsputil.h"
#include "i386/mmx.h"
#endif

#include "aclib.h"

static void* (*fast_memcpy)(void * to, const void * from, size_t len);

typedef struct ThisFilter
{
    VideoFilter vf;

    long long last_framenr;

    uint8_t *ref[4][3];
    int stride[3];
    int8_t got_frames[4];

    void (*filter_line)(struct ThisFilter *p, uint8_t *dst, uint8_t *prev, uint8_t *cur, uint8_t *next, int w, int refs, int parity);

    int mode;
    int width;
    int height;

    int mm_flags;
    TF_STRUCT;
} ThisFilter;


static void AllocFilter(ThisFilter* filter, int width, int height)
{
    int i,j;
    if ((width != filter->width) || height != filter->height)
    {
        printf("yadifdeint: size changed from %d x %d -> %d x %d\n", filter->width, filter->height, width, height);
        for (i=0; i<3*3; i++)
        {
            uint8_t **p= &filter->ref[i%3][i/3];
            if (*p) free(*p - 3*filter->stride[i/3]);
            *p= NULL;
        }
        for (i=0; i<3; i++)
        {
            int is_chroma= !!i;
            int w= ((width   + 31) & (~31))>>is_chroma;
            int h= ((height+6+ 31) & (~31))>>is_chroma;

            filter->stride[i]= w;
            for (j=0; j<3; j++) 
            {
                //3lines at top and bottom as save space for save [pos +- 2*ref] memory access
                filter->ref[j][i]= (uint8_t*)calloc(w*h*sizeof(uint8_t),1)+3*w;
            }
        }
        filter->width = width;
        filter->height = height;
        memset(filter->got_frames, 0, sizeof(filter->got_frames));
    }
}

static inline void * memcpy_pic2(void * dst, const void * src,
                                 int bytesPerLine, int height,
                                 int dstStride, int srcStride, int limit2width)
{
    int i;
    void *retval=dst;

    if (!limit2width && dstStride == srcStride)
    {
        if (srcStride < 0) 
        {
            src = (uint8_t*)src + (height-1)*srcStride;
            dst = (uint8_t*)dst + (height-1)*dstStride;
            srcStride = -srcStride;
        }
        fast_memcpy(dst, src, srcStride*height);
    }
    else
    {
        for (i=0; i<height; i++)
        {
            fast_memcpy(dst, src, bytesPerLine);
            src = (uint8_t*)src + srcStride;
            dst = (uint8_t*)dst + dstStride;
        }
    }

    return retval;
}
#define memcpy_pic(d, s, b, h, ds, ss) memcpy_pic2(d, s, b, h, ds, ss, 0)

static void store_ref(struct ThisFilter *p, uint8_t *src, int src_offsets[3], int src_stride[3], int width, int height)
{
    int i;

    memcpy (p->ref[3], p->ref[0], sizeof(uint8_t *)*3);
    memmove(p->ref[0], p->ref[1], sizeof(uint8_t *)*3*3);

    memcpy (&p->got_frames[3], &p->got_frames[0], sizeof(uint8_t));
    memmove(&p->got_frames[0], &p->got_frames[1], sizeof(uint8_t) * 3);

    for (i=0; i<3; i++)
    {
        int is_chroma= !!i;
        memcpy_pic(p->ref[2][i], src + src_offsets[i], width>>is_chroma, height>>is_chroma, p->stride[i], src_stride[i]);
    }
    p->got_frames[2] = 1;
}


#if defined(MMX)

#define LOAD4(mem,dst) \
            "movd      "mem", "#dst" \n\t"\
            "punpcklbw %%mm7, "#dst" \n\t"

#define PABS(tmp,dst) \
            "pxor     "#tmp", "#tmp" \n\t"\
            "psubw    "#dst", "#tmp" \n\t"\
            "pmaxsw   "#tmp", "#dst" \n\t"

#define CHECK(pj,mj) \
            "movq "#pj"(%[cur],%[mrefs]), %%mm2 \n\t" /* cur[x-refs-1+j] */\
            "movq "#mj"(%[cur],%[prefs]), %%mm3 \n\t" /* cur[x+refs-1-j] */\
            "movq      %%mm2, %%mm4 \n\t"\
            "movq      %%mm2, %%mm5 \n\t"\
            "pxor      %%mm3, %%mm4 \n\t"\
            "pavgb     %%mm3, %%mm5 \n\t"\
            "pand     %[pb1], %%mm4 \n\t"\
            "psubusb   %%mm4, %%mm5 \n\t"\
            "psrlq     $8,    %%mm5 \n\t"\
            "punpcklbw %%mm7, %%mm5 \n\t" /* (cur[x-refs+j] + cur[x+refs-j])>>1 */\
            "movq      %%mm2, %%mm4 \n\t"\
            "psubusb   %%mm3, %%mm2 \n\t"\
            "psubusb   %%mm4, %%mm3 \n\t"\
            "pmaxub    %%mm3, %%mm2 \n\t"\
            "movq      %%mm2, %%mm3 \n\t"\
            "movq      %%mm2, %%mm4 \n\t" /* ABS(cur[x-refs-1+j] - cur[x+refs-1-j]) */\
            "psrlq      $8,   %%mm3 \n\t" /* ABS(cur[x-refs  +j] - cur[x+refs  -j]) */\
            "psrlq     $16,   %%mm4 \n\t" /* ABS(cur[x-refs+1+j] - cur[x+refs+1-j]) */\
            "punpcklbw %%mm7, %%mm2 \n\t"\
            "punpcklbw %%mm7, %%mm3 \n\t"\
            "punpcklbw %%mm7, %%mm4 \n\t"\
            "paddw     %%mm3, %%mm2 \n\t"\
            "paddw     %%mm4, %%mm2 \n\t" /* score */

#define CHECK1 \
            "movq      %%mm0, %%mm3 \n\t"\
            "pcmpgtw   %%mm2, %%mm3 \n\t" /* if (score < spatial_score) */\
            "pminsw    %%mm2, %%mm0 \n\t" /* spatial_score= score; */\
            "movq      %%mm3, %%mm6 \n\t"\
            "pand      %%mm3, %%mm5 \n\t"\
            "pandn     %%mm1, %%mm3 \n\t"\
            "por       %%mm5, %%mm3 \n\t"\
            "movq      %%mm3, %%mm1 \n\t" /* spatial_pred= (cur[x-refs+j] + cur[x+refs-j])>>1; */

#define CHECK2 /* pretend not to have checked dir=2 if dir=1 was bad.\
                  hurts both quality and speed, but matches the C version. */\
            "paddw    %[pw1], %%mm6 \n\t"\
            "psllw     $14,   %%mm6 \n\t"\
            "paddsw    %%mm6, %%mm2 \n\t"\
            "movq      %%mm0, %%mm3 \n\t"\
            "pcmpgtw   %%mm2, %%mm3 \n\t"\
            "pminsw    %%mm2, %%mm0 \n\t"\
            "pand      %%mm3, %%mm5 \n\t"\
            "pandn     %%mm1, %%mm3 \n\t"\
            "por       %%mm5, %%mm3 \n\t"\
            "movq      %%mm3, %%mm1 \n\t"

static void filter_line_mmx2(struct ThisFilter *p, uint8_t *dst, uint8_t *prev, uint8_t *cur, uint8_t *next, int w, int refs, int parity)
{
    static const uint64_t pw_1 = 0x0001000100010001ULL;
    static const uint64_t pb_1 = 0x0101010101010101ULL;
    const int mode = p->mode;
    uint64_t tmp0, tmp1, tmp2, tmp3;
    int x;

#define FILTER\
    for (x=0; x<w; x+=4){\
        asm volatile(\
            "pxor      %%mm7, %%mm7 \n\t"\
            LOAD4("(%[cur],%[mrefs])", %%mm0) /* c = cur[x-refs] */\
            LOAD4("(%[cur],%[prefs])", %%mm1) /* e = cur[x+refs] */\
            LOAD4("(%["prev2"])", %%mm2) /* prev2[x] */\
            LOAD4("(%["next2"])", %%mm3) /* next2[x] */\
            "movq      %%mm3, %%mm4 \n\t"\
            "paddw     %%mm2, %%mm3 \n\t"\
            "psraw     $1,    %%mm3 \n\t" /* d = (prev2[x] + next2[x])>>1 */\
            "movq      %%mm0, %[tmp0] \n\t" /* c */\
            "movq      %%mm3, %[tmp1] \n\t" /* d */\
            "movq      %%mm1, %[tmp2] \n\t" /* e */\
            "psubw     %%mm4, %%mm2 \n\t"\
            PABS(      %%mm4, %%mm2) /* temporal_diff0 */\
            LOAD4("(%[prev],%[mrefs])", %%mm3) /* prev[x-refs] */\
            LOAD4("(%[prev],%[prefs])", %%mm4) /* prev[x+refs] */\
            "psubw     %%mm0, %%mm3 \n\t"\
            "psubw     %%mm1, %%mm4 \n\t"\
            PABS(      %%mm5, %%mm3)\
            PABS(      %%mm5, %%mm4)\
            "paddw     %%mm4, %%mm3 \n\t" /* temporal_diff1 */\
            "psrlw     $1,    %%mm2 \n\t"\
            "psrlw     $1,    %%mm3 \n\t"\
            "pmaxsw    %%mm3, %%mm2 \n\t"\
            LOAD4("(%[next],%[mrefs])", %%mm3) /* next[x-refs] */\
            LOAD4("(%[next],%[prefs])", %%mm4) /* next[x+refs] */\
            "psubw     %%mm0, %%mm3 \n\t"\
            "psubw     %%mm1, %%mm4 \n\t"\
            PABS(      %%mm5, %%mm3)\
            PABS(      %%mm5, %%mm4)\
            "paddw     %%mm4, %%mm3 \n\t" /* temporal_diff2 */\
            "psrlw     $1,    %%mm3 \n\t"\
            "pmaxsw    %%mm3, %%mm2 \n\t"\
            "movq      %%mm2, %[tmp3] \n\t" /* diff */\
\
            "paddw     %%mm0, %%mm1 \n\t"\
            "paddw     %%mm0, %%mm0 \n\t"\
            "psubw     %%mm1, %%mm0 \n\t"\
            "psrlw     $1,    %%mm1 \n\t" /* spatial_pred */\
            PABS(      %%mm2, %%mm0)      /* ABS(c-e) */\
\
            "movq -1(%[cur],%[mrefs]), %%mm2 \n\t" /* cur[x-refs-1] */\
            "movq -1(%[cur],%[prefs]), %%mm3 \n\t" /* cur[x+refs-1] */\
            "movq      %%mm2, %%mm4 \n\t"\
            "psubusb   %%mm3, %%mm2 \n\t"\
            "psubusb   %%mm4, %%mm3 \n\t"\
            "pmaxub    %%mm3, %%mm2 \n\t"\
            "pshufw $9,%%mm2, %%mm3 \n\t"\
            "punpcklbw %%mm7, %%mm2 \n\t" /* ABS(cur[x-refs-1] - cur[x+refs-1]) */\
            "punpcklbw %%mm7, %%mm3 \n\t" /* ABS(cur[x-refs+1] - cur[x+refs+1]) */\
            "paddw     %%mm2, %%mm0 \n\t"\
            "paddw     %%mm3, %%mm0 \n\t"\
            "psubw    %[pw1], %%mm0 \n\t" /* spatial_score */\
\
            CHECK(-2,0)\
            CHECK1\
            CHECK(-3,1)\
            CHECK2\
            CHECK(0,-2)\
            CHECK1\
            CHECK(1,-3)\
            CHECK2\
\
            /* if (p->mode<2) ... */\
            "movq    %[tmp3], %%mm6 \n\t" /* diff */\
            "cmp       $2, %[mode] \n\t"\
            "jge       1f \n\t"\
            LOAD4("(%["prev2"],%[mrefs],2)", %%mm2) /* prev2[x-2*refs] */\
            LOAD4("(%["next2"],%[mrefs],2)", %%mm4) /* next2[x-2*refs] */\
            LOAD4("(%["prev2"],%[prefs],2)", %%mm3) /* prev2[x+2*refs] */\
            LOAD4("(%["next2"],%[prefs],2)", %%mm5) /* next2[x+2*refs] */\
            "paddw     %%mm4, %%mm2 \n\t"\
            "paddw     %%mm5, %%mm3 \n\t"\
            "psrlw     $1,    %%mm2 \n\t" /* b */\
            "psrlw     $1,    %%mm3 \n\t" /* f */\
            "movq    %[tmp0], %%mm4 \n\t" /* c */\
            "movq    %[tmp1], %%mm5 \n\t" /* d */\
            "movq    %[tmp2], %%mm7 \n\t" /* e */\
            "psubw     %%mm4, %%mm2 \n\t" /* b-c */\
            "psubw     %%mm7, %%mm3 \n\t" /* f-e */\
            "movq      %%mm5, %%mm0 \n\t"\
            "psubw     %%mm4, %%mm5 \n\t" /* d-c */\
            "psubw     %%mm7, %%mm0 \n\t" /* d-e */\
            "movq      %%mm2, %%mm4 \n\t"\
            "pminsw    %%mm3, %%mm2 \n\t"\
            "pmaxsw    %%mm4, %%mm3 \n\t"\
            "pmaxsw    %%mm5, %%mm2 \n\t"\
            "pminsw    %%mm5, %%mm3 \n\t"\
            "pmaxsw    %%mm0, %%mm2 \n\t" /* max */\
            "pminsw    %%mm0, %%mm3 \n\t" /* min */\
            "pxor      %%mm4, %%mm4 \n\t"\
            "pmaxsw    %%mm3, %%mm6 \n\t"\
            "psubw     %%mm2, %%mm4 \n\t" /* -max */\
            "pmaxsw    %%mm4, %%mm6 \n\t" /* diff= MAX3(diff, min, -max); */\
            "1: \n\t"\
\
            "movq    %[tmp1], %%mm2 \n\t" /* d */\
            "movq      %%mm2, %%mm3 \n\t"\
            "psubw     %%mm6, %%mm2 \n\t" /* d-diff */\
            "paddw     %%mm6, %%mm3 \n\t" /* d+diff */\
            "pmaxsw    %%mm2, %%mm1 \n\t"\
            "pminsw    %%mm3, %%mm1 \n\t" /* d = clip(spatial_pred, d-diff, d+diff); */\
            "packuswb  %%mm1, %%mm1 \n\t"\
\
            :[tmp0]"=m"(tmp0),\
             [tmp1]"=m"(tmp1),\
             [tmp2]"=m"(tmp2),\
             [tmp3]"=m"(tmp3)\
            :[prev] "r"(prev),\
             [cur]  "r"(cur),\
             [next] "r"(next),\
             [prefs]"r"((long)refs),\
             [mrefs]"r"((long)-refs),\
             [pw1]  "m"(pw_1),\
             [pb1]  "m"(pb_1),\
             [mode] "g"(mode)\
        );\
        asm volatile("movd %%mm1, %0" :"=m"(*dst));\
        dst += 4;\
        prev+= 4;\
        cur += 4;\
        next+= 4;\
    }

    if (parity)
    {
#define prev2 "prev"
#define next2 "cur"
        FILTER
#undef prev2
#undef next2
    }
    else
    {
#define prev2 "cur"
#define next2 "next"
        FILTER
#undef prev2
#undef next2
    }
}
#undef LOAD4
#undef PABS
#undef CHECK
#undef CHECK1
#undef CHECK2
#undef FILTER

#endif /* defined(MMX) && defined(NAMED_ASM_ARGS) */

static void filter_line_c(struct ThisFilter *p, uint8_t *dst, uint8_t *prev, uint8_t *cur, uint8_t *next, int w, int refs, int parity)
{
    int x;
    uint8_t *prev2= parity ? prev : cur ;
    uint8_t *next2= parity ? cur  : next;
    for (x=0; x<w; x++)
    {
        int c= cur[-refs];
        int d= (prev2[0] + next2[0])>>1;
        int e= cur[+refs];
        int temporal_diff0= ABS(prev2[0] - next2[0]);
        int temporal_diff1=( ABS(prev[-refs] - c) + ABS(prev[+refs] - e) )>>1;
        int temporal_diff2=( ABS(next[-refs] - c) + ABS(next[+refs] - e) )>>1;
        int diff= MAX3(temporal_diff0>>1, temporal_diff1, temporal_diff2);
        int spatial_pred= (c+e)>>1;
        int spatial_score= ABS(cur[-refs-1] - cur[+refs-1]) + ABS(c-e)
                         + ABS(cur[-refs+1] - cur[+refs+1]) - 1;

#define CHECK(j)\
    {   int score= ABS(cur[-refs-1+j] - cur[+refs-1-j])\
                 + ABS(cur[-refs  +j] - cur[+refs  -j])\
                 + ABS(cur[-refs+1+j] - cur[+refs+1-j]);\
        if (score < spatial_score){\
            spatial_score= score;\
            spatial_pred= (cur[-refs  +j] + cur[+refs  -j])>>1;\

        CHECK(-1) CHECK(-2) }} }}
        CHECK( 1) CHECK( 2) }} }}

        //if (p->mode<2)  #always < 2 for myhttv
        {
            int b= (prev2[-2*refs] + next2[-2*refs])>>1;
            int f= (prev2[+2*refs] + next2[+2*refs])>>1;
#if 0
            int a= cur[-3*refs];
            int g= cur[+3*refs];
            int max= MAX3(d-e, d-c, MIN3(MAX(b-c,f-e),MAX(b-c,b-a),MAX(f-g,f-e)) );
            int min= MIN3(d-e, d-c, MAX3(MIN(b-c,f-e),MIN(b-c,b-a),MIN(f-g,f-e)) );
#else
            int max= MAX3(d-e, d-c, MIN(b-c, f-e));
            int min= MIN3(d-e, d-c, MAX(b-c, f-e));
#endif

            diff= MAX3(diff, min, -max);
        }

        if (spatial_pred > d + diff)
           spatial_pred = d + diff;
        else if (spatial_pred < d - diff)
           spatial_pred = d - diff;

        dst[0] = spatial_pred;

        dst++;
        cur++;
        prev++;
        next++;
        prev2++;
        next2++;
    }
}

static void filter_func(struct ThisFilter *p, uint8_t *dst, int dst_offsets[3], int dst_stride[3], int width, int height, int parity, int tff)
{
    int y, i;

    uint8_t nr_p, nr_c, nr_n;

    //check if we already got this frames
    nr_n = 2;//always there after store_ref
    nr_c = p->got_frames[1]?1:nr_n;
    nr_p = p->got_frames[0]?0:nr_c;

    for (i=0; i<3; i++)
    {
        int is_chroma= !!i;
        int w= width >>is_chroma;
        int h= height>>is_chroma;
        int refs= p->stride[i];

        for (y=0; y<h; y++)
        {
            if ((y ^ parity) & 1)
            {
                uint8_t *prev= &p->ref[nr_p][i][y*refs];
                uint8_t *cur = &p->ref[nr_c][i][y*refs];
                uint8_t *next= &p->ref[nr_n][i][y*refs];
                uint8_t *dst2= dst + dst_offsets[i] + y*dst_stride[i];
                p->filter_line(p, dst2, prev, cur, next, w, refs, parity ^ tff);
            }
            else
            {
                fast_memcpy(dst + dst_offsets[i] + y*dst_stride[i], &p->ref[nr_c][i][y*refs], w);
            }
        }
    }
#ifdef MMX
    emms();
#endif
}


static int YadifDeint (VideoFilter * f, VideoFrame * frame)
{
    ThisFilter *filter = (ThisFilter *) f;
    TF_VARS;

    int second_field = 0;
    AllocFilter(filter, frame->width, frame->height);

    //printf("FNr=%lld T=%lld IL=%d TF=%d RP=%d G[2]=%d G[1]=%d G[0]=%d\n", frame->frameNumber, frame->timecode, frame->interlaced_frame, frame->top_field_first, frame->repeat_pict, filter->got_frames[2], filter->got_frames[1], filter->got_frames[0]);

    if (filter->last_framenr != frame->frameNumber)
    {
        if (filter->last_framenr != (frame->frameNumber - 1))
        {
            //printf("yadifdeint: Framenumbers not consecutive %lld -> %lld\n", filter->last_framenr, frame->frameNumber);
            memset(filter->got_frames, 0, sizeof(filter->got_frames));
        }
        store_ref(filter, frame->buf,  frame->offsets, frame->pitches, frame->width, frame->height);
        second_field = 0;
    }
    else
    {
        second_field = 1;
    }

    /* filter all frames, even if frame->interlaced_frame is not set */
    filter_func(
        filter, frame->buf, frame->offsets, frame->pitches,
        frame->width, frame->height, second_field, frame->top_field_first);

    filter->last_framenr = frame->frameNumber;

    return 0;
}


void CleanupYadifDeintFilter (VideoFilter * filter)
{
    int i;
    ThisFilter* f = (ThisFilter*)filter;
    for (i=0; i<3*3; i++)
    {
        uint8_t **p= &f->ref[i%3][i/3];
        if (*p) free(*p - 3*f->stride[i/3]);
        *p= NULL;
    }
}

VideoFilter * YadifDeintFilter (VideoFrameType inpixfmt, VideoFrameType outpixfmt,
    int *width, int *height, char *options)
{
    ThisFilter *filter;
    (void) height;
    (void) options;

    fprintf(stderr, "Initialize Yadif Deinterlacer. In-Pixformat = %d Out-Pixformat=%d\n", inpixfmt, outpixfmt);
    filter = (ThisFilter *) malloc (sizeof(ThisFilter));
    if (filter == NULL)
    {
        fprintf (stderr, "YadifDeint: failed to allocate memory for filter.\n");
        return NULL;
    }

    filter->width = 0;
    filter->height = 0;
    filter->mode = 1; // currently not important for myth (always < 2)
    memset(filter->ref, 0, sizeof(filter->ref));

    AllocFilter(filter, *width, *height);

#ifdef MMX
    filter->mm_flags = mm_support();
    TF_INIT(filter);
#else
    filter->mm_flags = 0;
#endif

    filter->filter_line = filter_line_c;
#ifdef MMX
    if (filter->mm_flags & MM_MMX) 
    {
        filter->filter_line = filter_line_mmx2;
    }	
    
    if (filter->mm_flags & MM_SSE2)
		  fast_memcpy=fast_memcpy_SSE;
	  else if (filter->mm_flags & MM_MMXEXT)
		  fast_memcpy=fast_memcpy_MMX2;
	  else if (filter->mm_flags & MM_3DNOW)
  		fast_memcpy=fast_memcpy_3DNow;
    else if (filter->mm_flags & MM_MMX)
		  fast_memcpy=fast_memcpy_MMX;
  	else
#endif
		  fast_memcpy=memcpy; // prior to mmx we use the standard memcpy

    //hard coded for benchmarking
    //fast_memcpy = fast_memcpy_SSE;

    filter->vf.filter = &YadifDeint;
    filter->vf.cleanup = &CleanupYadifDeintFilter;
    return (VideoFilter *) filter;
}


static FmtConv FmtList[] =
{
    { FMT_YV12, FMT_YV12 } ,
    FMT_NULL
};

FilterInfo filter_table[] =
{
    {
symbol:     "YadifDeintFilter",
            name:       "yadifdeint",
            descript:   "combines data from several fields to deinterlace with less motion blur",
            formats:    FmtList,
            libname:    NULL
    },
    {
symbol:     "YadifDeintFilter",
            name:       "yadifdoubleprocessdeint",
            descript:   "combines data from several fields to deinterlace with less motion blur",
            formats:    FmtList,
            libname:    NULL
    },FILT_NULL
};

/* vim: set expandtab tabstop=4 shiftwidth=4: */
