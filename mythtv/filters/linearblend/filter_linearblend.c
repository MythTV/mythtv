// a linear blending deinterlacer yoinked from the mplayer sources.

#include <stdlib.h>
#include <stdio.h>

#include "mythconfig.h"
#if HAVE_STDINT_H
#include <stdint.h>
#endif

#if HAVE_MMX || HAVE_AMD3DNOW
#include "ffmpeg-mmx.h"
#endif

#include "../mm_arch.h"
#if HAVE_ALTIVEC_H
    #include <altivec.h>
#endif

#define PAVGB(a,b)   "pavgb " #a ", " #b " \n\t"
#define PAVGUSB(a,b) "pavgusb " #a ", " #b " \n\t"

#include "filter.h"
#include "mythframe.h"

typedef struct LBFilter
{
    VideoFilter vf;

    /* functions and variables below here considered "private" */
    int mm_flags;
    void (*subfilter)(unsigned char *, int);
    TF_STRUCT;
} LBFilter;

void linearBlend(unsigned char *src, int stride);
void linearBlendMMX(unsigned char *src, int stride);
void linearBlend3DNow(unsigned char *src, int stride);
int linearBlendFilterAltivec(VideoFilter *f, VideoFrame *frame, int field);

#if HAVE_ALTIVEC
inline void linearBlendAltivec(unsigned char *src, int stride);
#endif

#ifdef MMX

void linearBlendMMX(unsigned char *src, int stride)
{
//  src += 4 * stride;
    __asm__ volatile(
       "lea (%0, %1), %%"REG_a"                        \n\t"
       "lea (%%"REG_a", %1, 4), %%"REG_d"              \n\t"

       "movq (%0), %%mm0                               \n\t" // L0
       "movq (%%"REG_a", %1), %%mm1                    \n\t" // L2
       PAVGB(%%mm1, %%mm0)                                   // L0+L2
       "movq (%%"REG_a"), %%mm2                            \n\t" // L1
       PAVGB(%%mm2, %%mm0)
       "movq %%mm0, (%0)                               \n\t"
       "movq (%%"REG_a", %1, 2), %%mm0                     \n\t" // L3
       PAVGB(%%mm0, %%mm2)                                   // L1+L3
       PAVGB(%%mm1, %%mm2)                                   // 2L2 + L1 + L3
       "movq %%mm2, (%%"REG_a")                            \n\t"
       "movq (%0, %1, 4), %%mm2                        \n\t" // L4
       PAVGB(%%mm2, %%mm1)                                   // L2+L4
       PAVGB(%%mm0, %%mm1)                                   // 2L3 + L2 + L4
       "movq %%mm1, (%%"REG_a", %1)                        \n\t"
       "movq (%%"REG_d"), %%mm1                            \n\t" // L5
       PAVGB(%%mm1, %%mm0)                                   // L3+L5
       PAVGB(%%mm2, %%mm0)                                   // 2L4 + L3 + L5
       "movq %%mm0, (%%"REG_a", %1, 2)                     \n\t"
       "movq (%%"REG_d", %1), %%mm0                        \n\t" // L6
       PAVGB(%%mm0, %%mm2)                                   // L4+L6
       PAVGB(%%mm1, %%mm2)                                   // 2L5 + L4 + L6
       "movq %%mm2, (%0, %1, 4)                        \n\t"
       "movq (%%"REG_d", %1, 2), %%mm2                     \n\t" // L7
       PAVGB(%%mm2, %%mm1)                                   // L5+L7
       PAVGB(%%mm0, %%mm1)                                   // 2L6 + L5 + L7
       "movq %%mm1, (%%"REG_d")                            \n\t"
       "movq (%0, %1, 8), %%mm1                        \n\t" // L8
       PAVGB(%%mm1, %%mm0)                                   // L6+L8
       PAVGB(%%mm2, %%mm0)                                   // 2L7 + L6 + L8
       "movq %%mm0, (%%"REG_d", %1)                        \n\t"
       "movq (%%"REG_d", %1, 4), %%mm0                     \n\t" // L9
       PAVGB(%%mm0, %%mm2)                                   // L7+L9
       PAVGB(%%mm1, %%mm2)                                   // 2L8 + L7 + L9
       "movq %%mm2, (%%"REG_d", %1, 2)                     \n\t"

       : : "r" (src), "r" ((long)stride)
       : "%"REG_a, "%"REG_d
    );
}

