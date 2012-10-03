/* range / gamma adjustment filter
*/

#include <stdlib.h>
#include <stdio.h>

#include "config.h"
#if HAVE_STDINT_H
#include <stdint.h>
#endif

#include <string.h>
#include <math.h>

#include "filter.h"
#include "frame.h"

#if HAVE_MMX

#include "libavutil/mem.h"
#include "libavcodec/dsputil.h"
#include "ffmpeg-mmx.h"

static const mmx_t mm_cpool[] = {
    { w: {1, 1, 1, 1} },
    { ub: {36, 36, 36, 36, 36, 36, 36, 36} },
    { ub: {20, 20, 20, 20, 20, 20, 20, 20} },
    { ub: {31, 31, 31, 31, 31, 31, 31, 31} },
    { ub: {15, 15, 15, 15, 15, 15, 15, 15} }
};
    
#endif /* HAVE_MMX */

typedef struct ThisFilter
{
    VideoFilter vf;

#if HAVE_MMX
    int yfilt;
    int cfilt;

    mmx_t yscale;
    mmx_t yshift;
    mmx_t ymin;

    mmx_t cscale;
    mmx_t cshift;
    mmx_t cmin;
#endif /* HAVE_MMX */

    uint8_t ytable[256];
    uint8_t ctable[256];

    TF_STRUCT;
} ThisFilter;

static void adjustRegion(uint8_t *buf, uint8_t *end, const uint8_t *table)
{
    while (buf < end)
    {
        *buf = table[*buf];
        buf++;
    }
}

#if HAVE_MMX
static void adjustRegionMMX(uint8_t *buf, uint8_t *end, const uint8_t *table,
                     const mmx_t *shift, const mmx_t *scale, const mmx_t *min,
                     const mmx_t *clamp1, const mmx_t *clamp2)
{
    movq_m2r (*scale, mm6);
    movq_m2r (*min, mm7);
    while (buf < end - 15)
    {
        movq_m2r (buf[0], mm0);
        movq_m2r (buf[8], mm2);
        movq_m2r (*shift, mm4);
        pxor_r2r (mm5, mm5);

        psubusb_r2r (mm7, mm0);
        psubusb_r2r (mm7, mm2);

        movq_r2r (mm0, mm1);
        movq_r2r (mm2, mm3);

        punpcklbw_r2r (mm5, mm0);
        punpckhbw_r2r (mm5, mm1);
        punpcklbw_r2r (mm5, mm2);
        punpckhbw_r2r (mm5, mm3);

        movq_m2r (mm_cpool[0], mm5);

        psllw_r2r (mm4, mm0);
        psllw_r2r (mm4, mm1);
        psllw_r2r (mm4, mm2);
        psllw_r2r (mm4, mm3);

        movq_m2r (*clamp1, mm4);

        pmulhw_r2r (mm6, mm0);
        pmulhw_r2r (mm6, mm1);
        pmulhw_r2r (mm6, mm2);
        pmulhw_r2r (mm6, mm3);

        paddw_r2r (mm5, mm0);
        paddw_r2r (mm5, mm1);
        paddw_r2r (mm5, mm2);
        paddw_r2r (mm5, mm3);

        movq_m2r (*clamp2, mm5);

        psrlw_i2r (1, mm0);
        psrlw_i2r (1, mm1);
        psrlw_i2r (1, mm2);
        psrlw_i2r (1, mm3);

        packuswb_r2r (mm1, mm0);
        packuswb_r2r (mm3, mm2);

        paddusb_r2r (mm4, mm0);
        paddusb_r2r (mm4, mm2);

        psubusb_r2r (mm5, mm0);
        psubusb_r2r (mm5, mm2);

        movq_r2m (mm0, buf[0]);
        movq_r2m (mm2, buf[8]);

        buf += 16;
    }
    while (buf < end)
    {
        *buf = table[*buf];
        buf++;
    }
}
#endif /* HAVE_MMX */

static int adjustFilter (VideoFilter *vf, VideoFrame *frame, int field)
{
    (void)field;
    ThisFilter *filter = (ThisFilter *) vf;
    TF_VARS;

    TF_START;
    {
        unsigned char *ybeg = frame->buf + frame->offsets[0];
        unsigned char *yend = ybeg + (frame->pitches[0] * frame->height);
        int cheight = (frame->codec == FMT_YV12) ?
            (frame->height >> 1) : frame->height;
        unsigned char *ubeg = frame->buf + frame->offsets[1];
        unsigned char *uend = ubeg + (frame->pitches[1] * cheight);
        unsigned char *vbeg = frame->buf + frame->offsets[2];
        unsigned char *vend = ubeg + (frame->pitches[2] * cheight);

#if HAVE_MMX
        if (filter->yfilt)
            adjustRegionMMX(ybeg, yend, filter->ytable,
                            &(filter->yshift), &(filter->yscale),
                            &(filter->ymin), mm_cpool + 1, mm_cpool + 2);
        else
            adjustRegion(ybeg, yend, filter->ytable);

        if (filter->cfilt)
        {
            adjustRegionMMX(ubeg, uend, filter->ctable,
                            &(filter->cshift), &(filter->cscale),
                            &(filter->cmin), mm_cpool + 3, mm_cpool + 4);
            adjustRegionMMX(vbeg, vend, filter->ctable,
                            &(filter->cshift), &(filter->cscale),
                            &(filter->cmin), mm_cpool + 3, mm_cpool + 4);
        }
        else
        {
            adjustRegion(ubeg, uend, filter->ctable);
            adjustRegion(vbeg, vend, filter->ctable);
        }

        if (filter->yfilt || filter->cfilt)
            emms();

#else /* HAVE_MMX */
        adjustRegion(ybeg, yend, filter->ytable);
        adjustRegion(ubeg, uend, filter->ctable);
        adjustRegion(vbeg, vend, filter->ctable);
#endif /* HAVE_MMX */
    }
    TF_END(filter, "Adjust: ");
    return 0;
}

