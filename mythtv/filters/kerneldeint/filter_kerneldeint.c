/* Rewrite of neuron2's KernelDeint filter for Avisynth */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "filter.h"
#include "frame.h"

#define ABS(A) ( (A) > 0 ? (A) : -(A) )
#define CLAMP(A,L,U) ((A)>(U)?(U):((A)<(L)?(L):(A)))

#define MM_MMX    0x0001        /* standard MMX */
#define MM_3DNOW  0x0004        /* AMD 3DNOW */
#define MM_MMXEXT 0x0002        /* SSE integer functions or AMD MMX ext */
#define MM_SSE    0x0008        /* SSE functions */
#define MM_SSE2   0x0010        /* PIV SSE2 functions */

#ifdef i386
#include "mmx.h"

static const mmx_t mm_cpool[] =
{
    { 0x0000000000000000LL },
};

#define cpuid(index,eax,ebx,ecx,edx)\
    __asm __volatile\
        ("movl %%ebx, %%esi\n\t"\
         "cpuid\n\t"\
         "xchgl %%ebx, %%esi"\
         : "=a" (eax), "=S" (ebx),\
           "=c" (ecx), "=d" (edx)\
         : "0" (index));

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
#else // i386
int mm_support(void) { return 0; };
#define mmx_t int
#endif

typedef struct ThisFilter
{
    VideoFilter vf;

    int width;
    int height;
    int uoff;
    int voff;
    int cwidth;
    int cheight;
    int threshold;
    int skipchroma;
    int mm_flags;
    int size;
    void (*filtfunc)(uint8_t*, uint8_t*, int, int, int);
    mmx_t threshold_low;
    mmx_t threshold_high;
    uint8_t *line;
    TF_STRUCT;
} ThisFilter;

void
KDP (uint8_t *Plane, uint8_t *Line, int W, int H, int Threshold)
{
    int X, Y;
    uint8_t *LineCur, *LineCur1U, *LineCur1D, *LineCur2D, tmp;

    LineCur = Plane+W;
    LineCur1U = Plane;
    LineCur1D = Plane+2*W;
    LineCur2D = Plane+3*W;

    for (X = 0; X < W ; X++)
    {
        Line[X] = LineCur[X];
        if (Threshold == 0 || ABS((int)LineCur[X]-(int)LineCur1U[X])
            > Threshold - 1)
            LineCur[X] = (LineCur1U[X] + LineCur1D[X]) / 2;
    }
    LineCur += 2 * W;
    LineCur1U += 2 * W;
    LineCur1D += 2 * W;
    LineCur2D += 2 * W;
    for (Y = 3; Y < H / 2 - 1; Y++)
    {
        for (X = 0; X < W; X++)
        {
            tmp = Line[X];
            Line[X] = LineCur[X];
            if (Threshold == 0 || ABS((int)LineCur[X] - (int)LineCur1U[X])
                > Threshold-1)
                LineCur[X] = CLAMP((LineCur1U[X] * 4 + LineCur1D[X] * 4
                        + LineCur[X] * 2 - tmp - LineCur2D[X])
                         / 8, 0, 255);
        }
        LineCur += 2 * W;
        LineCur1U += 2 * W;
        LineCur1D += 2 * W;
        LineCur2D += 2 * W;
    }
    for (X = 0; X < W; X++)
        if (Threshold == 0 || ABS((int)LineCur[X] - (int)LineCur1U[X])
            > Threshold - 1)
            LineCur[X] = LineCur1U[X];
}

