/*
 * Field-order deinterlacer
 *
 * Written by Paul Gardiner (mythtv@glidos.net), based on overal
 * structure of yadif deinterlacer.
 */
#include <stdlib.h>
#include <stdio.h>

#include "config.h"
#if HAVE_STDINT_H
#include <stdint.h>
#endif
#include <inttypes.h>

#include <string.h>
#include <math.h>

#include "filter.h"
#include "frame.h"

#define NREFS 2
#define NCHANS 3

typedef struct ThisFilter
{
    VideoFilter vf;

    long long last_framenr;

    uint8_t *ref[NREFS + 1][NCHANS];
    int stride[NCHANS];
    int8_t got_frames[NREFS + 1];

    int width;
    int height;

    TF_STRUCT;
} ThisFilter;


static void AllocFilter(ThisFilter* filter, int width, int height)
{
    int i, j;
    if ((width != filter->width) || height != filter->height)
    {
        for (i = 0; i < NCHANS * NREFS; i++)
        {
            uint8_t **p = &filter->ref[i / NCHANS][i % NCHANS];
            if (*p) free(*p);
            *p = NULL;
        }
        for (i = 0; i < NCHANS; i++)
        {
            int is_chroma = !!i;
            int w = ((width   + 31) & (~31)) >> is_chroma;
            int h = ((height  + 31) & (~31)) >> is_chroma;

            filter->stride[i] = w;
            for (j = 0; j < NREFS; j++) 
            {
                filter->ref[j][i] =
                    (uint8_t*)calloc(w * h * sizeof(uint8_t), 1);
            }
        }
        filter->width  = width;
        filter->height = height;
        memset(filter->got_frames, 0, sizeof(filter->got_frames));
    }
}

static inline void * memcpy_pic(void * dst, const void * src,
                                int bytesPerLine, int height,
                                int dstStride, int srcStride)
{
    int i;
    void *retval = dst;

    if (dstStride == srcStride)
    {
        if (srcStride < 0)
        {
            src = (const uint8_t*)src + (height - 1) * srcStride;
            dst = (uint8_t*)dst + (height - 1) * dstStride;
            srcStride = -srcStride;
        }
        memcpy(dst, src, srcStride * height);
    }
    else
    {
        for (i = 0; i < height; i++)
        {
            memcpy(dst, src, bytesPerLine);
            src = (const uint8_t*)src + srcStride;
            dst = (uint8_t*)dst + dstStride;
        }
    }
    return retval;
}

static void store_ref(struct ThisFilter *p, uint8_t *src, int src_offsets[3],
                      int src_stride[3], int width, int height)
{
    int i;

    memcpy (p->ref[NREFS], p->ref[0], sizeof(uint8_t *) * NCHANS);
    memmove(p->ref[0], p->ref[1], sizeof(uint8_t *) * NREFS * NCHANS);
    memcpy (&p->got_frames[NREFS], &p->got_frames[0], sizeof(uint8_t));
    memmove(&p->got_frames[0], &p->got_frames[1], sizeof(uint8_t) * NREFS);

    for (i = 0; i < NCHANS; i++)
    {
        int is_chroma = !!i;
        memcpy_pic(p->ref[NREFS-1][i], src + src_offsets[i],
                   width >> is_chroma, height >> is_chroma,
                   p->stride[i], src_stride[i]);
    }
    p->got_frames[NREFS - 1] = 1;
}

static void filter_func(struct ThisFilter *p, uint8_t *dst,
                        int dst_offsets[3], int dst_stride[3], int width,
                        int height, int parity, int tff, int dirty)
{
    int i, y;
    uint8_t nr_p, nr_c;
    nr_c = NREFS - 1;
    nr_p = p->got_frames[NREFS - 2] ? (NREFS - 2) : nr_c;

    for (i = 0; i < NCHANS; i++)
    {
        int is_chroma = !!i;
        int w    = width  >> is_chroma;
        int h    = height >> is_chroma;
        int refs = p->stride[i];

        for (y = 0; y < h; y++)
        {
            int do_copy = dirty;
            uint8_t *dst2 = dst + dst_offsets[i] + y * dst_stride[i];
            uint8_t *src  = &p->ref[nr_c][i][y * refs];
            int     field = parity ^ tff;
            if (((y ^ (1 - field)) & 1) && !parity)
            {
                src = &p->ref[nr_p][i][y * refs];
                do_copy = 1;
            }
            if (do_copy)
                memcpy(dst2, src, w);
        }
    }
}

static int FieldorderDeint (VideoFilter * f, VideoFrame * frame, int field)
{
    ThisFilter *filter = (ThisFilter *) f;
    TF_VARS;

    AllocFilter(filter, frame->width, frame->height);

    int dirty = 1;
    if (filter->last_framenr != frame->frameNumber)
    {
        if (filter->last_framenr != (frame->frameNumber - 1))
        {
            memset(filter->got_frames, 0, sizeof(filter->got_frames));
        }
        store_ref(filter, frame->buf,  frame->offsets,
                  frame->pitches, frame->width, frame->height);
        dirty = 0;
    }

    filter_func(
        filter, frame->buf, frame->offsets, frame->pitches,
        frame->width, frame->height, field, frame->top_field_first,
        dirty);

    filter->last_framenr = frame->frameNumber;

    return 0;
}


static void CleanupFieldorderDeintFilter(VideoFilter * filter)
{
    int i;
    ThisFilter* f = (ThisFilter*)filter;
    for (i = 0; i < NCHANS * NREFS; i++)
    {
        uint8_t **p= &f->ref[i / NCHANS][i % NCHANS];
        if (*p) free(*p);
        *p= NULL;
    }
}

static VideoFilter *FieldorderDeintFilter(VideoFrameType inpixfmt,
                                          VideoFrameType outpixfmt,
                                          int *width, int *height,
                                          char *options, int threads)
{
    ThisFilter *filter;
    (void) height;
    (void) options;

    filter = (ThisFilter *) malloc (sizeof(ThisFilter));
    if (filter == NULL)
    {
        fprintf (stderr, "FieldorderDeint: failed to allocate memory for filter.\n");
        return NULL;
    }

    filter->width = 0;
    filter->height = 0;
    memset(filter->ref, 0, sizeof(filter->ref));

    AllocFilter(filter, *width, *height);

    filter->vf.filter = &FieldorderDeint;
    filter->vf.cleanup = &CleanupFieldorderDeintFilter;
    return (VideoFilter *) filter;
}

static FmtConv FmtList[] =
{
    { FMT_YV12, FMT_YV12 } ,
    FMT_NULL
};

const FilterInfo filter_table[] =
{
    {
        .filter_init= &FieldorderDeintFilter,
        .name=       (char*)"fieldorderdoubleprocessdeint",
        .descript=   (char*)"avoids synchronisation problems when matching an "
                    "interlaced video mode to an interlaced source",
        .formats=    FmtList,
        .libname=    NULL
    }, FILT_NULL
};

/* vim: set expandtab tabstop=4 shiftwidth=4: */