static void fillTable(uint8_t *table, int in_min, int in_max, int out_min,
                int out_max, float gamma)
{
    int i;
    float f;

    for (i = 0; i < 256; i++)
    {
        f = ((float)i - in_min) / (in_max - in_min);
        f = f < 0.0 ? 0.0 : f;
        f = f > 1.0 ? 1.0 : f;
        table[i] = (pow (f, gamma) * (out_max - out_min) + out_min + 0.5);
    }
}

#if HAVE_MMX
static int fillTableMMX(uint8_t *table, mmx_t *shift, mmx_t *scale, mmx_t *min,
                   int in_min, int in_max, int out_min, int out_max,
                   float gamma)
{
    int shiftc, scalec, i;

    fillTable(table, in_min, in_max, out_min, out_max, gamma);
    scalec = ((out_max - out_min) << 15)/(in_max - in_min);
    if ((av_get_cpu_flags() & AV_CPU_FLAG_MMX) == 0 || gamma < 0.9999 || 
        gamma > 1.00001 || scalec > 32767 << 7)
        return 0;
    shiftc = 2;
    while (scalec > 32767)
    {
        shiftc++;
        scalec >>= 1;
    }
    if (shiftc > 7)
        return 0;
    for (i = 0; i < 4; i++)
    {
        scale->w[i] = scalec;
    }
    for (i = 0; i < 8; i++)
        min->b[i] = in_min;
    shift->q = shiftc;
    return 1;
}
#endif /* HAVE_MMX */

static VideoFilter *
newAdjustFilter (VideoFrameType inpixfmt, VideoFrameType outpixfmt, 
                 int *width, int *height, char *options, int threads)
{
    ThisFilter *filter;
    int numopts = 0, ymin = 16, ymax = 253, cmin = 16, cmax = 240;
    float ygamma = 1.0f, cgamma = 1.0f;
    (void) width;
    (void) height;
    (void) threads;

    if (inpixfmt != outpixfmt ||
        (inpixfmt != FMT_YV12 && inpixfmt != FMT_YUV422P))
    {
        fprintf(stderr, "adjust: only YV12->YV12 and YUV422P->YUV422P"
                " conversions are supported\n");
        return NULL;
    }

    if (options)
    {
        numopts = sscanf(options, "%20d:%20d:%20f:%20d:%20d:%20f",
                         &ymin, &ymax, &ygamma,
                         &cmin, &cmax, &cgamma);
    }

    if (numopts != 6 && (numopts !=1 && ymin != -1))
    {
        ymin = 16;
        ymax = 253;
        ygamma = 1.0f;
        cmin = 16;
        cmax = 240;
        cgamma = 1.0f;
    }

    filter = malloc (sizeof (ThisFilter));

    if (filter == NULL)
    {
        fprintf (stderr, "adjust: failed to allocate memory for filter\n");
        return NULL;
    }

    if (ymin == -1)
    {
        filter->vf.filter = NULL;
        filter->vf.cleanup = NULL;
        return (VideoFilter *) filter;
    }

#if HAVE_MMX
    filter->yfilt = fillTableMMX (filter->ytable, &(filter->yshift),
                                    &(filter->yscale), &(filter->ymin),
                                    ymin, ymax, 16, 235, ygamma);
    filter->cfilt = fillTableMMX (filter->ctable, &(filter->cshift),
                                    &(filter->cscale), &(filter->cmin),
                                    cmin, cmax, 16, 240, cgamma);
#else
    fillTable (filter->ytable, ymin, ymax, 16, 235, ygamma);
    fillTable (filter->ctable, cmin, cmax, 16, 240, cgamma);
#endif

    filter->vf.filter = &adjustFilter;
    filter->vf.cleanup = NULL;
    
    TF_INIT(filter);
    return (VideoFilter *) filter;
}

static FmtConv FmtList[] = 
{
    { FMT_YV12, FMT_YV12 },
    { FMT_YUV422P, FMT_YUV422P },
    FMT_NULL
};

const FilterInfo filter_table[] =
{
    {
        filter_init: &newAdjustFilter,
        name:       (char*)"adjust",
        descript:   (char*)"adjust range and gamma of video",
        formats:    FmtList,
        libname:    NULL
    },
    FILT_NULL
};