#ifdef i386
void
KDP_MMX (uint8_t *Plane, uint8_t *Line, int W, int H, int Threshold)
{
    int X, Y;
    uint8_t *LineCur, *LineCur1U, *LineCur1D, *LineCur2D, tmp;
    mmx_t mm_lthr = { w:{-Threshold,-Threshold,-Threshold,-Threshold} };
    mmx_t mm_hthr = { w:{Threshold-(Threshold>0),Threshold-(Threshold>0),
                         Threshold-(Threshold>0),Threshold-(Threshold>0)} };
    
    LineCur = Plane+W;
    LineCur1U = Plane;
    LineCur1D = Plane+2*W;
    LineCur2D = Plane+3*W;

    for (X = 0; X < W - 7; X += 8)
    {
        movq_m2r (LineCur1U[X], mm0);
        movq_m2r (LineCur1U[X], mm1);
        movq_m2r (LineCur1D[X], mm2);
        movq_m2r (LineCur1D[X], mm3);
        movq_m2r (LineCur[X], mm4);
        movq_m2r (LineCur[X], mm5);
        punpcklbw_m2r (mm_cpool[0], mm0);
        punpckhbw_m2r (mm_cpool[0], mm1);
        punpcklbw_m2r (mm_cpool[0], mm2);
        punpckhbw_m2r (mm_cpool[0], mm3);
        movq_r2r (mm0, mm6);
        movq_r2r (mm1, mm7);
        punpcklbw_m2r (mm_cpool[0], mm4);
        punpckhbw_m2r (mm_cpool[0], mm5);
        paddw_r2r (mm2, mm0);
        paddw_r2r (mm3, mm1);
        psrlw_i2r (1, mm0);
        psrlw_i2r (1, mm1);
        psubw_r2r (mm6, mm4);
        psubw_r2r (mm7, mm5);
        packuswb_r2r (mm1,mm0);
        movq_r2r (mm4, mm6);
        movq_r2r (mm5, mm7);
        pcmpgtw_m2r (mm_lthr, mm4);
        pcmpgtw_m2r (mm_lthr, mm5);
        pcmpgtw_m2r (mm_hthr, mm6);
        pcmpgtw_m2r (mm_hthr, mm7);
        packsswb_r2r (mm5, mm4);
        packsswb_r2r (mm7, mm6);
        pxor_r2r (mm6, mm4);
        movq_r2r (mm4, mm5);
        pandn_r2r (mm0, mm4);
        pand_m2r (LineCur[X], mm5);
        por_r2r (mm4, mm5);
        movq_r2m (mm5, LineCur[X]);
    } 

    for (/*X*/; X < W ; X++)
    {
        Line[X] = LineCur[X];
        if (Threshold == 0 || ABS((int)LineCur[X]-(int)LineCur1U[X])
            > Threshold - 1)
            LineCur[X] = (LineCur1U[X] + LineCur1D[X]) / 2;
    }

    LineCur += 2 * W;
    LineCur1U += 2 * W;
    LineCur1D += 2 * W;
    LineCur2D += 2 * W;
    for (Y = 0; Y < H / 2 - 2; Y++)
    {
        for (X = 0; X < W - 7; X += 8)
        {
            movq_m2r (LineCur1U[X], mm0);
            movq_m2r (LineCur1U[X], mm1);
            movq_m2r (LineCur1D[X], mm2);
            movq_m2r (LineCur1D[X], mm3);
            movq_m2r (LineCur[X], mm4);
            movq_m2r (LineCur[X], mm5);
            punpcklbw_m2r (mm_cpool[0], mm0);
            punpckhbw_m2r (mm_cpool[0], mm1);
            punpcklbw_m2r (mm_cpool[0], mm2);
            punpckhbw_m2r (mm_cpool[0], mm3);
            movq_r2r (mm0, mm6);
            movq_r2r (mm1, mm7);
            paddw_r2r (mm2, mm0);
            paddw_r2r (mm3, mm1);
            movq_m2r (LineCur[X], mm2);
            movq_m2r (LineCur[X], mm3);
            psllw_i2r (2, mm0);
            psllw_i2r (2, mm1);
            punpcklbw_m2r (mm_cpool[0], mm2);
            punpckhbw_m2r (mm_cpool[0], mm3);
            psllw_i2r (1, mm2);
            psllw_i2r (1, mm3);
            paddw_r2r (mm2, mm0);
            paddw_r2r (mm3, mm1);
            movq_m2r (Line[X], mm2);
            movq_m2r (Line[X], mm3);
            punpcklbw_m2r (mm_cpool[0], mm2);
            punpckhbw_m2r (mm_cpool[0], mm3);
            movq_r2m (mm4, Line[X]);
            punpcklbw_m2r (mm_cpool[0], mm4);
            punpckhbw_m2r (mm_cpool[0], mm5);
            psubusw_r2r (mm2, mm0);
            psubusw_r2r (mm3, mm1);
            movq_m2r (LineCur2D[X], mm2);
            movq_m2r (LineCur2D[X], mm3);
            punpcklbw_m2r (mm_cpool[0], mm2);
            punpckhbw_m2r (mm_cpool[0], mm3);
            psubusw_r2r (mm2, mm0);
            psubusw_r2r (mm3, mm1);
            psrlw_i2r (3, mm0);
            psrlw_i2r (3, mm1);
            psubw_r2r (mm6, mm4);
            psubw_r2r (mm7, mm5);
            packuswb_r2r (mm1,mm0);
            movq_r2r (mm4, mm6);
            movq_r2r (mm5, mm7);
            pcmpgtw_m2r (mm_lthr, mm4);
            pcmpgtw_m2r (mm_lthr, mm5);
            pcmpgtw_m2r (mm_hthr, mm6);
            pcmpgtw_m2r (mm_hthr, mm7);
            packsswb_r2r (mm5, mm4);
            packsswb_r2r (mm7, mm6);
            pxor_r2r (mm6, mm4);
            movq_r2r (mm4, mm5);
            pandn_r2r (mm0, mm4);
            pand_m2r (LineCur[X], mm5);
            por_r2r (mm4, mm5);
            movq_r2m (mm5, LineCur[X]);
        } 

        for (/*X*/; X < W; X++)
        {
            tmp = Line[X];
            Line[X] = LineCur[X];
            if (Threshold == 0 || ABS((int)LineCur[X] - (int)LineCur1U[X])
                > Threshold-1)
                LineCur[X] = CLAMP((LineCur1U[X] * 4 + LineCur1D[X] * 4
                        + LineCur[X] * 2 - tmp - LineCur2D[X])
                         / 8, 0, 255);
        }
        LineCur += 2 * W;
        LineCur1U += 2 * W;
        LineCur1D += 2 * W;
        LineCur2D += 2 * W;
    }
    for (X = 0; X < W; X++)
        if (Threshold == 0 || ABS((int)LineCur[X] - (int)LineCur1U[X])
            > Threshold - 1)
            LineCur[X] = LineCur1U[X];
}
#else
void KDP_MMX (uint8_t *Plane, uint8_t *Line, int W, int H, int Threshold) {
     (void)Plane; (void)Line; (void)W; (void)H; (void)Threshold;
}
#endif

