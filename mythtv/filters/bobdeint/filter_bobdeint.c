// Moves top and bottom fields of interlaced video to the top and
// bottom halves of the frame.  Video output will display these two halves 
// separately.

#include <stdlib.h>
#include <stdio.h>

#include "mythconfig.h"
#if HAVE_STDINT_H
#include <stdint.h>
#endif

#include <string.h>

#include "filter.h"
#include "frame.h"

typedef struct BDFilter
{
    VideoFilter vf;

    /* functions and variables below here considered "private" */
    unsigned char *tmp_ptr;
    int tmp_size;
    unsigned char *line_state;
    int state_size;
} BDFilter;

#define ODD(_n) (((_n)%2)==1)

static void doSplit(BDFilter *filter, unsigned char *buf, int lines, int width)
{
    /* Algorithm shamelessly stolen from mplayer's vo_yuv4mpeg */
    int i, j, k_start, modv;
    unsigned char *line_state = filter->line_state;
    unsigned char *tmp = filter->tmp_ptr;
    
    modv = lines - 1;
    if (ODD(lines))
        modv = lines;
    memset(line_state, 0, modv);
    k_start = 1;
    line_state[0] = 1;
    while (k_start < modv)
    {
        i = j = k_start;
        memcpy(tmp, &(buf[i*width]), width);
        while (!line_state[j])
        {
            line_state[j] = 1;
            i = j;
            j = j * 2 % modv;
            memcpy(&(buf[i*width]), &(buf[j*width]), width);
        }
        memcpy(&(buf[i*width]), tmp, width);
        while (k_start < modv && line_state[k_start])
            k_start++;
    }
}

int bobDeintFilter(VideoFilter *f, VideoFrame *frame, int field)
{
    (void)field;
    BDFilter *filter = (BDFilter *)(f);
    int width = frame->width;
    int height = frame->height;
    int ymax = height;
    int stride = frame->pitches[0];

    unsigned char *yoff = frame->buf + frame->offsets[0];
    unsigned char *uoff = frame->buf + frame->offsets[1];
    unsigned char *voff = frame->buf + frame->offsets[2];

    if (filter->tmp_size < width) 
    {
        filter->tmp_ptr = (unsigned char *)realloc(
            filter->tmp_ptr, width * sizeof(char));
        filter->tmp_size = width;
    }
    if (filter->state_size < height) 
    {
        filter->line_state = (unsigned char *)realloc(
            filter->line_state, height * sizeof(char));
        filter->state_size = height;
    }

    doSplit(filter, yoff, ymax, stride);
    
    stride = frame->pitches[1];
    ymax = height >> 1;
    doSplit(filter, uoff, ymax, stride);
    doSplit(filter, voff, ymax, stride);

    return 0;
}

void bobDtor(VideoFilter *f)
{
    BDFilter *filter = (BDFilter *)(f);
    if (filter->line_state)
        free(filter->line_state);
    if (filter->tmp_ptr)
        free(filter->tmp_ptr);
}

static VideoFilter *new_filter(VideoFrameType inpixfmt,
                               VideoFrameType outpixfmt,
                               int *width, int *height, char *options,
                               int threads)
{
    BDFilter *filter;
    (void)width;
    (void)height;
    (void)options;
    (void)threads;

    if (inpixfmt != FMT_YV12 || outpixfmt != FMT_YV12)
        return NULL;

    filter = malloc(sizeof(BDFilter));

    if (filter == NULL)
    {
        fprintf(stderr,"Couldn't allocate memory for filter\n");
        return NULL;
    }

    filter->vf.filter = &bobDeintFilter;
    filter->tmp_size = 0;
    filter->tmp_ptr = NULL;
    filter->state_size = 0;
    filter->line_state = NULL;
    filter->vf.cleanup = &bobDtor;
    return (VideoFilter *)filter;
}

static FmtConv FmtList[] = 
{
    { FMT_YV12, FMT_YV12 },
    FMT_NULL
};

const FilterInfo filter_table[] =
{
    {
        .filter_init= &new_filter,
        .name=       (char*)"bobdeint",
        .descript=   (char*)
        "bob deinterlace filter; "
        "splits fields to top and bottom of buffer",
        .formats=    FmtList,
        .libname=    NULL,
    },
    FILT_NULL
};
