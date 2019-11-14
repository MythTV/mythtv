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
#include "mythframe.h"

typedef struct BDFilter
{
    VideoFilter    m_vf;

    /* functions and variables below here considered "private" */
    unsigned char *m_tmpPtr;
    int            m_tmpSize;
    unsigned char *m_lineState;
    int            m_stateSize;
} BDFilter;

#define ODD(_n) (((_n)%2)==1)

static void doSplit(BDFilter *filter, unsigned char *buf, int lines, int width)
{
    /* Algorithm shamelessly stolen from mplayer's vo_yuv4mpeg */
    unsigned char *line_state = filter->m_lineState;
    unsigned char *tmp = filter->m_tmpPtr;
    
    int modv = lines - 1;
    if (ODD(lines))
        modv = lines;
    memset(line_state, 0, modv);
    int k_start = 1;
    line_state[0] = 1;
    while (k_start < modv)
    {
        int i = k_start;
        int j = k_start;
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

    if (filter->m_tmpSize < width)
    {
        filter->m_tmpPtr = (unsigned char *)realloc(
            filter->m_tmpPtr, width * sizeof(unsigned char));
        filter->m_tmpSize = width;
    }
    if (filter->m_stateSize < height)
    {
        filter->m_lineState = (unsigned char *)realloc(
            filter->m_lineState, height * sizeof(unsigned char));
        filter->m_stateSize = height;
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
    if (filter->m_lineState)
        free(filter->m_lineState);
    if (filter->m_tmpPtr)
        free(filter->m_tmpPtr);
}

static VideoFilter *new_filter(VideoFrameType inpixfmt,
                               VideoFrameType outpixfmt,
                               const int *width, const int *height, const char *options,
                               int threads)
{
    (void)width;
    (void)height;
    (void)options;
    (void)threads;

    if (inpixfmt != FMT_YV12 || outpixfmt != FMT_YV12)
        return NULL;

    BDFilter *filter = malloc(sizeof(BDFilter));
    if (filter == NULL)
    {
        fprintf(stderr,"Couldn't allocate memory for filter\n");
        return NULL;
    }

    filter->m_vf.filter = &bobDeintFilter;
    filter->m_tmpSize = 0;
    filter->m_tmpPtr = NULL;
    filter->m_stateSize = 0;
    filter->m_lineState = NULL;
    filter->m_vf.cleanup = &bobDtor;
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
