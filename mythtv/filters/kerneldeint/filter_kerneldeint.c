/* Rewrite of neuron2's KernelDeint filter for Avisynth */

#include <stdlib.h>
#include <stdio.h>

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include <string.h>
#include <math.h>

#include "filter.h"
#include "frame.h"

#include "config.h"
#include "dsputil.h"

#undef ABS
#define ABS(A) ( (A) > 0 ? (A) : -(A) )
#define CLAMP(A,L,U) ((A)>(U)?(U):((A)<(L)?(L):(A)))

#ifdef MMX
#include "i386/mmx.h"

static const mmx_t mm_cpool[] =
{
    { 0x0000000000000000LL },
};

#else
#define mmx_t int
#endif

typedef struct ThisFilter
{
    VideoFilter vf;

    int threshold;
    int skipchroma;
    int mm_flags;
    void (*filtfunc)(uint8_t*, uint8_t*, int, int, int);
    mmx_t threshold_low;
    mmx_t threshold_high;
    uint8_t *line;
    int linesize;
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

#ifdef MMX
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
#endif

static int
KernelDeint (VideoFilter * f, VideoFrame * frame)
{
    ThisFilter *filter = (ThisFilter *) f;
    TF_VARS;

    if (frame->pitches[0] > filter->linesize)
    {
        if (filter->line)
            free(filter->line);
        filter->line = malloc(frame->pitches[0]);
        filter->linesize = frame->pitches[0];
    }

    if (!filter->line)
    {
        fprintf (stderr, "KernelDeint: failed to allocate line buffer.\n");
        return -1;
    }

    TF_START;
    {
        unsigned char *ybeg = frame->buf + frame->offsets[0];
        unsigned char *ubeg = frame->buf + frame->offsets[1];
        unsigned char *vbeg = frame->buf + frame->offsets[2];
        int cheight = (frame->codec == FMT_YV12) ?
            (frame->height >> 1) : frame->height;

        (filter->filtfunc)(ybeg, filter->line, frame->pitches[0],
                           frame->height, filter->threshold);

        if (!filter->skipchroma)
        {
            (filter->filtfunc)(ubeg, filter->line, frame->pitches[1],
                               cheight, filter->threshold);
            (filter->filtfunc)(vbeg, filter->line, frame->pitches[2],
                               cheight, filter->threshold);
        }
#ifdef MMX
        if (filter->mm_flags)
            emms();
#endif
    }
    TF_END(filter, "KernelDeint: ");
    return 0;
}

void
CleanupKernelDeintFilter (VideoFilter * filter)
{
    if (((ThisFilter *)filter)->line)
        free (((ThisFilter *)filter)->line);
}

VideoFilter *
NewKernelDeintFilter (VideoFrameType inpixfmt, VideoFrameType outpixfmt,
                    int *width, int *height, char *options)
{
    ThisFilter *filter;
    int numopts;
    (void) height;

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

#ifdef MMX
    filter->mm_flags = mm_support();
    if (filter->mm_flags & MM_MMX)
        filter->filtfunc = &KDP_MMX;
    else
#else
        filter->mm_flags = 0,
#endif
        filter->filtfunc = &KDP;

    filter->line = malloc(*width);
    filter->linesize = *width;

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
