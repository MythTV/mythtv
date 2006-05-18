/* taken from mplayer  */
/* inverse telecine video filter */
/* requires MMX support to work */

#include <stdlib.h>
#include <stdio.h>

#ifdef HAV_STDINT_H
#include <stdint.h>
#endif

#include <string.h>

#include "config.h"
#include "filter.h"
#include "frame.h"

#include "pullup.h"

typedef struct ThisFilter
{
    VideoFilter vf;

    struct pullup_context *context;
    int uoff;
    int voff;
    int height;
    int width;
    int progressive_frame_seen;
    int interlaced_frame_seen;
    int apply_filter;
} ThisFilter;

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
IvtcFilter (VideoFilter *vf, VideoFrame *frame)
{
    ThisFilter *filter = (ThisFilter *) vf; 

    if (!frame->interlaced_frame)
    {
        filter->progressive_frame_seen = 1;
    }
    
    if (filter->progressive_frame_seen &&
        frame->interlaced_frame)
    {
        filter->interlaced_frame_seen = 1;
    }
    
    if (!frame->interlaced_frame &&
        !filter->apply_filter &&
        filter->interlaced_frame_seen &&
        filter->progressive_frame_seen)
    {
        fprintf(stderr,"turning on inverse telecine filter");
        filter->apply_filter = 1;
    }
    
    if (!filter->apply_filter)
        return 1;
    
    struct pullup_buffer *b;
    struct pullup_frame *f;
    int height  = frame->height;
    int width   = frame->width;
    int cwidth  = width / 2;
    int cheight = height / 2;
    int p = frame->top_field_first ^ 1;

    struct pullup_context *c = filter->context;
    if (c->bpp[0] == 0)
        c->bpp[0] = c->bpp[1] = c->bpp[2] = frame->bpp;
    
    if ((frame->width != filter->width) ||
        (frame->height != filter->height))
    {
        return 0;
    }


    b = pullup_get_buffer(c,2);
    if (!b)
    {
        f = pullup_get_frame(c);
        pullup_release_frame(f);
        return 0;   
    }
        
    memcpy_pic(b->planes[0], frame->buf, height, width, width);
    memcpy_pic(b->planes[1], frame->buf + filter->uoff, cheight, cwidth, cwidth);
    memcpy_pic(b->planes[2], frame->buf + filter->voff, cheight, cwidth, cwidth);

    pullup_submit_field(c, b, p);
    pullup_submit_field(c, b, p^1);
    if (frame->repeat_pict)
        pullup_submit_field(c, b, p);

    pullup_release_buffer(b, 2);

    f = pullup_get_frame(c);

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

    memcpy_pic(frame->buf, f->buffer->planes[0], height, width, width);
    memcpy_pic(frame->buf + filter->uoff, f->buffer->planes[1], cheight, cwidth, cwidth);
    memcpy_pic(frame->buf + filter->voff, f->buffer->planes[2], cheight, cwidth, cwidth);
                            
    pullup_release_frame(f);
    return 1;
}

void
IvtcFilterCleanup( VideoFilter * filter)
{
    pullup_free_context((((ThisFilter *)filter)->context));    
}

VideoFilter *
NewIvtcFilter (VideoFrameType inpixfmt, VideoFrameType outpixfmt,
                        int *width, int *height, char *options)
{
    ThisFilter *filter;

    options = NULL;
    if (inpixfmt != FMT_YV12)
        return NULL;
    if (outpixfmt != FMT_YV12)
        return NULL;

    filter = malloc (sizeof (ThisFilter));

    if (filter == NULL)
    {
        fprintf (stderr, "Ivtc: failed to allocate memory for filter\n");
        return NULL;
    }

    filter->progressive_frame_seen = 0;
    filter->interlaced_frame_seen = 0;
    filter->apply_filter = 0;
    filter->height = *height;
    filter->width  = *width;
    filter->uoff = *width * *height;
    filter->voff = *width * *height  * 5 /  4;
    filter->context = pullup_alloc_context();
    struct pullup_context *c = filter->context;
    c->metric_plane = 0;
    c->strict_breaks = 0;
    c->junk_left = c->junk_right = 1;
    c->junk_top = c->junk_bottom = 4;
    c->verbose = 0;
    c->format = PULLUP_FMT_Y;
    c->nplanes = 4;
    pullup_preinit_context(c);
    c->bpp[0] = c->bpp[1] = c->bpp[2] = 0;
    c->w[0] = *width;
    c->h[0] = *height;
    c->w[1] = c->w[2] = (*width >> 1);
    c->h[1] = c->h[2] = (*height >> 1);
    c->w[3] = 0;
    c->h[3] = 0;
    c->stride[0] = *width;
    c->stride[1] = c->stride[2] = (*width >> 1);
    c->stride[3] = c->w[3];
    c->background[1] = c->background[2]  = 128;

#ifdef HAVE_MMX
    c->cpu      |= PULLUP_CPU_MMX;
#endif

    pullup_init_context(c);
    filter->vf.filter = &IvtcFilter;
    filter->vf.cleanup = &IvtcFilterCleanup;
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
        symbol:     "NewIvtcFilter",
        name:       "ivtc",
        descript:   "inverse telecine filter",
        formats:    FmtList,
        libname:    NULL    
    },
    FILT_NULL
};

