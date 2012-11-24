/*
 *  filter_vflip
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "filter.h"
#include "frame.h"

typedef struct ThisFilter
{
    VideoFilter vf;
    TF_STRUCT;
} ThisFilter;

static void reverse_memcpy(unsigned char *dst, const unsigned char *src, int n)
{
    int i;
    for (i = 0; i < n; ++i)
        dst[n - 1 - i] = src[i];
}

static int swap(VideoFrame *frame, int datasize, int offset, int shift)
{
    int i, j;
    int oldoffset;
    int newoffset;
    unsigned char *temp = malloc(datasize);
    if (temp == NULL)
    {
        fprintf(stderr, "Couldn't allocate memory for temp\n");
        return 0;
    }
    for (i = 0, j = (frame->height - 1); i < frame->height / 2; i++, j--)
    {
        newoffset = i * datasize + offset;
        if (shift)
            newoffset += datasize;
        oldoffset = j * datasize + offset;
        if (!shift || i != frame->height - 1)
        {
            memcpy(temp, frame->buf + newoffset,
                   datasize); // new -> temp
            reverse_memcpy(frame->buf + newoffset, frame->buf + oldoffset,
                           datasize); // old -> new
            reverse_memcpy(frame->buf + oldoffset, temp,
                           datasize); // temp -> old
        }
    }
    if (temp)
        free(temp);

    return 1;
}

static int comp(const void *va, const void *vb)
{
    int *a = (int *) va;
    int *b = (int *) vb;
    if (*a == *b)
        return 0;
    else if (*a < *b)
        return -1;
    else
        return 1;
}

static int vflip(VideoFilter *vf, VideoFrame *frame, int field)
{
    (void)field;

    if (frame->codec != FMT_YV12)
    {
        fprintf(stderr, "codec %d unsupported for vflip, skipping\n",
                frame->codec);
        return 0;
    }

    TF_VARS;

    (void)vf;

    TF_START;

    int ouroffsets[3];
    memcpy(ouroffsets, frame->offsets, 3 * sizeof(int));
    qsort(ouroffsets, 3, sizeof(int), comp);

    int datasize;
    datasize = (ouroffsets[1] - ouroffsets[0]) / frame->height;
    if (!swap(frame, datasize, ouroffsets[0], 0))
        return 0;
    datasize = (ouroffsets[2] - ouroffsets[1]) / frame->height;
    if (!swap(frame, datasize, ouroffsets[1], 0))
        return 0;
    datasize = (frame->size - ouroffsets[2]) / frame->height;
    if (!swap(frame, datasize, ouroffsets[2], 0))
        return 0;

    TF_END((ThisFilter *)vf, "VFlip");

    return 1;
}

static VideoFilter *new_filter(VideoFrameType inpixfmt,
                               VideoFrameType outpixfmt,
                               int *width, int *height, char *options,
                               int threads)
{
    ThisFilter *filter;

    (void)width;
    (void)height;
    (void)options;
    (void)threads;

    if ((inpixfmt != outpixfmt) ||
        (inpixfmt != FMT_YV12 && inpixfmt != FMT_RGB24 &&
         inpixfmt != FMT_YUV422P) )
        return NULL;

    filter = malloc(sizeof(ThisFilter));

    if (filter == NULL)
    {
        fprintf(stderr, "Couldn't allocate memory for filter\n");
        return NULL;
    }
    filter->vf.filter = &vflip;
    filter->vf.cleanup = NULL;
    TF_INIT(filter)
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
        filter_init: &new_filter,
        name:       (char*)"vflip",
        descript:   (char*)"flips the video image vertically",
        formats:    FmtList,
        libname:    NULL
    },
    FILT_NULL
};