static int
KernelDeint (VideoFilter * f, VideoFrame * frame)
{
    ThisFilter *filter = (ThisFilter *) f;
    TF_VARS;

    TF_START;
    (filter->filtfunc)(frame->buf, filter->line, filter->width,
                           filter->height, filter->threshold);
    if (!filter->skipchroma)
    {
        (filter->filtfunc)(frame->buf + filter->uoff, filter->line,
                           filter->cwidth, filter->cheight, filter->threshold);
        (filter->filtfunc)(frame->buf + filter->voff, filter->line,
                           filter->cwidth, filter->cheight, filter->threshold);
    }
    if (filter->mm_flags)
        emms();
    TF_END(filter, "KernelDeint: ");
    return 0;
}

void
CleanupKernelDeintFilter (VideoFilter * filter)
{
    free (((ThisFilter *)filter)->line);
}

VideoFilter *
NewKernelDeintFilter (VideoFrameType inpixfmt, VideoFrameType outpixfmt,
                    int *width, int *height, char *options)
{
    ThisFilter *filter;
    int numopts;

    if ( inpixfmt != outpixfmt ||
        (inpixfmt != FMT_YV12 && inpixfmt != FMT_YUV422P) )
    {
        fprintf (stderr, "KernelDeint: valid format conversions are"
                 " YV12->YV12 or YUV422P->YUV422P\n");
        return NULL;
    }

    filter = (ThisFilter *) malloc (sizeof(ThisFilter));
    if (filter == NULL)
    {
        fprintf (stderr, "KernelDeint: failed to allocate memory for filter.\n");
        return NULL;
    }
    
    numopts = options ? sscanf(options, "%d:%d", &(filter->threshold), &(filter->skipchroma)) : 0;
    if (numopts < 2)
        filter->skipchroma = 0;
    if (numopts < 1)
        filter->threshold = 12;

    filter->mm_flags = mm_support();
    filter->width = *width;
    filter->height = *height;
    filter->cwidth = *width / 2;
    filter->uoff = *width * *height;
    if (filter->mm_flags & MM_MMX)
        filter->filtfunc = &KDP_MMX;
    else
        filter->filtfunc = &KDP;
    switch (inpixfmt)
    {
        case FMT_YUV422P:
            filter->voff = filter->uoff + *width * *height / 2;
            filter->size = *width * *height * 2;
            filter->cheight = *height;
            break;
        case FMT_YV12:
            filter->voff = filter->uoff + *width * *height / 4;
            filter->size = *width * *height * 3 / 2;
            filter->cheight = *height / 2;
            break;
        default:
            ;
    }

    filter->line = malloc(*width);
    if (filter->line == NULL)
    {
        fprintf (stderr, "KernelDeint: failed to allocate line buffer.\n");
        free (filter);
        return NULL;
    }
    TF_INIT(filter);

    filter->vf.filter = &KernelDeint;
    filter->vf.cleanup = &CleanupKernelDeintFilter;
    return (VideoFilter *) filter;
}

static FmtConv FmtList[] =
{
    { FMT_YV12, FMT_YV12 },
    { FMT_YUV422P, FMT_YUV422P },
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
