/*
 *  filter_invert
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "filter.h"
#include "frame.h"

int invert(VideoFilter *vf, VideoFrame *frame)
{  
    int size = frame->size;
    unsigned char *buf = frame->buf;
    (void)vf;

    while (size--)
    {
        *buf = 255 - (*buf);
        buf++;
    }

    return 0;
}

VideoFilter *new_filter(VideoFrameType inpixfmt, VideoFrameType outpixfmt, 
                        int *width, int *height, char *options)
{
    VideoFilter *filter;
    
    (void)width;
    (void)height;
    (void)options;

    if ((inpixfmt != outpixfmt) || 
        (inpixfmt != FMT_YV12 && inpixfmt != FMT_RGB24 && 
         inpixfmt != FMT_YUV422P) )
        return NULL;
    
    filter = malloc(sizeof(VideoFilter));

    if (filter == NULL)
    {
        fprintf(stderr,"Couldn't allocate memory for filter\n");
        return NULL;
    }
    filter->filter = &invert;
    filter->cleanup = NULL;
    return filter;
}

static FmtConv FmtList[] =
{
    { FMT_YV12, FMT_YV12 },
    { FMT_YUV422P, FMT_YUV422P },
    { FMT_RGB24, FMT_RGB24 },
    FMT_NULL
};

FilterInfo filter_table[] =
{
    {
        symbol:     "new_filter",
        name:       "invert",
        descript:   "inverts the colors of the input video",
        formats:    FmtList,
        libname:    NULL
    },
    FILT_NULL
};
