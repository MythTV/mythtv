// a linear blending deinterlacer yoinked from the mplayer sources.

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "config.h"

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

#define MM_MMX     0x0001 /* standard MMX */
#define MM_3DNOW   0x0004 /* AMD 3DNOW */
#define MM_MMXEXT  0x0002 /* SSE integer functions or AMD MMX ext */
#define MM_SSE     0x0008 /* SSE functions */
#define MM_SSE2    0x0010 /* PIV SSE2 functions */
#define MM_ALTIVEC 0x0020 /* Altivec */

#ifdef i386

#define cpuid(index,eax,ebx,ecx,edx)\
    __asm __volatile\
        ("movl %%ebx, %%esi\n\t"\
         "cpuid\n\t"\
         "xchgl %%ebx, %%esi"\
         : "=a" (eax), "=S" (ebx),\
           "=c" (ecx), "=d" (edx)\
         : "0" (index));

#define        emms()                  __asm__ __volatile__ ("emms")

/* Function to test if multimedia instructions are supported...  */
int mm_support(void)
{
    int rval;
    int eax, ebx, ecx, edx;

    __asm__ __volatile__ (
                          /* See if CPUID instruction is supported ... */
                          /* ... Get copies of EFLAGS into eax and ecx */
                          "pushf\n\t"
                          "popl %0\n\t"
                          "movl %0, %1\n\t"

                          /* ... Toggle the ID bit in one copy and store */
                          /*     to the EFLAGS reg */
                          "xorl $0x200000, %0\n\t"
                          "push %0\n\t"
                          "popf\n\t"

                          /* ... Get the (hopefully modified) EFLAGS */
                          "pushf\n\t"
                          "popl %0\n\t"
                          : "=a" (eax), "=c" (ecx)
                          :
                          : "cc"
                          );

    if (eax == ecx)
        return 0; /* CPUID not supported */

    cpuid(0, eax, ebx, ecx, edx);

    if (ebx == 0x756e6547 &&
        edx == 0x49656e69 &&
        ecx == 0x6c65746e) {

        /* intel */
    inteltest:
        cpuid(1, eax, ebx, ecx, edx);
        if ((edx & 0x00800000) == 0)
            return 0;
        rval = MM_MMX;
        if (edx & 0x02000000)
            rval |= MM_MMXEXT | MM_SSE;
        if (edx & 0x04000000)
            rval |= MM_SSE2;
        return rval;
    } else if (ebx == 0x68747541 &&
               edx == 0x69746e65 &&
               ecx == 0x444d4163) {
        /* AMD */
        cpuid(0x80000000, eax, ebx, ecx, edx);
        if ((unsigned)eax < 0x80000001)
            goto inteltest;
        cpuid(0x80000001, eax, ebx, ecx, edx);
        if ((edx & 0x00800000) == 0)
            return 0;
        rval = MM_MMX;
        if (edx & 0x80000000)
            rval |= MM_3DNOW;
        if (edx & 0x00400000)
            rval |= MM_MMXEXT;
        return rval;
    } else if (ebx == 0x746e6543 &&
               edx == 0x48727561 &&
               ecx == 0x736c7561) {  /*  "CentaurHauls" */
        /* VIA C3 */
        cpuid(0x80000000, eax, ebx, ecx, edx);
        if ((unsigned)eax < 0x80000001)
            goto inteltest;
        cpuid(0x80000001, eax, ebx, ecx, edx);
        rval = 0;
        if( edx & ( 1 << 31) )
          rval |= MM_3DNOW;
        if( edx & ( 1 << 23) )
          rval |= MM_MMX;
        if( edx & ( 1 << 24) )
          rval |= MM_MMXEXT;
        if (rval==0)
            goto inteltest;
        return rval;
    } else if (ebx == 0x69727943 &&
               edx == 0x736e4978 &&
               ecx == 0x64616574) {
        /* Cyrix Section */
        /* See if extended CPUID level 80000001 is supported */
        /* The value of CPUID/80000001 for the 6x86MX is undefined
           according to the Cyrix CPU Detection Guide (Preliminary
           Rev. 1.01 table 1), so we'll check the value of eax for
           CPUID/0 to see if standard CPUID level 2 is supported.
           According to the table, the only CPU which supports level
           2 is also the only one which supports extended CPUID levels.
        */
        if (eax != 2)
            goto inteltest;
        cpuid(0x80000001, eax, ebx, ecx, edx);
        if ((eax & 0x00800000) == 0)
            return 0;
        rval = MM_MMX;
        if (eax & 0x01000000)
            rval |= MM_MMXEXT;
        return rval;
    } else if (ebx == 0x756e6547 &&
               edx == 0x54656e69 &&
               ecx == 0x3638784d) {
        /* Tranmeta Crusoe */
        cpuid(0x80000000, eax, ebx, ecx, edx);
        if ((unsigned)eax < 0x80000001)
            return 0;
        cpuid(0x80000001, eax, ebx, ecx, edx);
        if ((edx & 0x00800000) == 0)
            return 0;
        return MM_MMX;
    } else {
        return 0;
    }
}

