/* Spatial/temporal denoising filter shamelessly swiped from Mplayer
   CPU detection borrowed from linearblend filter
   Mplayer filter code Copyright (C) 2003 Daniel Moreno <comac@comac.darktech.org>
   MythTV adaptation and MMX optimization by Andrew Mahone <andrewmahone@eml.cc>
*/

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

/* function to determin cacheline size, it's not perfect because some flakey
   procs make an accurate test complicated, but it should be good enough
*/
int
cacheline_size (void)
{
    uint32_t eax, ebx, ecx, edx;

    cpuid (0x80000000, eax, ebx, ecx, edx)
    if (eax > 0x80000004)
    {
        cpuid (0x80000005, eax, ebx, ecx, edx);
        return (ecx & 0xff);
    }
    else
        return 32;
}

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
    int voff;
    int cwidth;
    int cheight;
    int first;
    int mm_flags;
    void (*filtfunc)(uint8_t*, uint8_t*, uint8_t*, int, int, uint8_t*,
                     uint8_t*);
    uint8_t *line;
    uint8_t *prev;
    uint8_t coefs[4][512];
    TF_STRUCT;
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
        C = pow (Simil, Gamma) * (double) i;
        Ct[256 + i] = (C < 0) ? (C - 0.5) : (C + 0.5);
    }
}


static void
denoise (uint8_t * Frame,
         uint8_t * FramePrev,
         uint8_t * Line,
         int W, int H,
         uint8_t * Spatial, uint8_t * Temporal)
{
    uint8_t prev;
    int X, Y;
    uint8_t *LineCur = Frame;
    uint8_t *LinePrev = FramePrev;

    prev = Line[0] = Frame[0];
    Frame[0] = LowPass (FramePrev[0], Frame[1], Temporal);
    for (X = 1; X < W; X++)
    {
        prev = LowPass (prev, Frame[X], Spatial);
        Line[X] = prev;
        FramePrev[X] = Frame[X] = LowPass (FramePrev[X], prev, Temporal);
    }

    for (Y = 1; Y < H; Y++)
    {
        LineCur += W;
        LinePrev += W;
        prev = LineCur[0];
        Line[0] = LowPass (Line[0], prev, Spatial);
        LineCur[0] = LowPass (LinePrev[0], Line[0], Temporal);
        for (X = 1; X < W; X++)
        {
            prev = LowPass (prev, LineCur[X], Spatial);
            Line[X] = LowPass (Line[X], prev, Spatial);
            LinePrev[X] = LineCur[X] = LowPass (LinePrev[X], Line[X], Temporal);
        }
    }
}

