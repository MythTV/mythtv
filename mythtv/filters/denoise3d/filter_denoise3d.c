// Spatial/temporal denoising filter shamelessly swiped from Mplayer
// CPU detection borrowed from linearblend filter
// Mplayer filter code Copyright (C) 2003 Daniel Moreno <comac@comac.darktech.org>
// MythTV adaptation and MMX optimization by Andrew Mahone <andrewmahone@eml.cc>

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "filter.h"
#include "frame.h"

#define PARAM1_DEFAULT 4.0
#define PARAM2_DEFAULT 3.0
#define PARAM3_DEFAULT 6.0

#define LowPass(Prev, Curr, Coef) (Curr + Coef[Prev - Curr])

#define ABS(A) ( (A) > 0 ? (A) : -(A) )

#ifdef TIME_FILTER
#include <sys/time.h>
#ifndef TIME_INTERVAL
#define TIME_INTERVAL 1800
#endif
#endif

static const char FILTER_NAME[] = "denoise3d";

#include "mmx.h"

static const mmx_t mz = { 0x0LL };

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
    int (*filter) (VideoFilter *, VideoFrame *);
    void (*cleanup) (VideoFilter *);

    char *name;
    void *handle;               // Library handle;

    /* functions and variables below here considered "private" */
    int width;
    int height;
    int uoff;
    int voff;
    int cwidth;
    int cheight;

    uint8_t *line;
    uint8_t *prev;
    uint8_t coefs[4][512];
#ifdef TIME_FILTER
    int frames;
    double seconds;
#endif
} ThisFilter;

static void
PrecalcCoefs (uint8_t * Ct, double Dist25)
{
    int i;
    double Gamma, Simil, C;

    Gamma = log (0.25) / log (1.0 - Dist25 / 255.0);

    for (i = -256; i <= 255; i++)
    {
        Simil = 1.0 - ABS (i) / 255.0;
//        Ct[256+i] = lround(pow(Simil, Gamma) * (double)i);
        C = pow (Simil, Gamma) * (double) i;
        Ct[256 + i] = (C < 0) ? (C - 0.5) : (C + 0.5);
    }
}

static inline int
FiltCheckAndSetup (ThisFilter * filter, VideoFrame * frame)
{
    if (frame == NULL)
        return 1;
    if (frame->codec != FMT_YV12)
        return 1;
    if (frame->height != filter->height || frame->width != frame->width
        || !filter->line || !filter->prev)
    {
        if (filter->line)
            free ((void *) (filter->line));
        if (!(filter->line = (uint8_t *) malloc (frame->width)))
        {
            fprintf (stderr, "Couldn't allocate line buffer\n");
            fflush (stderr);
            filter->height = -1;
            filter->width = -1;
            return 1;
        }
        if (filter->prev)
            free ((void *) (filter->prev));
        if (!(filter->prev = (uint8_t *) malloc (frame->size)))
        {
            fprintf (stderr, "Couldn't allocate frame buffer\n");
            fflush (stderr);
            filter->height = -1;
            filter->width = -1;
            return 1;
        }
        filter->width = frame->width;
        filter->height = frame->height;
        filter->uoff = frame->width * frame->height;
        filter->voff = frame->width * frame->height * 5 / 4;
        filter->cwidth = frame->width / 2;
        filter->cheight = frame->height / 2;
        memcpy ((void *) filter->prev, (void *) frame->buf, frame->size);
    }
    return 0;
}

