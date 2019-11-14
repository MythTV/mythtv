/* taken from mplayer  */
/* inverse telecine video filter */
/* requires MMX support to work */

#include <stdlib.h>
#include <stdio.h>

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include <string.h>

#include "config.h"
#include "filter.h"
#include "mythframe.h"

#include "pullup.h"

typedef struct ThisFilter
{
    VideoFilter m_vf;

    struct pullup_context *m_context;
    int                    m_height;
    int                    m_width;
    int                    m_progressiveFrameSeen;
    int                    m_interlacedFrameSeen;
    int                    m_applyFilter;
} ThisFilter;

static void SetupFilter(ThisFilter *vf, int width, int height, const int *pitches);

static inline void * memcpy_pic(void * dst, const void * src, int height, int dstStride, int srcStride)
{
    void *retval=dst;

    if (dstStride == srcStride)
    {
        memcpy(dst, src, srcStride*height);
    }

    return retval;
}

static int
IvtcFilter (VideoFilter *vf, VideoFrame *frame, int field)
{
    (void)field;
    ThisFilter *filter = (ThisFilter *) vf;

    if (!frame->interlaced_frame)
    {
        filter->m_progressiveFrameSeen = 1;
    }

    if (filter->m_progressiveFrameSeen &&
        frame->interlaced_frame)
    {
        filter->m_interlacedFrameSeen = 1;
    }

    if (!frame->interlaced_frame &&
        !filter->m_applyFilter &&
        filter->m_interlacedFrameSeen &&
        filter->m_progressiveFrameSeen)
    {
        fprintf(stderr,"turning on inverse telecine filter");
        filter->m_applyFilter = 1;
    }

    if (!filter->m_applyFilter)
        return 1;

    SetupFilter(filter, frame->width, frame->height, (int*)frame->pitches);

    int ypitch  = filter->m_context->stride[0];
    int height  = filter->m_height;
    int cpitch  = filter->m_context->stride[1];
    int cheight = filter->m_height >> 1;
    int p = frame->top_field_first ^ 1;

    struct pullup_context *c = filter->m_context;
    if (c->bpp[0] == 0)
        c->bpp[0] = c->bpp[1] = c->bpp[2] = frame->bpp;

    struct pullup_buffer *b = pullup_get_buffer(c,2);
    if (!b)
    {
        struct pullup_frame *f = pullup_get_frame(c);
        pullup_release_frame(f);
        return 0;
    }

    memcpy_pic(b->planes[0], frame->buf + frame->offsets[0],
               height,  ypitch, ypitch);
    memcpy_pic(b->planes[1], frame->buf + frame->offsets[1],
               cheight, cpitch, cpitch);
    memcpy_pic(b->planes[2], frame->buf + frame->offsets[2],
               cheight, cpitch, cpitch);

    pullup_submit_field(c, b, p);
    pullup_submit_field(c, b, p^1);
    if (frame->repeat_pict)
        pullup_submit_field(c, b, p);

    pullup_release_buffer(b, 2);

    struct pullup_frame *f = pullup_get_frame(c);
    if (!f)
        return 0;


    if (f->length < 2)
    {
        pullup_release_frame(f);
        f = pullup_get_frame(c);
        if (!f)
            return 0;
        if (f->length < 2)
        {
            pullup_release_frame(f);
            if (!frame->repeat_pict)
                return 0;
            f = pullup_get_frame(c);
            if (!f)
                return 0;
            if (f->length < 2)
            {
                pullup_release_frame(f);
                return 0;
            }
        }
    }

    if (!f->buffer)
    {
        pullup_pack_frame(c, f);
    }

    if (!f->buffer)
        return 0;

    memcpy_pic(frame->buf + frame->offsets[0], f->buffer->planes[0],
               height,  ypitch, ypitch);
    memcpy_pic(frame->buf + frame->offsets[1], f->buffer->planes[1],
               cheight, cpitch, cpitch);
    memcpy_pic(frame->buf + frame->offsets[2], f->buffer->planes[2],
               cheight, cpitch, cpitch);

    pullup_release_frame(f);

    return 1;
}

static void
IvtcFilterCleanup( VideoFilter * filter)
{
    pullup_free_context((((ThisFilter *)filter)->m_context));
}

static void SetupFilter(ThisFilter *vf, int width, int height, const int *pitches)
{
    if (vf->m_width  == width  &&
        vf->m_height == height &&
        vf->m_context->stride[0] == pitches[0] &&
        vf->m_context->stride[1] == pitches[1] &&
        vf->m_context->stride[2] == pitches[2])
    {
        return;
    }

    vf->m_width         = width;
    vf->m_height        = height;

    vf->m_context->w[0] = width;
    vf->m_context->w[1] = width >> 1;
    vf->m_context->w[2] = width >> 1;
    vf->m_context->w[3] = 0;
    vf->m_context->h[0] = height;
    vf->m_context->h[1] = height >> 1;
    vf->m_context->h[2] = height >> 1;
    vf->m_context->h[3] = 0;
    vf->m_context->stride[0] = pitches[0];
    vf->m_context->stride[1] = pitches[1];
    vf->m_context->stride[2] = pitches[2];
    vf->m_context->stride[3] = 0;
}

static VideoFilter *NewIvtcFilter(VideoFrameType inpixfmt,
                                  VideoFrameType outpixfmt,
                                  const int *width, const int *height, const char *options,
                                  int threads)
{
    (void) threads;
    (void) options;

    if (inpixfmt != FMT_YV12)
        return NULL;
    if (outpixfmt != FMT_YV12)
        return NULL;

    ThisFilter *filter = malloc (sizeof (ThisFilter));
    if (filter == NULL)
    {
        fprintf (stderr, "Ivtc: failed to allocate memory for filter\n");
        return NULL;
    }

    memset(filter, 0, sizeof(ThisFilter));
    filter->m_progressiveFrameSeen = 0;
    filter->m_interlacedFrameSeen = 0;
    filter->m_applyFilter = 0;
    filter->m_context = pullup_alloc_context();
    struct pullup_context *c = filter->m_context;
    c->metric_plane = 0;
    c->strict_breaks = 0;
    c->junk_left = c->junk_right = 1;
    c->junk_top = c->junk_bottom = 4;
    c->verbose = 0;
    c->format = PULLUP_FMT_Y;
    c->nplanes = 4;
    pullup_preinit_context(c);
    c->bpp[0] = c->bpp[1] = c->bpp[2] = 0;
    c->background[1] = c->background[2]  = 128;

    int pitches[3] = { *width, *width >> 1, *width >> 1 };
    SetupFilter(filter, *width, *height, pitches);

#if HAVE_MMX
    c->cpu      |= PULLUP_CPU_MMX;
#endif

    pullup_init_context(c);
    filter->m_vf.filter = &IvtcFilter;
    filter->m_vf.cleanup = &IvtcFilterCleanup;
    return (VideoFilter *) filter;
}

static FmtConv FmtList[] =
{
        { FMT_YV12, FMT_YV12 },
            FMT_NULL
};

const FilterInfo filter_table[] =
{
    {
        .filter_init= &NewIvtcFilter,
        .name=       (char*)"ivtc",
        .descript=   (char*)"inverse telecine filter",
        .formats=    FmtList,
        .libname=    NULL
    },
    FILT_NULL
};