static void
denoiseMMX (uint8_t * Frame,
            uint8_t * FramePrev,
            uint8_t * Line,
            int W, int H,
            uint8_t * Spatial, uint8_t * Temporal)
{
    int X;
    uint8_t *LineCur = Frame;
    uint8_t *LinePrev = FramePrev;
    uint8_t *End = Frame + W * H;
    int16_t wbuf[16];
    uint8_t cbuf[16];

    Line[0] = LineCur[0];
    for (X = 1; X < W; X++)
        Line[X] = LowPass (Line[X-1], LineCur[X], Spatial);

    for (X = 0; X < W - 15; X += 16)
    {
        movq_m2r (LinePrev[X], mm0);
        movq_m2r (LinePrev[X+8], mm2);
        movq_m2r (Line[X], mm4);
        movq_m2r (Line[X+8], mm6);
        movq_r2r (mm0, mm1);
        movq_r2r (mm2, mm3);
        movq_r2r (mm4, mm5);
        movq_r2r (mm6, mm7);
        
        punpcklbw_m2r(mz, mm0);
        punpckhbw_m2r(mz, mm1);
        punpcklbw_m2r(mz, mm2);
        punpckhbw_m2r(mz, mm3);
        punpcklbw_m2r(mz, mm4);
        punpckhbw_m2r(mz, mm5);
        punpcklbw_m2r(mz, mm6);
        punpckhbw_m2r(mz, mm7);

        psubw_r2r (mm4, mm0);
        psubw_r2r (mm5, mm1);
        psubw_r2r (mm6, mm2);
        psubw_r2r (mm7, mm3);

        movq_r2m (mm0, wbuf[0]);
        movq_r2m (mm1, wbuf[4]);
        movq_r2m (mm2, wbuf[8]);
        movq_r2m (mm3, wbuf[12]);

        movq_m2r (Line[X], mm4);
        movq_m2r (Line[X+8], mm6);
        
        cbuf[0] = Temporal[wbuf[0]];
        cbuf[1] = Temporal[wbuf[1]];
        cbuf[2] = Temporal[wbuf[2]];
        cbuf[3] = Temporal[wbuf[3]];
        cbuf[4] = Temporal[wbuf[4]];
        cbuf[5] = Temporal[wbuf[5]];
        cbuf[6] = Temporal[wbuf[6]];
        cbuf[7] = Temporal[wbuf[7]];
        cbuf[8] = Temporal[wbuf[8]];
        cbuf[9] = Temporal[wbuf[9]];
        cbuf[10] = Temporal[wbuf[10]];
        cbuf[11] = Temporal[wbuf[11]];
        cbuf[12] = Temporal[wbuf[12]];
        cbuf[13] = Temporal[wbuf[13]];
        cbuf[14] = Temporal[wbuf[14]];
        cbuf[15] = Temporal[wbuf[15]];

        paddb_m2r (cbuf[0], mm4);
        paddb_m2r (cbuf[8], mm6);
        movq_r2m (mm4, LinePrev[X]);
        movq_r2m (mm6, LinePrev[X+8]);
        movq_r2m (mm4, LineCur[X]);
        movq_r2m (mm6, LineCur[X+8]);
    }

    for (/*X*/; X < W; X++)
        LineCur[X] = Line[X] = LowPass (LinePrev[X], Line[X], Temporal);

    LineCur += W;
    LinePrev += W;
    while (LineCur < End)
    {
        for (X = 1; X < W; X++)
            LineCur[X] = LowPass (LineCur[X-1], LineCur[X], Spatial);

        for (X = 0; X < W - 15; X += 16)
        {
            movq_m2r (Line[X], mm0);
            movq_m2r (Line[X+8], mm2);
            movq_m2r (LineCur[X], mm4);
            movq_m2r (LineCur[X+8], mm6);
            movq_r2r (mm0, mm1);
            movq_r2r (mm2, mm3);
            movq_r2r (mm4, mm5);
            movq_r2r (mm6, mm7);
            
            punpcklbw_m2r(mz, mm0);
            punpckhbw_m2r(mz, mm1);
            punpcklbw_m2r(mz, mm2);
            punpckhbw_m2r(mz, mm3);
            punpcklbw_m2r(mz, mm4);
            punpckhbw_m2r(mz, mm5);
            punpcklbw_m2r(mz, mm6);
            punpckhbw_m2r(mz, mm7);

            psubw_r2r (mm4, mm0);
            psubw_r2r (mm5, mm1);
            psubw_r2r (mm6, mm2);
            psubw_r2r (mm7, mm3);

            movq_r2m (mm0, wbuf[0]);
            movq_r2m (mm1, wbuf[4]);
            movq_r2m (mm2, wbuf[8]);
            movq_r2m (mm3, wbuf[12]);

            movq_m2r (LineCur[X], mm4);
            movq_m2r (LineCur[X+8], mm6);

            cbuf[0] = Spatial[wbuf[0]];
            cbuf[1] = Spatial[wbuf[1]];
            cbuf[2] = Spatial[wbuf[2]];
            cbuf[3] = Spatial[wbuf[3]];
            cbuf[4] = Spatial[wbuf[4]];
            cbuf[5] = Spatial[wbuf[5]];
            cbuf[6] = Spatial[wbuf[6]];
            cbuf[7] = Spatial[wbuf[7]];
            cbuf[8] = Spatial[wbuf[8]];
            cbuf[9] = Spatial[wbuf[9]];
            cbuf[10] = Spatial[wbuf[10]];
            cbuf[11] = Spatial[wbuf[11]];
            cbuf[12] = Spatial[wbuf[12]];
            cbuf[13] = Spatial[wbuf[13]];
            cbuf[14] = Spatial[wbuf[14]];
            cbuf[15] = Spatial[wbuf[15]];

            movq_m2r (LinePrev[X], mm0);
            movq_m2r (LinePrev[X+8], mm2);
            paddb_m2r (cbuf[0], mm4);
            paddb_m2r (cbuf[8], mm6);
            movq_r2m (mm4, Line[X]);
            movq_r2m (mm6, Line[X+8]);

            movq_r2r (mm0, mm1);
            movq_r2r (mm2, mm3);
            movq_r2r (mm4, mm5);
            movq_r2r (mm6, mm7);
            
            punpcklbw_m2r(mz, mm0);
            punpckhbw_m2r(mz, mm1);
            punpcklbw_m2r(mz, mm2);
            punpckhbw_m2r(mz, mm3);
            punpcklbw_m2r(mz, mm4);
            punpckhbw_m2r(mz, mm5);
            punpcklbw_m2r(mz, mm6);
            punpckhbw_m2r(mz, mm7);

            psubw_r2r (mm4, mm0);
            psubw_r2r (mm5, mm1);
            psubw_r2r (mm6, mm2);
            psubw_r2r (mm7, mm3);

            movq_r2m (mm0, wbuf[0]);
            movq_r2m (mm1, wbuf[4]);
            movq_r2m (mm2, wbuf[8]);
            movq_r2m (mm3, wbuf[12]);

            movq_m2r (Line[X], mm4);
            movq_m2r (Line[X+8], mm6);

            cbuf[0] = Temporal[wbuf[0]];
            cbuf[1] = Temporal[wbuf[1]];
            cbuf[2] = Temporal[wbuf[2]];
            cbuf[3] = Temporal[wbuf[3]];
            cbuf[4] = Temporal[wbuf[4]];
            cbuf[5] = Temporal[wbuf[5]];
            cbuf[6] = Temporal[wbuf[6]];
            cbuf[7] = Temporal[wbuf[7]];
            cbuf[8] = Temporal[wbuf[8]];
            cbuf[9] = Temporal[wbuf[9]];
            cbuf[10] = Temporal[wbuf[10]];
            cbuf[11] = Temporal[wbuf[11]];
            cbuf[12] = Temporal[wbuf[12]];
            cbuf[13] = Temporal[wbuf[13]];
            cbuf[14] = Temporal[wbuf[14]];
            cbuf[15] = Temporal[wbuf[15]];

            paddb_m2r (cbuf[0], mm4);
            paddb_m2r (cbuf[8], mm6);
            movq_r2m (mm4, LinePrev[X]);
            movq_r2m (mm6, LinePrev[X+8]);
            movq_r2m (mm4, LineCur[X]);
            movq_r2m (mm6, LineCur[X+8]);
        }

        for (/*X*/; X < W; X++)
        {
            Line[X] = LowPass (Line[X], LineCur[X], Spatial);
            LineCur[X] = LinePrev[X] = LowPass (LinePrev[X], Line[X], Temporal);
        }

        LineCur += W;
        LinePrev += W;
    }
}

