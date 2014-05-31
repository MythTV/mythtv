/* Spatial/temporal denoising filter shamelessly swiped from Mplayer
   CPU detection borrowed from linearblend filter
   Mplayer filter code Copyright (C) 2003 Daniel Moreno <comac@comac.darktech.org>
   MythTV adaptation and MMX optimization by Andrew Mahone <andrewmahone@eml.cc>
*/

#include <stdlib.h>
#include <stdio.h>

#include "mythconfig.h"
#if HAVE_STDINT_H
#include <stdint.h>
#endif

#include <string.h>
#include <math.h>

#include "filter.h"
#include "mythframe.h"
#include "../mm_arch.h"

#define PARAM1_DEFAULT 4.0
#define PARAM2_DEFAULT 3.0
#define PARAM3_DEFAULT 6.0

#define LowPass(Prev, Curr, Coef) (Curr + Coef[Prev - Curr])

#undef ABS
#define ABS(A) ( (A) > 0 ? (A) : -(A) )

#ifdef MMX
#include "ffmpeg-mmx.h"
static const mmx_t mz = { 0x0LL };
#endif

typedef struct ThisFilter
{
    VideoFilter vf;

    int offsets[3];
    int pitches[3];
    int mm_flags;
    int line_size;
    int prev_size;
    uint8_t *line;
    uint8_t *prev;
    uint8_t coefs[4][512];

    void (*filtfunc)(uint8_t*, uint8_t*, uint8_t*,
                     int, int, uint8_t*, uint8_t*);

    TF_STRUCT;
} ThisFilter;

static void calc_coefs(uint8_t * Ct, double Dist25)
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

static void denoise(uint8_t *Frame,
                    uint8_t *FramePrev,
                    uint8_t *Line,
                    int W, int H,
                    uint8_t *Spatial, uint8_t *Temporal)
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

static void denoiseMMX(uint8_t *Frame,
                       uint8_t *FramePrev,
                       uint8_t *Line,
                       int W, int H,
                       uint8_t *Spatial, uint8_t *Temporal)
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

static int alloc_line(ThisFilter *filter, int size)
{
    if (filter->line_size >= size)
        return 1;

    uint8_t *tmp = realloc(filter->line, size);
    if (!tmp)
    {
        fprintf(stderr, "Couldn't allocate memory for line buffer\n");
        return 0;
    }

    filter->line = tmp;
    filter->line_size = size;

    return 1;
}

static int alloc_prev(ThisFilter *filter, int size)
{
    if (filter->prev_size >= size)
        return 1;

    uint8_t *tmp = realloc(filter->prev, size);
    if (!tmp)
    {
        fprintf(stderr, "Couldn't allocate memory for frame buffer\n");
        return 0;
    }

    filter->prev = tmp;
    filter->prev_size = size;

    return 1;
}

static int imax(int a, int b) { return (a > b) ? a : b; }

static int init_buf(ThisFilter *filter, VideoFrame *frame)
{
    if (!alloc_prev(filter, frame->size))
        return 0;

    int sz = imax(imax(frame->pitches[0], frame->pitches[1]), frame->pitches[2]);
    if (!alloc_line(filter, sz))
        return 0;

    if ((filter->prev_size  != frame->size)       ||
        (filter->offsets[0] != frame->offsets[0]) ||
        (filter->offsets[1] != frame->offsets[1]) ||
        (filter->offsets[2] != frame->offsets[2]) ||
        (filter->pitches[0] != frame->pitches[0]) ||
        (filter->pitches[1] != frame->pitches[1]) ||
        (filter->pitches[2] != frame->pitches[2]))
    {
        memcpy(filter->prev,    frame->buf,     frame->size);
        memcpy(filter->offsets, frame->offsets, sizeof(int) * 3);
        memcpy(filter->pitches, frame->pitches, sizeof(int) * 3);
    }

    return 1;
}

