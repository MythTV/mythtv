/* Rewrite of neuron2's KernelDeint filter for Avisynth */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "filter.h"
#include "frame.h"

#define ABS(A) ( (A) > 0 ? (A) : -(A) )
#define CLAMP(A,U,L) ((A)>(U)?(U):((A)<(L)?(L):(A)))

#ifdef TIME_FILTER
#include <sys/time.h>
#ifndef TIME_INTERVAL
#define TIME_INTERVAL 300
#endif /* undef TIME_INTERVAL */
#endif /* TIME_FILTER */

#include "mmx.h"

static const mmx_t m0 = { 0x0LL };
static const mmx_t m1 = { 0xFFFFFFFFFFFFFFFFLL };

#define cpuid(index,eax,ebx,ecx,edx)\
    __asm __volatile\
        ("movl %%ebx, %%esi\n\t"\
         "cpuid\n\t"\
         "xchgl %%ebx, %%esi"\
         : "=a" (eax), "=S" (ebx),\
           "=c" (ecx), "=d" (edx)\
         : "0" (index));

#define MM_MMX    0x0001        /* standard MMX */
#define MM_3DNOW  0x0004        /* AMD 3DNOW */
#define MM_MMXEXT 0x0002        /* SSE integer functions or AMD MMX ext */
#define MM_SSE    0x0008        /* SSE functions */
#define MM_SSE2   0x0010        /* PIV SSE2 functions */

/* Function to test if multimedia instructions are supported...  */
int
mm_support (void)
{
    int rval;
    int eax, ebx, ecx, edx;

    __asm__ __volatile__ (
                             /* See if CPUID instruction is supported ... */
                             /* ... Get copies of EFLAGS into eax and ecx */
                             "pushf\n\t" "popl %0\n\t" "movl %0, %1\n\t"
                             /* ... Toggle the ID bit in one copy and store */
                             /*     to the EFLAGS reg */
                             "xorl $0x200000, %0\n\t" "push %0\n\t" "popf\n\t"
                             /* ... Get the (hopefully modified) EFLAGS */
                             "pushf\n\t"
                             "popl %0\n\t":"=a" (eax), "=c" (ecx)::"cc");

    if (eax == ecx)
        return 0;               /* CPUID not supported */

    cpuid (0, eax, ebx, ecx, edx);

    if (ebx == 0x756e6547 && edx == 0x49656e69 && ecx == 0x6c65746e)
    {

        /* intel */
      inteltest:
        cpuid (1, eax, ebx, ecx, edx);
        if ((edx & 0x00800000) == 0)
            return 0;
        rval = MM_MMX;
        if (edx & 0x02000000)
            rval |= MM_MMXEXT | MM_SSE;
        if (edx & 0x04000000)
            rval |= MM_SSE2;
        return rval;
    }
    else if (ebx == 0x68747541 && edx == 0x69746e65 && ecx == 0x444d4163)
    {
        /* AMD */
        cpuid (0x80000000, eax, ebx, ecx, edx);
        if ((unsigned) eax < 0x80000001)
            goto inteltest;
        cpuid (0x80000001, eax, ebx, ecx, edx);
        if ((edx & 0x00800000) == 0)
            return 0;
        rval = MM_MMX;
        if (edx & 0x80000000)
            rval |= MM_3DNOW;
        if (edx & 0x00400000)
            rval |= MM_MMXEXT;
        return rval;
    }
    else if (ebx == 0x746e6543 && edx == 0x48727561 && ecx == 0x736c7561)
    {                           /*  "CentaurHauls" */
        /* VIA C3 */
        cpuid (0x80000000, eax, ebx, ecx, edx);
        if ((unsigned) eax < 0x80000001)
            goto inteltest;
        cpuid (0x80000001, eax, ebx, ecx, edx);
        rval = 0;
        if (edx & (1 << 31))
            rval |= MM_3DNOW;
        if (edx & (1 << 23))
            rval |= MM_MMX;
        if (edx & (1 << 24))
            rval |= MM_MMXEXT;
        return rval;
    }
    else if (ebx == 0x69727943 && edx == 0x736e4978 && ecx == 0x64616574)
    {
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
        cpuid (0x80000001, eax, ebx, ecx, edx);
        if ((eax & 0x00800000) == 0)
            return 0;
        rval = MM_MMX;
        if (eax & 0x01000000)
            rval |= MM_MMXEXT;
        return rval;
    }
    else
    {
        return 0;
    }
}

typedef struct ThisFilter
{
    VideoFilter vf;

    int width;
    int height;
    int uoff;
    int ivoff;
    int ovoff;
    int cwidth;
    int cheight;
    int threshold;
    int bttv_broken;
    int mm_flags;
    int isize;
    int osize;
    void (*yfilt)(uint8_t*, uint8_t*, uint8_t*, int, int, int);
    void (*cfilt)(uint8_t*, uint8_t*, uint8_t*, int, int, int);
    int first;
    mmx_t threshold_low;
    mmx_t threshold_high;
    uint8_t *prev;
    uint8_t *cur;
#ifdef TIME_FILTER
    int frames;
    double seconds;
#endif /* TIME_FILTER */
} ThisFilter;