void linearBlend3DNow(unsigned char *src, int stride)
{
//  src += 4 * stride;
    __asm__ volatile(
       "lea (%0, %1), %%"REG_a"                           \n\t"
       "lea (%%"REG_a", %1, 4), %%"REG_d"                     \n\t"

       "movq (%0), %%mm0                               \n\t" // L0
       "movq (%%"REG_a", %1), %%mm1                        \n\t" // L2
       PAVGUSB(%%mm1, %%mm0)                                 // L0+L2
       "movq (%%"REG_a"), %%mm2                            \n\t" // L1
       PAVGUSB(%%mm2, %%mm0)
       "movq %%mm0, (%0)                               \n\t"
       "movq (%%"REG_a", %1, 2), %%mm0                     \n\t" // L3
       PAVGUSB(%%mm0, %%mm2)                                 // L1+L3
       PAVGUSB(%%mm1, %%mm2)                                 // 2L2 + L1 + L3
       "movq %%mm2, (%%"REG_a")                            \n\t"
       "movq (%0, %1, 4), %%mm2                        \n\t" // L4
       PAVGUSB(%%mm2, %%mm1)                                 // L2+L4
       PAVGUSB(%%mm0, %%mm1)                                 // 2L3 + L2 + L4
       "movq %%mm1, (%%"REG_a", %1)                        \n\t"
       "movq (%%"REG_d"), %%mm1                            \n\t" // L5
       PAVGUSB(%%mm1, %%mm0)                                 // L3+L5
       PAVGUSB(%%mm2, %%mm0)                                 // 2L4 + L3 + L5
       "movq %%mm0, (%%"REG_a", %1, 2)                     \n\t"
       "movq (%%"REG_d", %1), %%mm0                        \n\t" // L6
       PAVGUSB(%%mm0, %%mm2)                                 // L4+L6
       PAVGUSB(%%mm1, %%mm2)                                 // 2L5 + L4 + L6
       "movq %%mm2, (%0, %1, 4)                        \n\t"
       "movq (%%"REG_d", %1, 2), %%mm2                     \n\t" // L7
       PAVGUSB(%%mm2, %%mm1)                                 // L5+L7
       PAVGUSB(%%mm0, %%mm1)                                 // 2L6 + L5 + L7
       "movq %%mm1, (%%"REG_d")                            \n\t"
       "movq (%0, %1, 8), %%mm1                        \n\t" // L8
       PAVGUSB(%%mm1, %%mm0)                                 // L6+L8
       PAVGUSB(%%mm2, %%mm0)                                 // 2L7 + L6 + L8
       "movq %%mm0, (%%"REG_d", %1)                        \n\t"
       "movq (%%"REG_d", %1, 4), %%mm0                     \n\t" // L9
       PAVGUSB(%%mm0, %%mm2)                                 // L7+L9
       PAVGUSB(%%mm1, %%mm2)                                 // 2L8 + L7 + L9
       "movq %%mm2, (%%"REG_d", %1, 2)                     \n\t"

       : : "r" (src), "r" ((long)stride)
       : "%"REG_a, "%"REG_d
    );
}

#endif

#if HAVE_ALTIVEC

inline void linearBlendAltivec(unsigned char *src, int stride)
{
    vector unsigned char a, b, c;
    int i;
    
    b = vec_ld(0, src);
    c = vec_ld(stride, src);
    
    for (i = 2; i < 10; i++)
    {
        a = b;
        b = c;
        c = vec_ld(stride * i, src);
        vec_st(vec_avg(vec_avg(a, c), b), stride * (i - 2), src);
    }
}

