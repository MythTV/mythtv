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
#include "mythframe.h"

#define NREFS 2
#define NCHANS 3

typedef struct ThisFilter
{
    VideoFilter  m_vf;

    long long    m_lastFrameNr;

    uint8_t     *m_ref[NREFS + 1][NCHANS];
    int          m_stride[NCHANS];
    int8_t       m_gotFrames[NREFS + 1];

    int          m_width;
    int          m_height;

    TF_STRUCT;
} ThisFilter;


static void AllocFilter(ThisFilter* filter, int width, int height)
{
    if ((width != filter->m_width) || height != filter->m_height)
    {
        for (int i = 0; i < NCHANS * NREFS; i++)
        {
            uint8_t **p = &filter->m_ref[i / NCHANS][i % NCHANS];
            if (*p) free(*p);
            *p = NULL;
        }
        for (int i = 0; i < NCHANS; i++)
        {
            int is_chroma = !!i;
            int w = ((width   + 31) & (~31)) >> is_chroma;
            int h = ((height  + 31) & (~31)) >> is_chroma;

            filter->m_stride[i] = w;
            for (int j = 0; j < NREFS; j++) 
            {
                filter->m_ref[j][i] =
                    (uint8_t*)calloc(w * h * sizeof(uint8_t), 1);
            }
        }
        filter->m_width  = width;
        filter->m_height = height;
        memset(filter->m_gotFrames, 0, sizeof(filter->m_gotFrames));
    }
}

static inline void * memcpy_pic(void * dst, const void * src,
                                int bytesPerLine, int height,
                                int dstStride, int srcStride)
{
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
        for (int i = 0; i < height; i++)
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
    memcpy (p->m_ref[NREFS], p->m_ref[0], sizeof(uint8_t *) * NCHANS);
    memmove(p->m_ref[0], p->m_ref[1], sizeof(uint8_t *) * NREFS * NCHANS);
    memcpy (&p->m_gotFrames[NREFS], &p->m_gotFrames[0], sizeof(uint8_t));
    memmove(&p->m_gotFrames[0], &p->m_gotFrames[1], sizeof(uint8_t) * NREFS);

    for (int i = 0; i < NCHANS; i++)
    {
        int is_chroma = !!i;
        memcpy_pic(p->m_ref[NREFS-1][i], src + src_offsets[i],
                   width >> is_chroma, height >> is_chroma,
                   p->m_stride[i], src_stride[i]);
    }
    p->m_gotFrames[NREFS - 1] = 1;
}

static void filter_func(struct ThisFilter *p, uint8_t *dst,
                        int dst_offsets[3], const int dst_stride[3], int width,
                        int height, int parity, int tff, int dirty)
{
    uint8_t nr_c = NREFS - 1;
    uint8_t nr_p = p->m_gotFrames[NREFS - 2] ? (NREFS - 2) : nr_c;

    for (int i = 0; i < NCHANS; i++)
    {
        int is_chroma = !!i;
        int w    = width  >> is_chroma;
        int h    = height >> is_chroma;
        int refs = p->m_stride[i];

        for (int y = 0; y < h; y++)
        {
            int do_copy = dirty;
            uint8_t *dst2 = dst + dst_offsets[i] + y * dst_stride[i];
            uint8_t *src  = &p->m_ref[nr_c][i][y * refs];
            int     field = parity ^ tff;
            if (((y ^ (1 - field)) & 1) && !parity)
            {
                src = &p->m_ref[nr_p][i][y * refs];
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

    AllocFilter(filter, frame->width, frame->height);

    int dirty = 1;
    if (filter->m_lastFrameNr != frame->frameNumber)
    {
        if (filter->m_lastFrameNr != (frame->frameNumber - 1))
        {
            memset(filter->m_gotFrames, 0, sizeof(filter->m_gotFrames));
        }
        store_ref(filter, frame->buf,  frame->offsets,
                  frame->pitches, frame->width, frame->height);
        dirty = 0;
    }

    filter_func(
        filter, frame->buf, frame->offsets, frame->pitches,
        frame->width, frame->height, field, frame->top_field_first,
        dirty);

    filter->m_lastFrameNr = frame->frameNumber;

    return 0;
}


static void CleanupFieldorderDeintFilter(VideoFilter * filter)
{
    ThisFilter* f = (ThisFilter*)filter;
    for (int i = 0; i < NCHANS * NREFS; i++)
    {
        uint8_t **p= &f->m_ref[i / NCHANS][i % NCHANS];
        if (*p) free(*p);
        *p= NULL;
    }
}

static VideoFilter *FieldorderDeintFilter(VideoFrameType inpixfmt,
                                          VideoFrameType outpixfmt,
                                          const int *width, const int *height,
                                          const char *options, int threads)
{
    (void) inpixfmt;
    (void) outpixfmt;
    (void) options;
    (void) threads;

    ThisFilter *filter = (ThisFilter *) malloc (sizeof(ThisFilter));
    if (filter == NULL)
    {
        fprintf (stderr, "FieldorderDeint: failed to allocate memory for filter.\n");
        return NULL;
    }

    filter->m_width = 0;
    filter->m_height = 0;
    memset(filter->m_ref, 0, sizeof(filter->m_ref));

    AllocFilter(filter, *width, *height);

    filter->m_vf.filter = &FieldorderDeint;
    filter->m_vf.cleanup = &CleanupFieldorderDeintFilter;
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