static int
denoise3DFilter (VideoFilter * f, VideoFrame * frame)
{
    int width = frame->width;
    int height = frame->height;
    uint8_t *yuvptr = frame->buf;
    ThisFilter *filter = (ThisFilter *) f;
    TF_VARS;

    TF_START;
    if (filter->first)
    {
        memcpy(filter->prev, yuvptr, frame->size);
        filter->first = 0;
    }

    (filter->filtfunc) (yuvptr, filter->prev, filter->line, width, height,
                        filter->coefs[0] + 256, filter->coefs[1] + 256);
    (filter->filtfunc) (yuvptr + filter->uoff, filter->prev + filter->uoff,
                        filter->line, filter->cwidth, filter->cheight,
                        filter->coefs[2] + 256, filter->coefs[3] + 256);
    (filter->filtfunc) (yuvptr + filter->voff, filter->prev + filter->voff,
                        filter->line, filter->cwidth, filter->cheight,
                        filter->coefs[2] + 256, filter->coefs[3] + 256);
    if (filter->mm_flags & MM_MMX)
        emms();
    TF_END(filter, "Denoise3D: ");
    return 0;
}

void
Denoise3DFilterCleanup (VideoFilter * filter)
{
    free (((ThisFilter *)filter)->prev);
    free (((ThisFilter *)filter)->line);
}

VideoFilter *
NewDenoise3DFilter (VideoFrameType inpixfmt, VideoFrameType outpixfmt, 
                    int *width, int *height, char *options)
{
    double LumSpac, LumTmp, ChromSpac, ChromTmp;
    double Param1, Param2, Param3;
    ThisFilter *filter;
    if (inpixfmt != FMT_YV12)
        return NULL;
    if (outpixfmt != FMT_YV12)
        return NULL;
    filter = malloc (sizeof (ThisFilter));
    if (filter == NULL)
    {
        fprintf (stderr, "Denoise3D: failed to allocate memory for filter\n");
        return NULL;
    }

    filter->line = (uint8_t *) malloc (*width);
    if (filter->line == NULL )
    {
        fprintf (stderr, "Denoise3D: failed to allocate line buffer\n");
        free (filter);
        return NULL;
    }
    filter->prev = (uint8_t *) malloc (*width * *height * 3 / 2);
    if (filter->prev == NULL )
    {
        fprintf (stderr, "Denoise3D: failed to allocate frame buffer\n");
        free (filter->line);
        free (filter);
        return NULL;
    }
    filter->width = *width;
    filter->height = *height;
    filter->uoff = *width * *height;
    filter->voff = *width * *height * 5 / 4;
    filter->cwidth = *width / 2;
    filter->cheight = *height / 2;

    filter->mm_flags = mm_support();
    if (filter->mm_flags & MM_MMX)
    {
        filter->filtfunc = &denoiseMMX;
    }
    else
    {
        filter->filtfunc = &denoise;
    }
    filter->vf.filter = &denoise3DFilter;
    filter->vf.cleanup = &Denoise3DFilterCleanup;
    TF_INIT(filter);
    filter->first = 1;

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

static FmtConv FmtList[] = 
{
    { FMT_YV12, FMT_YV12 },
    FMT_NULL
};

FilterInfo filter_table[] = 
{
    {
        symbol:     "NewDenoise3DFilter",
        name:       "denoise3d",
        descript:   "removes noise with a spatial and temporal low-pass filter",
        formats:    FmtList,
        libname:    NULL
    },
    FILT_NULL
};