static int denoise3DFilter(VideoFilter *f, VideoFrame *frame, int field)
{
    (void)field;
    ThisFilter *filter = (ThisFilter*) f;
    TF_VARS;

    if (!init_buf(filter, frame))
        return -1;

    TF_START;

#ifdef MMX
    if (filter->mm_flags & AV_CPU_FLAG_MMX)
        emms();
#endif

    (filter->filtfunc)(frame->buf   + frame->offsets[0],
                       filter->prev + frame->offsets[0],
                       filter->line,  frame->pitches[0], frame->height,
                       filter->coefs[0] + 256,
                       filter->coefs[1] + 256);

    (filter->filtfunc)(frame->buf   + frame->offsets[1],
                       filter->prev + frame->offsets[1],
                       filter->line,  frame->pitches[1], frame->height >> 1,
                       filter->coefs[2] + 256,
                       filter->coefs[3] + 256);

    (filter->filtfunc)(frame->buf   + frame->offsets[2],
                       filter->prev + frame->offsets[2],
                       filter->line,  frame->pitches[2], frame->height >> 1,
                       filter->coefs[2] + 256,
                       filter->coefs[3] + 256);
#ifdef MMX
    if (filter->mm_flags & AV_CPU_FLAG_MMX)
        emms();
#endif

    TF_END(filter, "Denoise3D: ");
    return 0;
}

static void Denoise3DFilterCleanup(VideoFilter *filter)
{
    if (((ThisFilter*)filter)->prev)
        free(((ThisFilter*)filter)->prev);

    if (((ThisFilter*)filter)->line)
        free (((ThisFilter*)filter)->line);
}

static VideoFilter *NewDenoise3DFilter(VideoFrameType inpixfmt,
                                       VideoFrameType outpixfmt,
                                       int *width, int *height, char *options,
                                       int threads)
{
    double LumSpac   = PARAM1_DEFAULT;
    double LumTmp    = PARAM3_DEFAULT;
    double ChromSpac = PARAM2_DEFAULT;
    double ChromTmp  = 0.0;
    ThisFilter *filter;

    (void) width;
    (void) height;
    (void) threads;
    if (inpixfmt != FMT_YV12 || outpixfmt != FMT_YV12)
    {
        fprintf(stderr, "Denoise3D: attempt to initialize "
                "with unsupported format\n");
        return NULL;
    }

    filter = malloc(sizeof (ThisFilter));
    if (filter == NULL)
    {
        fprintf (stderr, "Denoise3D: failed to allocate memory for filter\n");
        return NULL;
    }

    memset(filter, 0, sizeof(ThisFilter));

    filter->vf.filter = &denoise3DFilter;
    filter->vf.cleanup = &Denoise3DFilterCleanup;
    filter->filtfunc = &denoise;

#ifdef MMX
    filter->mm_flags = av_get_cpu_flags();
    if (filter->mm_flags & AV_CPU_FLAG_MMX)
        filter->filtfunc = &denoiseMMX;
#endif

    TF_INIT(filter);

    if (options)
    {
        double param1, param2, param3;
        switch (sscanf(options, "%20lf:%20lf:%20lf",
                       &param1, &param2, &param3))
        {
            case 0:
            default:
                break;

            case 1:
                LumSpac   = param1;
                LumTmp    = PARAM3_DEFAULT * param1 / PARAM1_DEFAULT;
                ChromSpac = PARAM2_DEFAULT * param1 / PARAM1_DEFAULT;
                break;

            case 2:
                LumSpac   = param1;
                LumTmp    = PARAM3_DEFAULT * param1 / PARAM1_DEFAULT;
                ChromSpac = param2;
                break;

            case 3:
                LumSpac   = param1;
                LumTmp    = param3;
                ChromSpac = param2;
                break;
        }
    }

    ChromTmp = LumTmp * ChromSpac / LumSpac;

    calc_coefs(filter->coefs[0], LumSpac);
    calc_coefs(filter->coefs[1], LumTmp);
    calc_coefs(filter->coefs[2], ChromSpac);
    calc_coefs(filter->coefs[3], ChromTmp);

    return (VideoFilter*) filter;
}

static FmtConv FmtList[] =
{
    { FMT_YV12, FMT_YV12 },
    FMT_NULL
};

const FilterInfo filter_table[] =
{
    {
        .filter_init= &NewDenoise3DFilter,
        .name=       (char*)"denoise3d",
        .descript=   (char*)
        "removes noise with a spatial and temporal low-pass filter",
        .formats=    FmtList,
        .libname=    NULL
    },
    FILT_NULL
};
