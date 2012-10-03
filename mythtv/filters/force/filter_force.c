/* Not really proper filters, these are all no-ops but they each only "handle"
 * one format, so they can be used to force a format selection
*/

#include <stdlib.h>
#include "filter.h"
#include "frame.h"

static VideoFilter *
new_force_template (VideoFrameType inpixfmt, VideoFrameType outpixfmt,
                    VideoFrameType mypixfmt)
{
    VideoFilter *filter;
    
    if (inpixfmt != mypixfmt || outpixfmt != mypixfmt)
        return NULL;

    filter = (VideoFilter *) malloc(sizeof(VideoFilter));

    if (filter)
    {
        filter->filter = NULL;
        filter->cleanup = NULL;
    }

    return filter;
}

static VideoFilter *
new_force_yv12 (VideoFrameType inpixfmt, VideoFrameType outpixfmt, int *width,
            int *height, char *options, int threads)
{
    (void) width;
    (void) height;
    (void) options;

    return new_force_template (inpixfmt, outpixfmt, FMT_YV12);
}

static VideoFilter *
new_force_yuv422p (VideoFrameType inpixfmt, VideoFrameType outpixfmt, int *width,
            int *height, char *options, int threads)
{
    (void) width;
    (void) height;
    (void) options;

    return new_force_template (inpixfmt, outpixfmt, FMT_YUV422P);
}

static VideoFilter *
new_force_rgb24 (VideoFrameType inpixfmt, VideoFrameType outpixfmt, int *width,
            int *height, char *options, int threads)
{
    (void) width;
    (void) height;
    (void) options;

    return new_force_template (inpixfmt, outpixfmt, FMT_RGB24);
}

static VideoFilter *
new_force_argb32 (VideoFrameType inpixfmt, VideoFrameType outpixfmt, int *width,
            int *height, char *options, int threads)
{
    (void) width;
    (void) height;
    (void) options;
    (void) threads;

    return new_force_template (inpixfmt, outpixfmt, FMT_ARGB32);
}

static FmtConv Fmt_List_YV12[] =
{
    { FMT_YV12, FMT_YV12 },
    FMT_NULL
};

static FmtConv Fmt_List_YUV422P[] =
{
    { FMT_YUV422P, FMT_YUV422P },
    FMT_NULL
};

static FmtConv Fmt_List_RGB24[] =
{
    { FMT_RGB24, FMT_RGB24 },
    FMT_NULL
};

static FmtConv Fmt_List_ARGB32[] =
{
    { FMT_ARGB32, FMT_ARGB32 },
    FMT_NULL
};

const FilterInfo filter_table[] =
{
    {
        filter_init: &new_force_yv12,
        name:       (char*)"forceyv12",
        descript:   (char*)"forces use of YV12 video format",
        formats:    Fmt_List_YV12,
        libname:    NULL
    },
    {
        filter_init: &new_force_yuv422p,
        name:       (char*)"forceyuv422p",
        descript:   (char*)"forces use of YUV422P video format",
        formats:    Fmt_List_YUV422P,
        libname:    NULL
    },
    {
        filter_init: &new_force_rgb24,
        name:       (char*)"forcergb24",
        descript:   (char*)"forces use of RGB24 video format",
        formats:    Fmt_List_RGB24,
        libname:    NULL
    },
    {
        filter_init: &new_force_argb32,
        name:       (char*)"forceargb32",
        descript:   (char*)"forces use of ARGB32 video format",
        formats:    Fmt_List_ARGB32,
        libname:    NULL
    },
    FILT_NULL
};
