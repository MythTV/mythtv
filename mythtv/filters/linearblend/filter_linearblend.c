// a linear blending deinterlacer yoinked from the mplayer sources.

#include <stdlib.h>
#include <stdio.h>

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include "config.h"
#include "dsputil.h"
#include "../mm_arch.h"

#define PAVGB(a,b)   "pavgb " #a ", " #b " \n\t"
#define PAVGUSB(a,b) "pavgusb " #a ", " #b " \n\t"

#include "filter.h"
#include "frame.h"

typedef struct LBFilter
{
    int (*filter)(VideoFilter *, VideoFrame *);
    void (*cleanup)(VideoFilter *);

    void *handle; // Library handle;
    VideoFrameType inpixfmt;
    VideoFrameType outpixfmt;
    char *opts;
    FilterInfo *info;

    /* functions and variables below here considered "private" */
    int mm_flags;
    void (*subfilter)(unsigned char *, int);
    TF_STRUCT;
} LBFilter;

#ifdef MMX

void linearBlendMMX(unsigned char *src, int stride)
{
//  src += 4 * stride;
    asm volatile(
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
    asm volatile(
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

#ifdef HAVE_ALTIVEC

// we fall back to the default routines in some situations
void linearBlend(unsigned char *src, int stride);

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

int linearBlendFilterAltivec(VideoFilter *f, VideoFrame *frame)
{
    (void)f;
    int width = frame->width;
    int height = frame->height;
    unsigned char *yuvptr = frame->buf;
    int stride = width;
    int ymax = height - 8;
    int x,y;
    unsigned char *src = 0;
    unsigned char *uoff;
    unsigned char *voff;
    TF_VARS;

    TF_START;
 
    if ((stride % 16) || ((unsigned int)yuvptr % 16))
    {
        for (y = 0; y < ymax; y += 8)
        {  
            for (x = 0; x < stride; x+= 8)
            {
                src = yuvptr + x + y * stride;  
                linearBlend(src, stride);  
            }
        }
    }
    else
    {
        src = yuvptr;
        for (y = 0; y < ymax; y += 8)
        {  
            for (x = 0; x < stride; x+= 16)
            {
                linearBlendAltivec(src, stride);
                src += 16;
            }
            src += stride * 7;
        }
    }
 
    stride = width / 2;
    ymax = height / 2 - 8;
  
    uoff = yuvptr + width * height;
    voff = yuvptr + width * height * 5 / 4;
 
    if ((stride % 16) || ((unsigned int)uoff % 16))
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

int linearBlendFilter(VideoFilter *f, VideoFrame *frame)
{
    int width = frame->width;
    int height = frame->height;
    unsigned char *yuvptr = frame->buf;
    int stride = width;
    int ymax = height - 8;
    int x,y;
    unsigned char *src;
    unsigned char *uoff;
    unsigned char *voff;
    LBFilter *vf = (LBFilter *)f;
    TF_VARS;

    TF_START;

    for (y = 0; y < ymax; y+=8)
    {  
        for (x = 0; x < stride; x+=8)
        {
            src = yuvptr + x + y * stride;  
            (vf->subfilter)(src, stride);  
        }
    }
 
    stride = width / 2;
    ymax = height / 2 - 8;
  
    uoff = yuvptr + width * height;
    voff = yuvptr + width * height * 5 / 4;
 
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

#ifdef MMX
    if ((vf->mm_flags & MM_MMXEXT) || (vf->mm_flags & MM_3DNOW))
        emms();
#endif

    TF_END(vf, "LinearBlend: ");
    return 0;
}

VideoFilter *new_filter(VideoFrameType inpixfmt, VideoFrameType outpixfmt, 
                        int *width, int *height, char *options)
{
    LBFilter *filter;
    (void)width;
    (void)height;
    (void)options;

    if (inpixfmt != FMT_YV12 || outpixfmt != FMT_YV12)
        return NULL;

    filter = malloc(sizeof(LBFilter));

    if (filter == NULL)
    {
        fprintf(stderr,"Couldn't allocate memory for filter\n");
        return NULL;
    }

    filter->filter = &linearBlendFilter;
    filter->subfilter = &linearBlend;    /* Default, non accellerated */
    filter->mm_flags = mm_support();
#ifdef MMX
    if (filter->mm_flags & MM_MMXEXT)
        filter->subfilter = &linearBlendMMX;
    else if (filter->mm_flags & MM_3DNOW)
        filter->subfilter = &linearBlend3DNow;
#endif
#ifdef HAVE_ALTIVEC
    if (filter->mm_flags & MM_ALTIVEC)
        filter->filter = &linearBlendFilterAltivec;
#endif

    filter->cleanup = NULL;
    TF_INIT(filter);
    return (VideoFilter *)filter;
}

static FmtConv FmtList[] = 
{
    { FMT_YV12, FMT_YV12 },
    FMT_NULL
};

FilterInfo filter_table[] = 
{
    {
        symbol:     "new_filter",
        name:       "linearblend",
        descript:   "fast blending deinterlace filter",
        formats:    FmtList,
        libname:    NULL
    },
    FILT_NULL
};
