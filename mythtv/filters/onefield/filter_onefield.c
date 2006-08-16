// selects one field only from interlaced video (defaulting to top)
// based on linearblend

#include <stdlib.h>
#include <stdio.h>

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include <string.h>

#include "filter.h"
#include "frame.h"

typedef struct OFFilter
{
    int (*filter)(VideoFilter *, VideoFrame *);
    void (*cleanup)(VideoFilter *);

    void *handle; // Library handle;
    VideoFrameType inpixfmt;
    VideoFrameType outpixfmt;
    char *opts;
    FilterInfo *info;

    /* functions and variables below here considered "private" */
    int bottom;
} OFFilter;

int oneFieldFilter(VideoFilter *f, VideoFrame *frame)
{
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

VideoFilter *new_filter(VideoFrameType inpixfmt, VideoFrameType outpixfmt, 
                        int *width, int *height, char *options)
{
    OFFilter *filter;
    (void)width;
    (void)height;

    if (inpixfmt != FMT_YV12 || outpixfmt != FMT_YV12)
        return NULL;

    filter = malloc(sizeof(OFFilter));

    if (filter == NULL)
    {
        fprintf(stderr,"Couldn't allocate memory for filter\n");
        return NULL;
    }

    filter->filter = &oneFieldFilter;
    filter->bottom = 0;
    if (options != NULL && strstr(options, "bottom") != NULL)
        filter->bottom = 1;

    filter->cleanup = NULL;
    return (VideoFilter *)filter;
}

static FmtConv FmtList[] = 
{
    { FMT_YV12, FMT_YV12 },
    FMT_NULL
};

FilterInfo filter_table[] = 
{
    {
        symbol:     "new_filter",
        name:       "onefield",
        descript:   "one-field-only deinterlace filter; parameter \"bottom\" for bottom field, otherwise top",
        formats:    FmtList,
        libname:    NULL,
    },
    FILT_NULL
};
