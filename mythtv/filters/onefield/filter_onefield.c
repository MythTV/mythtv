// selects one field only from interlaced video (defaulting to top)
// based on linearblend

#include <stdlib.h>
#include <stdio.h>

#include "mythconfig.h"
#if HAVE_STDINT_H
#include <stdint.h>
#endif

#include <string.h>

#include "filter.h"
#include "mythframe.h"

typedef struct OFFilter
{
    VideoFilter vf;

    /* functions and variables below here considered "private" */
    int bottom;
} OFFilter;

int oneFieldFilter(VideoFilter *f, VideoFrame *frame, int field)
{
    (void)field;
    OFFilter *filter = (OFFilter *)(f);
    int height = frame->height;
    int bottom = filter->bottom;
    int stride = frame->pitches[0];
    int ymax = height - 2;
    int y;
    unsigned char *yoff = frame->buf + frame->offsets[0];
    unsigned char *uoff = frame->buf + frame->offsets[1];
    unsigned char *voff = frame->buf + frame->offsets[2];

    for (y = 0; y < ymax; y += 2) 
    {
        unsigned char *src = (bottom ? &(yoff[(y+1)*stride]) : &(yoff[y*stride]));
        unsigned char *dst = (bottom ? &(yoff[y*stride]) : &(yoff[(y+1)*stride]));
        memcpy(dst, src, stride);
    }
 
    stride = frame->pitches[1];
    ymax = height / 2 - 2;
  
    for (y = 0; y < ymax; y += 2)
    {
        unsigned char *src = (bottom ? &(uoff[(y+1)*stride]) : &(uoff[y*stride]));
        unsigned char *dst = (bottom ? &(uoff[y*stride]) : &(uoff[(y+1)*stride]));
        memcpy(dst, src, stride);
        src = (bottom ? &(voff[(y+1)*stride]) : &(voff[y*stride]));
        dst = (bottom ? &(voff[y*stride]) : &(voff[(y+1)*stride]));
        memcpy(dst, src, stride);
    }

    return 0;
}

static VideoFilter *new_filter(VideoFrameType inpixfmt,
                               VideoFrameType outpixfmt,
                               int *width, int *height, char *options,
                               int threads)
{
    OFFilter *filter;
    (void)width;
    (void)height;
    (void)threads;

    if (inpixfmt != FMT_YV12 || outpixfmt != FMT_YV12)
        return NULL;

    filter = malloc(sizeof(OFFilter));

    if (filter == NULL)
    {
        fprintf(stderr,"Couldn't allocate memory for filter\n");
        return NULL;
    }

    filter->vf.filter = &oneFieldFilter;
    filter->bottom = 0;
    if (options != NULL && strstr(options, "bottom") != NULL)
        filter->bottom = 1;

    filter->vf.cleanup = NULL;
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
        .name=       (char*)"onefield",
        .descript=   (char*)
        "one-field-only deinterlace filter; "
        "parameter \"bottom\" for bottom field, otherwise top",
        .formats=    FmtList,
        .libname=    NULL,
    },
    FILT_NULL
};