void
KDP (uint8_t *PlanePrev, uint8_t *PlaneCur, uint8_t *PlaneOut, int W, int H,
     int Threshold)
{
    int X, Y;
    uint8_t *LineCur, *LineCur1U, *LineCur1D;
    uint8_t *LinePrev, *LinePrev2U, *LinePrev2D;
    PlaneCur = PlaneOut;
    LineCur = PlaneCur+W;
    LineCur1U = PlaneCur;
    LineCur1D = PlaneCur+2*W;
    LinePrev = PlanePrev+W;
    LinePrev2U = PlanePrev-W;
    LinePrev2D = PlanePrev+3*W;

    for (X = 0; X < W ; X++)
    {
        if (Threshold == 0 || ABS((int)LineCur[X]-(int)LineCur1U[X])
            > Threshold)
            LineCur[X] = (LineCur1U[X] + LineCur1D[X]) / 2;
    }
    LineCur += 2 * W;
    LineCur1U += 2 * W;
    LineCur1D += 2 * W;
    LinePrev += 2 * W;
    LinePrev2U += 2 * W;
    LinePrev2D += 2 * W;
    for (Y = 3; Y < H / 2 - 1; Y++)
    {
        for (X = 0; X < W; X++)
            if (Threshold == 0 || ABS((int)LineCur[X] - (int)LineCur1U[X])
                > Threshold)
                LineCur[X] = CLAMP((LineCur1U[X] * 4 + LineCur1D[X] * 4
                        + LinePrev[X] * 2 - LinePrev2U[X] - LinePrev2D[X])
                         / 8, 255, 0);
        LineCur += 2 * W;
        LineCur1U += 2 * W;
        LineCur1D += 2 * W;
        LinePrev += 2 * W;
        LinePrev2U += 2 * W;
        LinePrev2D += 2 * W;
    }
    for (X = 0; X < W; X++)
        if (Threshold == 0 || ABS((int)LineCur[X] - (int)LineCur1U[X])
            > Threshold)
            LineCur[X] = LineCur1U[X];
}

void
KDPD (uint8_t *PlanePrev, uint8_t *PlaneCur, uint8_t *PlaneOut, int W, int H,
      int Threshold)
{
    int X, Y;
    uint8_t *LineCur, *LineCur1U, *LineCur1D, *LinePrev, *LinePrev2U,
            *LinePrev2D, *LineOut;
    LineCur = PlaneCur+W;
    LineCur1U = PlaneCur;
    LineCur1D = PlaneCur+2*W;
    LinePrev = PlanePrev+W;
    LinePrev2U = PlanePrev-W;
    LinePrev2D = PlanePrev+3*W;
    LineOut = PlaneOut;

    for (X = 0; X < W ; X++)
    {
        if (Threshold == 0 || ABS((int)LineCur[X]-(int)LineCur1U[X])
            > Threshold)
            LineOut[X] = (LineCur1U[X] * 3 + LineCur1D[X]) / 4;
        else
            LineOut[X] = (LineCur1U[X] + LineCur[X]) / 2;
    }
    LineCur += 2 * W;
    LineCur1U += 2 * W;
    LineCur1D += 2 * W;
    LinePrev += 2 * W;
    LinePrev2U += 2 * W;
    LinePrev2D += 2 * W;
    LineOut += W;
    for (Y = 3; Y < H / 2 - 1; Y++)
    {
        for (X = 0; X < W; X++)
            if (Threshold == 0 || ABS((int)LineCur[X] - (int)LineCur1U[X])
                > Threshold)
                LineOut[X] = CLAMP((LineCur1U[X] * 12 + LineCur1D[X] * 4
                        + LinePrev[X] * 2 - LinePrev2U[X] - LinePrev2D[X])
                         / 16, 255, 0);
            else
                LineOut[X] = (LineCur1U[X] + LineCur[X]) / 2;
        LineCur += 2 * W;
        LineCur1U += 2 * W;
        LineCur1D += 2 * W;
        LinePrev += 2 * W;
        LinePrev2U += 2 * W;
        LinePrev2D += 2 * W;
        LineOut += W;
    }
    for (X = 0; X < W; X++)
        if (Threshold == 0 || ABS((int)LineCur[X] - (int)LineCur1U[X])
            > Threshold)
            LineOut[X] = LineCur1U[X];
        else
            LineOut[X] = (LineCur1U[X] + LineCur[X]) / 2;
}


