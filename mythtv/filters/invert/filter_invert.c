/*
 *  filter_invert
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

int invert(VideoFilter *vf, VideoFrame *frame, int field)
{
    (void)field;
    int size = frame->size;
    unsigned char *buf = frame->buf;
    TF_VARS;

    (void)vf;

    TF_START;


    while (size--)
    {
        *buf = 255 - (*buf);
        buf++;
    }

    TF_END((ThisFilter *)vf, "Invert");

    return 0;
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
        fprintf(stderr,"Couldn't allocate memory for filter\n");
        return NULL;
    }
    filter->vf.filter = &invert;
    filter->vf.cleanup = NULL;
    TF_INIT(filter)
    return (VideoFilter *) filter;
}

static FmtConv FmtList[] =
{
    { FMT_YV12, FMT_YV12 },
    { FMT_YUV422P, FMT_YUV422P },
    { FMT_RGB24, FMT_RGB24 },
    FMT_NULL
};

const FilterInfo filter_table[] =
{
    {
        .filter_init= &new_filter,
        .name=       (char*)"invert",
        .descript=   (char*)"inverts the colors of the input video",
        .formats=    FmtList,
        .libname=    NULL
    },
    FILT_NULL
};