int linearBlendFilterAltivec(VideoFilter *f, VideoFrame *frame, int field)
{
    (void)field;
    (void)f;
    int height = frame->height;
    unsigned char *yptr = frame->buf + frame->offsets[0];
    int stride = frame->pitches[0];
    int ymax = height - 8;
    int x,y;
    unsigned char *src = 0;
    unsigned char *uoff = frame->buf + frame->offsets[1];
    unsigned char *voff = frame->buf + frame->offsets[2];
    TF_VARS;

    TF_START;
 
    if ((stride & 0xf) || ((unsigned int)yptr & 0xf))
    {
        for (y = 0; y < ymax; y += 8)
        {  
            for (x = 0; x < stride; x += 8)
            {
                src = yptr + x + y * stride;  
                linearBlend(src, stride);  
            }
        }
    }
    else
    {
        src = yptr;
        for (y = 0; y < ymax; y += 8)
        {  
            for (x = 0; x < stride; x += 16)
            {
                linearBlendAltivec(src, stride);
                src += 16;
            }
            src += stride * 7;
        }
    }
 
    stride = frame->pitches[1];
    ymax = height / 2 - 8;
  
    if ((stride & 0xf) || ((unsigned int)uoff & 0xf))
    {
        for (y = 0; y < ymax; y += 8)
        {
            for (x = 0; x < stride; x += 8)
            {
                src = uoff + x + y * stride;
                linearBlend(src, stride);
           
                src = voff + x + y * stride;
                linearBlend(src, stride);
            }
        }
    }
    else
    {
        for (y = 0; y < ymax; y += 8)
        {
            for (x = 0; x < stride; x += 16)
            {
                linearBlendAltivec(src, stride);
                uoff += 16;
           
                linearBlendAltivec(src, stride);
                voff += 16;
            }
            uoff += stride * 7;
            voff += stride * 7;
        }
    }

    TF_END(vf, "LinearBlendAltivec: ");
    return 0;
}

#endif /* HAVE_ALTIVEC */

void linearBlend(unsigned char *src, int stride)
{
    int a, b, c, x;

    for (x = 0; x < 2; x++)
    {
        a= *(uint32_t*)&src[stride*0];
        b= *(uint32_t*)&src[stride*1];
        c= *(uint32_t*)&src[stride*2];
        a= (a&c) + (((a^c)&0xFEFEFEFEUL)>>1);
        *(uint32_t*)&src[stride*0]= (a|b) - (((a^b)&0xFEFEFEFEUL)>>1);

        a= *(uint32_t*)&src[stride*3];
        b= (a&b) + (((a^b)&0xFEFEFEFEUL)>>1);
        *(uint32_t*)&src[stride*1]= (c|b) - (((c^b)&0xFEFEFEFEUL)>>1);

        b= *(uint32_t*)&src[stride*4];
        c= (b&c) + (((b^c)&0xFEFEFEFEUL)>>1);
        *(uint32_t*)&src[stride*2]= (c|a) - (((c^a)&0xFEFEFEFEUL)>>1);

        c= *(uint32_t*)&src[stride*5];
        a= (a&c) + (((a^c)&0xFEFEFEFEUL)>>1);
        *(uint32_t*)&src[stride*3]= (a|b) - (((a^b)&0xFEFEFEFEUL)>>1);

        a= *(uint32_t*)&src[stride*6];
        b= (a&b) + (((a^b)&0xFEFEFEFEUL)>>1);
        *(uint32_t*)&src[stride*4]= (c|b) - (((c^b)&0xFEFEFEFEUL)>>1);

        b= *(uint32_t*)&src[stride*7];
        c= (b&c) + (((b^c)&0xFEFEFEFEUL)>>1);
        *(uint32_t*)&src[stride*5]= (c|a) - (((c^a)&0xFEFEFEFEUL)>>1);

        c= *(uint32_t*)&src[stride*8];
        a= (a&c) + (((a^c)&0xFEFEFEFEUL)>>1);
        *(uint32_t*)&src[stride*6]= (a|b) - (((a^b)&0xFEFEFEFEUL)>>1);

        a= *(uint32_t*)&src[stride*9];
        b= (a&b) + (((a^b)&0xFEFEFEFEUL)>>1);
        *(uint32_t*)&src[stride*7]= (c|b) - (((c^b)&0xFEFEFEFEUL)>>1);

        src += 4;
    }
}