static int
KernelDeint (VideoFilter * f, VideoFrame * frame)
{
    ThisFilter *filter = (ThisFilter *) f;
    uint8_t * tmp = filter->cur;

#ifdef TIME_FILTER
    struct timeval t1;
    gettimeofday (&t1, NULL);
#endif /* TIME_FILTER */
/* swap prev and cur buffers */
    filter->cur = filter->prev;
    filter->prev = tmp;

    if(filter->first)
    {
        memcpy (filter->prev, frame->buf, filter->isize);
        filter->first = 0;
    }
    memcpy (filter->cur, frame->buf, filter->isize);

    if (filter->yfilt)
        (filter->yfilt)(filter->prev, filter->cur, frame->buf, filter->width,
                        filter->height, filter->threshold);
    if (filter->cfilt)
    {
        (filter->cfilt)(filter->prev + filter->uoff,
                        filter->cur + filter->uoff, frame->buf + filter->uoff,
                        filter->cwidth, filter->cheight, filter->threshold);
        (filter->cfilt)(filter->prev + filter->ivoff,
                        filter->cur + filter->ivoff,
                        frame->buf + filter->ovoff,
                        filter->cwidth, filter->cheight, filter->threshold);
    }
    frame->codec = FMT_YV12;
    frame->size = filter->osize;
#ifdef TIME_FILTER
    struct timeval t2;
    gettimeofday (&t2, NULL);
    filter->seconds +=
        (t2.tv_sec - t1.tv_sec) + (t2.tv_usec - t1.tv_usec) * .000001;
    filter->frames = (filter->frames + 1) % TIME_INTERVAL;
    if (filter->frames == 0)
    {
        fprintf (stderr,
                 "KernelDeint: filter timed at %3f frames/sec for %dx%d\n",
                 TIME_INTERVAL / filter->seconds, filter->width,
                 filter->height);
        filter->seconds = 0;
    }
#endif /* TIME_FILTER */
    return 0;
}

void
CleanupKernelDeintFilter (VideoFilter * filter)
{
    free (((ThisFilter *)filter)->prev);
    free (((ThisFilter *)filter)->cur);
}

VideoFilter *
NewKernelDeintFilter (VideoFrameType inpixfmt, VideoFrameType outpixfmt,
                    int *width, int *height, char *options)
{
    ThisFilter *filter;
    int numopts;
    int bttv_broken;
    (void) options;

    if (outpixfmt != FMT_YV12)
    {
        fprintf (stderr, "KernelDeint: only YV12 output supported\n");
        return NULL;
    }

    if (inpixfmt != FMT_YV12 && inpixfmt != FMT_YUV422P)
    {
        fprintf (stderr, "KernelDeint: only YV12 and YUV422P input supported\n");
        return NULL;
    }

    filter = (ThisFilter *) malloc (sizeof(ThisFilter));
    if (filter == NULL)
    {
        fprintf (stderr, "KernelDeint: failed to allocate memory for filter.\n");
        return NULL;
    }
    
    numopts = sscanf(options, "%d:%d", &(filter->threshold), &bttv_broken);
    if (numopts < 2)
        bttv_broken = 0;
    if (numopts < 1)
        filter->threshold = 12;

    filter->width = *width;
    filter->height = *height;
    filter->cwidth = *width / 2;
    filter->uoff = *width * *height;
    filter->osize = *width * *height * 3 / 2;
    filter->yfilt = &KDP;
    switch (inpixfmt)
    {
        case FMT_YUV422P:
            filter->ivoff = filter->uoff + *width * *height / 2;
            filter->ovoff = filter->uoff + *width * *height / 4;
            filter->isize = *width * *height * 2;
            filter->cheight = *height;
            filter->cfilt = &KDPD;
            break;
        case FMT_YV12:
            filter->ivoff = filter->uoff + *width * *height / 4;
            filter->ovoff = filter->ivoff;
            filter->isize = *width * *height * 3 / 2;
            filter->cheight = *height / 2;
            if (bttv_broken)
                filter->cfilt = NULL;
            else
                filter->cfilt = &KDP;
            break;
        default:
            ;
    }

    filter->prev = malloc(filter->isize);
    if (filter->prev == NULL)
    {
        fprintf (stderr, "KernelDeint: failed to allocate frame buffer.\n");
        free (filter);
        return NULL;
    }
    filter->cur = malloc(filter->isize);
    if (filter->cur == NULL)
    {
        fprintf (stderr, "KernelDeint: failed to allocate frame buffer.\n");
        free (filter->prev);
        free (filter);
        return NULL;
    }
#ifdef TIME_FILTER
    filter->seconds = 0;
    filter->frames = 0;
#endif

    filter->vf.filter = &KernelDeint;
    filter->vf.cleanup = &CleanupKernelDeintFilter;
    return (VideoFilter *) filter;
}

static FmtConv FmtList[] =
{
    { FMT_YUV422P, FMT_YV12 },
    { FMT_YV12, FMT_YV12 },
    FMT_NULL
};

FilterInfo filter_table[] =
{
    {
        symbol:     "NewKernelDeintFilter",
        name:       "kerneldeint",
        descript:   "combines data from several fields to deinterlace with less motion blur",
        formats:    FmtList,
        libname:    NULL
    },
    FILT_NULL
};
