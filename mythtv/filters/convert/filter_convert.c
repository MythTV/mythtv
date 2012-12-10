/* format conversion filter
*/

#include <stdlib.h>
#include <stdio.h>

#if HAVE_STDINT_H
#include <stdint.h>
#endif

#include <string.h>
#include <math.h>

#include "filter.h"
#include "frame.h"

/*
  Disabled conversion filter Oct 2, 2006 -- dtk
  This filter is broken. First off the conversion is wrong,
  422P has the same height as YV12 and twice the width in the chroma,
  this is written as if the chroma is half the height and has the
  same width as YV12. Second, this filter assumes that the memory
  of YV12 is in one block, and that we have allocated width * height * 2
  bytes. In fact if this is an XVideo surface the memory may be in
  up to three segments, and we only allocate width * height * 3 / 2
  bytes when it is continuous. In order for this filter to work
  we need to change the VideoFilter::filter function to take a
  separate input and output video frames.
*/

typedef struct ThisFilter
{
    VideoFilter vf;

    int uoff;
    int cwidth;
    int cheight;
    int icsize;
    int ocsize;
    int osize;
    TF_STRUCT;
} ThisFilter;

static int
Cvt422420 (VideoFilter * f, VideoFrame * frame, int field)
{
    (void)field;
    ThisFilter * filter = (ThisFilter *) f;
    int X, Y, field;
    unsigned char * lineout, * linein0, * linein1;
    TF_VARS;

    TF_START;
    for (field = 0; field < 2; field++)
    {
        lineout = frame->buf + filter->uoff + field * filter->ocsize;
        linein0 = frame->buf + filter->uoff + field * filter->icsize;
        linein1 = linein0 + filter->cwidth;
        for (Y = 0; Y < filter->cheight; Y++)
        {
            for (X = 0; X < filter->cwidth; X++)
                lineout[X] = (linein0[X] + linein1[X]) / 2;
            lineout += filter->cwidth;
            linein0 += filter->cwidth * 2;
            linein1 += filter->cwidth * 2;
        }
    }
    frame->size = filter->osize;
    frame->codec = FMT_YV12;
    TF_END(filter, "ConvertYUV422P->YV12: ");
    return 0;
}

static int
Cvt420422 (VideoFilter * f, VideoFrame * frame, int field)
{
    (void)field;
    ThisFilter * filter = (ThisFilter *) f;
    int X, Y, field;
    unsigned char * lineout0, * lineout1, * linein;
    TF_VARS;

    TF_START;
    for (field = 1; field > -1; field--)
    {
        lineout0 = frame->buf + filter->uoff + filter->ocsize + field * filter->ocsize;
        lineout1 = lineout0 + filter->cwidth;
        linein = frame->buf + filter->uoff + filter->icsize + field * filter->icsize;
        for (Y = 0; Y < filter->cheight; Y++)
        {
            lineout0 -= filter->cwidth * 2;
            lineout1 -= filter->cwidth * 2;
            linein -= filter->cwidth;
            for (X = 0; X < filter->cwidth; X++)
                lineout0[X] = lineout1[X] = linein[X];
        }
    }
    frame->size = filter->osize;
    frame->codec = FMT_YUV422P;
    TF_END(filter, "ConvertYV12->YUV420P: ");
    return 0;
}

VideoFilter *
newConvertFilter (VideoFrameType inpixfmt, VideoFrameType outpixfmt, 
                    int *width, int *height, char *options, int threads)
{
    ThisFilter *filter;

    (void) threads;
    (void) options;
    if ((inpixfmt != FMT_YUV422P || outpixfmt != FMT_YV12) &&
        (inpixfmt != FMT_YV12 || outpixfmt != FMT_YUV422P) &&
        (inpixfmt != outpixfmt))
        return NULL;

    filter = malloc (sizeof (ThisFilter));
    if (filter == NULL)
    {
        fprintf (stderr, "Convert: failed to allocate memory for filter\n");
        return NULL;
    }

    if (inpixfmt == FMT_YV12 && outpixfmt == FMT_YUV422P)
    {
        filter->vf.filter = &Cvt420422;
        filter->uoff = *width * *height;
        filter->cwidth = *width / 2;
        filter->cheight = *height / 2;
        filter->icsize = *width * *height / 4;
        filter->ocsize = *width * *height / 2;
        filter->osize = *width * *height * 2;
    }
    else if (inpixfmt == FMT_YUV422P && outpixfmt == FMT_YV12)
    {
        filter->vf.filter = &Cvt422420;
        filter->uoff = *width * *height;
        filter->cwidth = *width / 2;
        filter->cheight = *height / 2;
        filter->icsize = *width * *height / 2;
        filter->ocsize = *width * *height / 4;
        filter->osize = *width * *height * 3 / 2;
    }
    else if (inpixfmt == outpixfmt)
        filter->vf.filter = NULL;
    filter->vf.cleanup = NULL;
    TF_INIT(filter);
    return (VideoFilter *) filter;
}

#if 0
static FmtConv FmtList[] = 
{
    { FMT_YV12, FMT_YUV422P },
    { FMT_YUV422P, FMT_YV12 },
    FMT_NULL
};

FilterInfo filter_table[] = 
{
    {
        .filter_init= &newConvertFilter,
        .name=       "convert",
        .descript=   "converts between various video types",
        .formats=    FmtList,
        .libname=    NULL
    },
    FILT_NULL
};
#endif