static int linearBlendFilter(VideoFilter *f, VideoFrame *frame, int  field)
{
    (void)field;
    int height = frame->height;
    unsigned char *yptr = frame->buf + frame->offsets[0];
    int stride = frame->pitches[0];
    int ymax = height - 8;
    int x,y;
    unsigned char *src;
    unsigned char *uoff = frame->buf + frame->offsets[1];
    unsigned char *voff = frame->buf + frame->offsets[2];
    LBFilter *vf = (LBFilter *)f;
    TF_VARS;

    TF_START;

    for (y = 0; y < ymax; y+=8)
    {  
        for (x = 0; x < stride; x+=8)
        {
            src = yptr + x + y * stride;
            (vf->subfilter)(src, stride);  
        }
    }
 
    stride = frame->pitches[1];
    ymax = height / 2 - 8;
  
    for (y = 0; y < ymax; y += 8)
    {
        for (x = 0; x < stride; x += 8)
        {
            src = uoff + x + y * stride;
            (vf->subfilter)(src, stride);
       
            src = voff + x + y * stride;
            (vf->subfilter)(src, stride);
        }
    }

#if HAVE_MMX || HAVE_AMD3DNOW
    if ((vf->mm_flags & AV_CPU_FLAG_MMX2) || (vf->mm_flags & AV_CPU_FLAG_3DNOW))
        emms();
#endif

    TF_END(vf, "LinearBlend: ");
    return 0;
}

static VideoFilter *new_filter(VideoFrameType inpixfmt,
                               VideoFrameType outpixfmt,
                               int *width, int *height, char *options,
                               int threads)
{
    LBFilter *filter;
    (void)width;
    (void)height;
    (void)options;
    (void)threads;
    if (inpixfmt != FMT_YV12 || outpixfmt != FMT_YV12)
        return NULL;

    filter = malloc(sizeof(LBFilter));

    if (filter == NULL)
    {
        fprintf(stderr,"Couldn't allocate memory for filter\n");
        return NULL;
    }

    filter->vf.filter = &linearBlendFilter;
    filter->subfilter = &linearBlend;    /* Default, non accellerated */
    filter->mm_flags = av_get_cpu_flags();
    if (HAVE_MMX && filter->mm_flags & AV_CPU_FLAG_MMX2)
        filter->subfilter = &linearBlendMMX;
    else if (HAVE_AMD3DNOW && filter->mm_flags & AV_CPU_FLAG_3DNOW)
        filter->subfilter = &linearBlend3DNow;
    else if (HAVE_ALTIVEC && filter->mm_flags & AV_CPU_FLAG_ALTIVEC)
        filter->vf.filter = &linearBlendFilterAltivec;

    filter->vf.cleanup = NULL;
    TF_INIT(filter);
    return (VideoFilter *)filter;
}

static FmtConv FmtList[] = 
{
    { FMT_YV12, FMT_YV12 },
    FMT_NULL
};

const FilterInfo filter_table[] =
{
    {
        .filter_init= &new_filter,
        .name=       (char*)"linearblend",
        .descript=   (char*)"fast blending deinterlace filter",
        .formats=    FmtList,
        .libname=    NULL
    },
    FILT_NULL
};