static void
denoise (uint8_t * Frame,       // mpi->planes[x]
         uint8_t * FramePrev,   // pmpi->planes[x]
         uint8_t * Line,
         int W, int H,
         uint8_t * Horizontal, uint8_t * Vertical, uint8_t * Temporal)
{
    uint8_t prev;
    int X, Y;
    uint8_t *LineCur = Frame;
    uint8_t *LinePrev = FramePrev;

    prev = Line[0] = Frame[0];
    Frame[0] = LowPass (FramePrev[0], Frame[1], Temporal);
    for (X = 1; X < W; X++)
    {
        prev = LowPass (prev, Frame[X], Horizontal);
        Line[X] = prev;
        Frame[X] = LowPass (FramePrev[X], prev, Temporal);
    }

    for (Y = 1; Y < H; Y++)
    {
        LineCur += W;
        LinePrev += W;
        prev = LineCur[0];
        Line[0] = LowPass (Line[0], prev, Vertical);
        LineCur[0] = LowPass (LinePrev[0], Line[0], Temporal);
        for (X = 1; X < W; X++)
        {
            prev = LowPass (prev, LineCur[X], Horizontal);
            Line[X] = LowPass (Line[X], prev, Vertical);
            LineCur[X] = LowPass (LinePrev[X], Line[X], Temporal);
        }
    }
}

static int
denoise3DFilter (VideoFilter * f, VideoFrame * frame)
{
    int width = frame->width;
    int height = frame->height;
    uint8_t *yuvptr = frame->buf;
    ThisFilter *filter = (ThisFilter *) f;

    if (FiltCheckAndSetup (filter, frame))
        return 1;

#ifdef TIME_FILTER
    struct timeval t1;
    gettimeofday (&t1, NULL);
#endif
    denoise (yuvptr, filter->prev, filter->line,
             width, height,
             filter->coefs[0] + 256,
             filter->coefs[0] + 256, filter->coefs[1] + 256);
    denoise (yuvptr + filter->uoff, filter->prev + filter->uoff, filter->line,
             filter->cwidth, filter->cheight,
             filter->coefs[2] + 256,
             filter->coefs[2] + 256, filter->coefs[3] + 256);
    denoise (yuvptr + filter->voff, filter->prev + filter->voff, filter->line,
             filter->cwidth, filter->cheight,
             filter->coefs[2] + 256,
             filter->coefs[2] + 256, filter->coefs[3] + 256);
    memcpy ((void *) filter->prev, (void *) yuvptr, frame->size);
#ifdef TIME_FILTER
    struct timeval t2;
    gettimeofday (&t2, NULL);
    filter->seconds +=
        (t2.tv_sec - t1.tv_sec) + (t2.tv_usec - t1.tv_usec) * .000001;
    filter->frames = (filter->frames + 1) % TIME_INTERVAL;
    if (filter->frames == 0)
    {
        fprintf (stderr,
                 "Denoise3D: filter timed at %3f frames/sec for %dx%d\n",
                 TIME_INTERVAL / filter->seconds, filter->width,
                 filter->height);
        filter->seconds = 0;
    }
#endif
    return 0;
}