void linearBlendMMX(unsigned char *src, int stride)
{
//  src += 4 * stride;
    asm volatile(
       "leal (%0, %1), %%eax                           \n\t"
       "leal (%%eax, %1, 4), %%edx                     \n\t"

       "movq (%0), %%mm0                               \n\t" // L0
       "movq (%%eax, %1), %%mm1                        \n\t" // L2
       PAVGB(%%mm1, %%mm0)                                   // L0+L2
       "movq (%%eax), %%mm2                            \n\t" // L1
       PAVGB(%%mm2, %%mm0)
       "movq %%mm0, (%0)                               \n\t"
       "movq (%%eax, %1, 2), %%mm0                     \n\t" // L3
       PAVGB(%%mm0, %%mm2)                                   // L1+L3
       PAVGB(%%mm1, %%mm2)                                   // 2L2 + L1 + L3
       "movq %%mm2, (%%eax)                            \n\t"
       "movq (%0, %1, 4), %%mm2                        \n\t" // L4
       PAVGB(%%mm2, %%mm1)                                   // L2+L4
       PAVGB(%%mm0, %%mm1)                                   // 2L3 + L2 + L4
       "movq %%mm1, (%%eax, %1)                        \n\t"
       "movq (%%edx), %%mm1                            \n\t" // L5
       PAVGB(%%mm1, %%mm0)                                   // L3+L5
       PAVGB(%%mm2, %%mm0)                                   // 2L4 + L3 + L5
       "movq %%mm0, (%%eax, %1, 2)                     \n\t"
       "movq (%%edx, %1), %%mm0                        \n\t" // L6
       PAVGB(%%mm0, %%mm2)                                   // L4+L6
       PAVGB(%%mm1, %%mm2)                                   // 2L5 + L4 + L6
       "movq %%mm2, (%0, %1, 4)                        \n\t"
       "movq (%%edx, %1, 2), %%mm2                     \n\t" // L7
       PAVGB(%%mm2, %%mm1)                                   // L5+L7
       PAVGB(%%mm0, %%mm1)                                   // 2L6 + L5 + L7
       "movq %%mm1, (%%edx)                            \n\t"
       "movq (%0, %1, 8), %%mm1                        \n\t" // L8
       PAVGB(%%mm1, %%mm0)                                   // L6+L8
       PAVGB(%%mm2, %%mm0)                                   // 2L7 + L6 + L8
       "movq %%mm0, (%%edx, %1)                        \n\t"
       "movq (%%edx, %1, 4), %%mm0                     \n\t" // L9
       PAVGB(%%mm0, %%mm2)                                   // L7+L9
       PAVGB(%%mm1, %%mm2)                                   // 2L8 + L7 + L9
       "movq %%mm2, (%%edx, %1, 2)                     \n\t"

       : : "r" (src), "r" (stride)
       : "%eax", "%edx"
    );
}

