/* Spatial/temporal denoising filter shamelessly swiped from Mplayer
   CPU detection borrowed from linearblend filter
   Mplayer filter code Copyright (C) 2003 Daniel Moreno <comac@comac.darktech.org>
   MythTV adaptation and MMX optimization by Andrew Mahone <andrewmahone@eml.cc>
*/

#include <stdlib.h>
#include <stdio.h>

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include <string.h>
#include <math.h>

#include "config.h"
#include "filter.h"
#include "frame.h"
#include "dsputil.h"

#define PARAM1_DEFAULT 4.0
#define PARAM2_DEFAULT 3.0
#define PARAM3_DEFAULT 6.0

#define LowPass(Prev, Curr, Coef) (Curr + Coef[Prev - Curr])

#undef ABS
#define ABS(A) ( (A) > 0 ? (A) : -(A) )

#ifdef MMX
#include "i386/mmx.h"
static const mmx_t mz = { 0x0LL };
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

static void SetupSize(ThisFilter *filter, VideoFrameType inpixfmt,
                      int *width, int *height)
{
    (void) inpixfmt;

    if (filter->line)
        free(filter->line);

    filter->line = (uint8_t *) malloc (*width);
    if (filter->line == NULL )
    {
        fprintf (stderr, "Denoise3D: failed to allocate line buffer\n");
        free (filter);
    }

    if (filter->prev)
        free(filter->prev);

    filter->prev = (uint8_t *) malloc (*width * *height * 3 / 2);
    if (filter->prev == NULL )
    {
        fprintf (stderr, "Denoise3D: failed to allocate frame buffer\n");
        free (filter->line);
        filter->line = NULL;
    }
    filter->width = *width;
    filter->height = *height;
    filter->uoff = *width * *height;
    filter->voff = *width * *height * 5 / 4;
    filter->cwidth = *width / 2;
    filter->cheight = *height / 2;
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

#ifdef MMX

static void
denoiseMMX (uint8_t * Frame,
            uint8_t * FramePrev,
            uint8_t * Line,
            int W, int H,
            uint8_t * Spatial, uint8_t * Temporal)
{
    int X, i;
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
        
        for (i = 0; i < 16; i++)
            cbuf[i] = Temporal[wbuf[i]];

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

            for (i = 0; i < 16; i++)
                cbuf[i] = Spatial[wbuf[i]];

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

            for (i = 0; i < 16; i++)
                cbuf[i] = Temporal[wbuf[i]];

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
#endif /* MMX */

static int
denoise3DFilter (VideoFilter * f, VideoFrame * frame)
{
    int width = frame->width;
    int height = frame->height;
    uint8_t *yuvptr = frame->buf;
    ThisFilter *filter = (ThisFilter *) f;
    TF_VARS;

    if (frame->width != filter->width || frame->height != filter->height)
        SetupSize(filter, frame->codec, &frame->width, &frame->height);

    if (!filter->line || !filter->prev)
    {
        fprintf(stderr, "denoise3d: failed to allocate buffers\n");
        return -1;
    }

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
#ifdef MMX
    if (filter->mm_flags & MM_MMX)
        emms();
#endif
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

    filter->line = NULL;
    filter->prev = NULL;

    SetupSize(filter, inpixfmt, width, height);

#ifdef MMX
    filter->mm_flags = mm_support();
    if (filter->mm_flags & MM_MMX)
        filter->filtfunc = &denoiseMMX;
    else
#else
        filter->mm_flags = 0,
#endif
        filter->filtfunc = &denoise;
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