static void
denoiseMMX (uint8_t * Frame,    // mpi->planes[x]
            uint8_t * FramePrev,        // pmpi->planes[x]
            uint8_t * Line,
            int W, int H,
            uint8_t * Horizontal, uint8_t * Vertical, uint8_t * Temporal)
{
    uint8_t prev;
    int X, Y;
    uint8_t *BlockCur, *BlockUp, *BlockPrev;
    uint8_t *LineCur = Frame;
    uint8_t *LinePrev = FramePrev;
    mmx_t cbuf[6];

    prev = Line[0] = Frame[0];
    BlockUp = Line;
    BlockCur = Frame;
    BlockPrev = FramePrev;
    for (X = 0; X < W; X++)
    {
        prev = LowPass (prev, Frame[X], Horizontal);
        Line[X] = prev;
    }
    for (X = 0; X < (W - 31); X += 32)
    {
        movq_m2r (BlockPrev[0], mm0);
        movq_r2r (mm0, mm1);
        punpcklbw_m2r (mz, mm0);
        movq_m2r (BlockUp[0], mm2);
        punpckhbw_m2r (mz, mm1);
        movq_r2r (mm2, mm3);
        punpcklbw_m2r (mz, mm2);
        psubw_r2r (mm2, mm0);
        punpckhbw_m2r (mz, mm3);
        psubw_r2r (mm3, mm1);
        movq_m2r (BlockPrev[8], mm2);
        movq_r2r (mm2, mm3);
        punpcklbw_m2r (mz, mm2);
        movq_m2r (BlockUp[8], mm4);
        punpckhbw_m2r (mz, mm3);
        movq_r2r (mm4, mm5);
        punpcklbw_m2r (mz, mm4);
        psubw_r2r (mm4, mm2);
        punpckhbw_m2r (mz, mm5);
        psubw_r2r (mm5, mm3);
        movq_m2r (BlockPrev[16], mm4);
        movq_r2r (mm4, mm5);
        punpcklbw_m2r (mz, mm4);
        movq_m2r (BlockUp[16], mm6);
        punpckhbw_m2r (mz, mm5);
        movq_r2r (mm6, mm7);
        punpcklbw_m2r (mz, mm6);
        psubw_r2r (mm6, mm4);
        punpckhbw_m2r (mz, mm7);
        movq_r2m (mm0, cbuf[0]);
        psubw_r2r (mm7, mm5);
        cbuf[0].ub[0] = Temporal[cbuf[0].w[0]];
        cbuf[0].ub[1] = Temporal[cbuf[0].w[1]];
        movq_r2m (mm1, cbuf[1]);
        cbuf[0].ub[2] = Temporal[cbuf[0].w[2]];
        cbuf[0].ub[3] = Temporal[cbuf[0].w[3]];
        movq_r2m (mm2, cbuf[2]);
        cbuf[0].ub[4] = Temporal[cbuf[1].w[0]];
        cbuf[0].ub[5] = Temporal[cbuf[1].w[1]];
        movq_r2m (mm3, cbuf[3]);
        cbuf[0].ub[6] = Temporal[cbuf[1].w[2]];
        cbuf[0].ub[7] = Temporal[cbuf[1].w[3]];
        movq_r2m (mm4, cbuf[4]);
        cbuf[1].ub[0] = Temporal[cbuf[2].w[0]];
        cbuf[1].ub[1] = Temporal[cbuf[2].w[1]];
        movq_m2r (cbuf[0], mm0);
        cbuf[1].ub[2] = Temporal[cbuf[2].w[2]];
        cbuf[1].ub[3] = Temporal[cbuf[2].w[3]];
        movq_r2m (mm5, cbuf[5]);
        cbuf[1].ub[4] = Temporal[cbuf[3].w[0]];
        cbuf[1].ub[5] = Temporal[cbuf[3].w[1]];
        paddb_m2r (BlockUp[0], mm0);
        cbuf[1].ub[6] = Temporal[cbuf[3].w[2]];
        cbuf[1].ub[7] = Temporal[cbuf[3].w[3]];
        movq_m2r (BlockPrev[16], mm3);
        cbuf[2].ub[0] = Temporal[cbuf[4].w[0]];
        cbuf[2].ub[1] = Temporal[cbuf[4].w[1]];
        movq_m2r (cbuf[1], mm1);
        cbuf[2].ub[2] = Temporal[cbuf[4].w[2]];
        cbuf[2].ub[3] = Temporal[cbuf[4].w[3]];
        paddb_m2r (BlockUp[8], mm1);
        cbuf[2].ub[4] = Temporal[cbuf[5].w[0]];
        cbuf[2].ub[5] = Temporal[cbuf[5].w[1]];
        movq_r2r (mm3, mm4);
        cbuf[2].ub[6] = Temporal[cbuf[5].w[2]];
        cbuf[2].ub[7] = Temporal[cbuf[5].w[3]];
        movq_m2r (cbuf[2], mm2);
        paddb_m2r (BlockUp[16], mm2);
        movq_m2r (BlockUp[24], mm5);
        punpcklbw_m2r (mz, mm3);
        movq_r2r (mm5, mm6);
        punpckhbw_m2r (mz, mm4);
        movq_r2m (mm0, BlockCur[0]);
        punpcklbw_m2r (mz, mm5);
        movq_r2m (mm1, BlockCur[8]);
        psubw_r2r (mm5, mm3);
        punpckhbw_m2r (mz, mm6);
        movq_r2m (mm3, cbuf[0]);
        psubw_r2r (mm6, mm4);
        movq_r2m (mm2, BlockCur[16]);
        cbuf[0].ub[0] = Temporal[cbuf[0].w[0]];
        cbuf[0].ub[1] = Temporal[cbuf[0].w[1]];
        movq_r2m (mm4, cbuf[1]);
        cbuf[0].ub[2] = Temporal[cbuf[0].w[2]];
        movq_r2m (mm0, BlockPrev[0]);
        cbuf[0].ub[3] = Temporal[cbuf[0].w[3]];
        movq_r2m (mm1, BlockPrev[8]);
        cbuf[0].ub[4] = Temporal[cbuf[1].w[0]];
        cbuf[0].ub[5] = Temporal[cbuf[1].w[1]];
        movq_r2m (mm2, BlockPrev[16]);
        cbuf[0].ub[6] = Temporal[cbuf[1].w[2]];
        cbuf[0].ub[7] = Temporal[cbuf[1].w[3]];
        movq_m2r (cbuf[0], mm3);
        paddb_m2r (BlockUp[24], mm3);
        movq_r2m (mm3, BlockCur[24]);
        movq_r2m (mm3, BlockPrev[16]);
        BlockUp += 32;
        BlockCur += 32;
        BlockPrev += 32;
    }
    for (/*X*/; X < W - 7; X += 8)
    {
        movq_m2r (BlockPrev[0], mm0);
        movq_r2r (mm0, mm1);
        punpcklbw_m2r (mz, mm0);
        movq_m2r (BlockUp[0], mm2);
        punpckhbw_m2r (mz, mm1);
        movq_r2r (mm2, mm3);
        punpcklbw_m2r (mz, mm2);
        punpckhbw_m2r (mz, mm3);
        psubw_r2r (mm2, mm0);
        psubw_r2r (mm3, mm1);
        movq_r2m (mm0, cbuf[0]);
        cbuf[0].ub[0] = Temporal[cbuf[0].w[0]];
        cbuf[0].ub[1] = Temporal[cbuf[0].w[1]];
        movq_r2m (mm1, cbuf[1]);
        cbuf[0].ub[2] = Temporal[cbuf[0].w[2]];
        cbuf[0].ub[3] = Temporal[cbuf[0].w[3]];
        cbuf[0].ub[4] = Temporal[cbuf[1].w[0]];
        cbuf[0].ub[5] = Temporal[cbuf[1].w[1]];
        cbuf[0].ub[6] = Temporal[cbuf[1].w[2]];
        cbuf[0].ub[7] = Temporal[cbuf[1].w[3]];
        movq_m2r (cbuf[0], mm0);
        paddb_m2r (BlockUp[0], mm0);
        movq_r2m (mm0, BlockCur[0]);
        movq_r2m (mm0, BlockPrev[0]);
        BlockUp += 8;
        BlockCur += 8;
        BlockPrev += 8;
    }
    for (/*X*/; X < W; X++)
    {
        FramePrev[X] = Frame[X] = LowPass (Line[X], Frame[X], Temporal);
    }

    for (Y = 1; Y < H; Y++)
    {
        LineCur += W;
        LinePrev += W;
        BlockUp = Line;
        BlockCur = LineCur;
        BlockPrev = LinePrev;
        prev = BlockCur[0];
        for (X = 0; X < W; X++)
        {
            prev = LowPass (prev, LineCur[X], Horizontal);
            LineCur[X] = prev;
        }
        for (X = 0; X < W - 7; X += 8)
        {
            movq_m2r (BlockUp[0], mm0);
            movq_r2r (mm0, mm1);
            punpcklbw_m2r (mz, mm0);
            movq_m2r (BlockCur[0], mm2);
            punpckhbw_m2r (mz, mm1);
            movq_r2r (mm2, mm3);
            punpcklbw_m2r (mz, mm2);
            psubw_r2r (mm2, mm0);
            punpckhbw_m2r (mz, mm3);
            psubw_r2r (mm3, mm1);
            movq_r2m (mm0, cbuf[0]);
            cbuf[0].ub[0] = Vertical[cbuf[0].w[0]];
            cbuf[0].ub[1] = Vertical[cbuf[0].w[1]];
            movq_r2m (mm1, cbuf[1]);
            cbuf[0].ub[2] = Vertical[cbuf[0].w[2]];
            cbuf[0].ub[3] = Vertical[cbuf[0].w[3]];
            cbuf[0].ub[4] = Vertical[cbuf[1].w[0]];
            movq_m2r (BlockCur[0], mm2);
            cbuf[0].ub[5] = Vertical[cbuf[1].w[1]];
            cbuf[0].ub[6] = Vertical[cbuf[1].w[2]];
            cbuf[0].ub[7] = Vertical[cbuf[1].w[3]];
            paddd_m2r (cbuf[0], mm2);
            movq_r2m (mm2, BlockUp[0]);
            movq_r2r (mm2, mm3);
            movq_m2r (BlockPrev[0], mm0);
            movq_r2r (mm0, mm1);
            punpcklbw_m2r (mz, mm0);
            punpcklbw_m2r (mz, mm2);
            punpckhbw_m2r (mz, mm1);
            psubw_r2r (mm2, mm0);
            punpckhbw_m2r (mz, mm3);
            movq_r2m (mm0, cbuf[0]);
            cbuf[0].ub[0] = Temporal[cbuf[0].w[0]];
            psubw_r2r (mm3, mm1);
            cbuf[0].ub[1] = Temporal[cbuf[0].w[1]];
            movq_r2m (mm1, cbuf[1]);
            cbuf[0].ub[2] = Temporal[cbuf[0].w[2]];
            cbuf[0].ub[3] = Temporal[cbuf[0].w[3]];
            cbuf[0].ub[4] = Temporal[cbuf[1].w[0]];
            movq_m2r (BlockUp[0], mm0);
            cbuf[0].ub[5] = Temporal[cbuf[1].w[1]];
            cbuf[0].ub[6] = Temporal[cbuf[1].w[2]];
            cbuf[0].ub[7] = Temporal[cbuf[1].w[3]];
            paddb_m2r (cbuf[0], mm0);
            movq_r2m (mm0, BlockCur[0]);
            BlockUp += 8;
            movq_r2m (mm0, BlockPrev[0]);
            BlockCur += 8;
            BlockPrev += 8;
        }
        for (/*X*/; X < W; X++)
        {
            prev = LowPass (prev, LineCur[X], Horizontal);
            Line[X] = LowPass (Line[X], prev, Vertical);
            LinePrev[X] = LineCur[X] =
                LowPass (LinePrev[X], Line[X], Temporal);
        }
    }
}