void linearBlend3DNow(unsigned char *src, int stride)
{
//  src += 4 * stride;
    asm volatile(
       "leal (%0, %1), %%eax                           \n\t"
       "leal (%%eax, %1, 4), %%edx                     \n\t"

       "movq (%0), %%mm0                               \n\t" // L0
       "movq (%%eax, %1), %%mm1                        \n\t" // L2
       PAVGUSB(%%mm1, %%mm0)                                 // L0+L2
       "movq (%%eax), %%mm2                            \n\t" // L1
       PAVGUSB(%%mm2, %%mm0)
       "movq %%mm0, (%0)                               \n\t"
       "movq (%%eax, %1, 2), %%mm0                     \n\t" // L3
       PAVGUSB(%%mm0, %%mm2)                                 // L1+L3
       PAVGUSB(%%mm1, %%mm2)                                 // 2L2 + L1 + L3
       "movq %%mm2, (%%eax)                            \n\t"
       "movq (%0, %1, 4), %%mm2                        \n\t" // L4
       PAVGUSB(%%mm2, %%mm1)                                 // L2+L4
       PAVGUSB(%%mm0, %%mm1)                                 // 2L3 + L2 + L4
       "movq %%mm1, (%%eax, %1)                        \n\t"
       "movq (%%edx), %%mm1                            \n\t" // L5
       PAVGUSB(%%mm1, %%mm0)                                 // L3+L5
       PAVGUSB(%%mm2, %%mm0)                                 // 2L4 + L3 + L5
       "movq %%mm0, (%%eax, %1, 2)                     \n\t"
       "movq (%%edx, %1), %%mm0                        \n\t" // L6
       PAVGUSB(%%mm0, %%mm2)                                 // L4+L6
       PAVGUSB(%%mm1, %%mm2)                                 // 2L5 + L4 + L6
       "movq %%mm2, (%0, %1, 4)                        \n\t"
       "movq (%%edx, %1, 2), %%mm2                     \n\t" // L7
       PAVGUSB(%%mm2, %%mm1)                                 // L5+L7
       PAVGUSB(%%mm0, %%mm1)                                 // 2L6 + L5 + L7
       "movq %%mm1, (%%edx)                            \n\t"
       "movq (%0, %1, 8), %%mm1                        \n\t" // L8
       PAVGUSB(%%mm1, %%mm0)                                 // L6+L8
       PAVGUSB(%%mm2, %%mm0)                                 // 2L7 + L6 + L8
       "movq %%mm0, (%%edx, %1)                        \n\t"
       "movq (%%edx, %1, 4), %%mm0                     \n\t" // L9
       PAVGUSB(%%mm0, %%mm2)                                 // L7+L9
       PAVGUSB(%%mm1, %%mm2)                                 // 2L8 + L7 + L9
       "movq %%mm2, (%%edx, %1, 2)                     \n\t"

       : : "r" (src), "r" (stride)
       : "%eax", "%edx"
    );
}

int linearBlendFilterAltivec(VideoFilter *f, VideoFrame *frame) {(void)f; (void)frame; return 0;};
#else // i386
#define emms()
void linearBlendMMX(unsigned char *src, int stride) {(void)src; (void)stride;};
void linearBlend3DNow(unsigned char *src, int stride) {(void)src;(void)stride;};

#ifdef HAVE_ALTIVEC

#include <Accelerate/Accelerate.h>

// we fall back to the default routines in some situations
void linearBlend(unsigned char *src, int stride);

int mm_support(void) { return MM_ALTIVEC; }

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
    unsigned char *src;
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

#else  // Altivec
int mm_support(void) { return 0; };
int linearBlendFilterAltivec(VideoFilter *f, VideoFrame *frame) {(void)f; (void)frame; return 0;};
#endif // Altivec
#endif // i386

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

    if ((vf->mm_flags & MM_MMXEXT) || (vf->mm_flags & MM_3DNOW))
        emms();

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
    filter->mm_flags = mm_support();
    if (filter->mm_flags & MM_MMXEXT)
        filter->subfilter = &linearBlendMMX;
    else if (filter->mm_flags & MM_3DNOW)
        filter->subfilter = &linearBlend3DNow;
    else if (filter->mm_flags && MM_ALTIVEC)
        filter->filter = &linearBlendFilterAltivec;
    else
        filter->subfilter = &linearBlend;

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