static int
denoise3DFilterMMX (VideoFilter * f, VideoFrame * frame)
{
    int width = frame->width;
    int height = frame->height;
    uint8_t *yuvptr = frame->buf;
    ThisFilter *filter = (ThisFilter *) f;

    if (FiltCheckAndSetup (filter, frame))
        return 1;

#ifdef TIME_FILTER
    struct timeval t1;
    gettimeofday (&t1, NULL);
#endif
    denoiseMMX (yuvptr, filter->prev, filter->line,
                width, height,
                filter->coefs[0] + 256,
                filter->coefs[0] + 256, filter->coefs[1] + 256);
    denoiseMMX (yuvptr + filter->uoff, filter->prev + filter->uoff,
                filter->line, filter->cwidth, filter->cheight,
                filter->coefs[2] + 256, filter->coefs[2] + 256,
                filter->coefs[3] + 256);
    denoiseMMX (yuvptr + filter->voff, filter->prev + filter->voff,
                filter->line, filter->cwidth, filter->cheight,
                filter->coefs[2] + 256, filter->coefs[2] + 256,
                filter->coefs[3] + 256);
    emms ();
#ifdef TIME_FILTER
    struct timeval t2;
    gettimeofday (&t2, NULL);
    filter->seconds +=
        (t2.tv_sec - t1.tv_sec) + (t2.tv_usec - t1.tv_usec) * .000001;
    filter->frames = (filter->frames + 1) % TIME_INTERVAL;
    if (filter->frames == 0)
    {
        fprintf (stderr,
                 "Denoise3D: filter timed at %3f frames/sec for %dx%d\n",
                 TIME_INTERVAL / filter->seconds, filter->width,
                 filter->height);
        filter->seconds = 0;
    }
#endif
    return 0;
}

void
cleanup (VideoFilter * filter)
{
    ThisFilter *vf = (ThisFilter *) filter;
    if (vf->prev)
        free ((void *) vf->prev);
    if (vf->line)
        free ((void *) vf->line);
    free (vf);
}

VideoFilter *
new_filter (char *options)
{
    double LumSpac, LumTmp, ChromSpac, ChromTmp;
    double Param1, Param2, Param3;
    ThisFilter *filter = malloc (sizeof (ThisFilter));
    int mm_flags = mm_support ();

    if (filter == NULL)
    {
        fprintf (stderr, "Couldn't allocate memory for filter\n");
        return NULL;
    }

    if (mm_flags & MM_MMX)
    {
        fprintf (stderr, "Using MMX filter\n");
        filter->filter = &denoise3DFilterMMX;
    }
    else
    {
        fprintf (stderr, "Using non-MMX filter\n");
        filter->filter = &denoise3DFilter;
    }
    filter->line = NULL;
    filter->prev = NULL;
    filter->width = -1;
    filter->width = -1;
    filter->cleanup = &cleanup;
    filter->name = (char *) FILTER_NAME;

    if (options)
    {
        switch (sscanf (options, "%lf:%lf:%lf", &Param1, &Param2, &Param3))
        {
        case 0:
            LumSpac = PARAM1_DEFAULT;
            LumTmp = PARAM3_DEFAULT;
            ChromSpac = PARAM2_DEFAULT;
            break;

        case 1:
            LumSpac = Param1;
            LumTmp = PARAM3_DEFAULT * Param1 / PARAM1_DEFAULT;
            ChromSpac = PARAM2_DEFAULT * Param1 / PARAM1_DEFAULT;
            break;

        case 2:
            LumSpac = Param1;
            LumTmp = PARAM3_DEFAULT * Param1 / PARAM1_DEFAULT;
            ChromSpac = Param2;
            break;

        case 3:
            LumSpac = Param1;
            LumTmp = Param3;
            ChromSpac = Param2;
            break;

        default:
            LumSpac = PARAM1_DEFAULT;
            LumTmp = PARAM3_DEFAULT;
            ChromSpac = PARAM2_DEFAULT;
        }
    }
    else
    {
        LumSpac = PARAM1_DEFAULT;
        LumTmp = PARAM3_DEFAULT;
        ChromSpac = PARAM2_DEFAULT;
    }

    ChromTmp = LumTmp * ChromSpac / LumSpac;
    PrecalcCoefs (filter->coefs[0], LumSpac);
    PrecalcCoefs (filter->coefs[1], LumTmp);
    PrecalcCoefs (filter->coefs[2], ChromSpac);
    PrecalcCoefs (filter->coefs[3], ChromTmp);
    return (VideoFilter *) filter;
}
