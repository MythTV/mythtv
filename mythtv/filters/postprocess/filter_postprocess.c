/*
 *  filter_postprocess
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "mythconfig.h"

#if HAVE_STDINT_H
#include <stdint.h>
#endif

#include "libavcodec/avcodec.h"
#include "filter.h"
#include "frame.h"
#include "libpostproc/postprocess.h"

//static const char FILTER_NAME[] = "PostProcess";

typedef struct ThisFilter
{
    VideoFilter vf;

    pp_mode *mode;
    pp_context *context;
    int width;
    int height;
    int ysize;
    int csize;
    unsigned char *src[3];
    unsigned char *dst[3];
    int srcStride[3];
    int dstStride[3];
    int eprint;
    TF_STRUCT;
} ThisFilter;


static int pp(VideoFilter *vf, VideoFrame *frame, int field)
{
    (void)field;
    ThisFilter* tf = (ThisFilter*)vf;
    TF_VARS;

    TF_START;

    tf->src[0] = tf->dst[0] = frame->buf;
    tf->src[1] = tf->dst[1] = frame->buf + tf->ysize;
    tf->src[2] = tf->dst[2] = frame->buf + tf->ysize + tf->csize;

    if (frame->qscale_table == NULL)
        frame->qstride = 0;

    tf->ysize = (frame->width) * (frame->height);
    tf->csize = tf->ysize / 4;

    tf->width = frame->width;
    tf->height = frame->height;

    tf->srcStride[0] = tf->ysize / (tf->height);
    tf->srcStride[1] = tf->csize / (tf->height) * 2;
    tf->srcStride[2] = tf->csize / (tf->height) * 2;

    tf->dstStride[0] = tf->ysize / (tf->height);
    tf->dstStride[1] = tf->csize / (tf->height) * 2;
    tf->dstStride[2] = tf->csize / (tf->height) * 2;

    pp_postprocess( (const uint8_t**)tf->src, tf->srcStride,
                    tf->dst, tf->dstStride,
                    frame->width, frame->height,
                    (signed char *)(frame->qscale_table), frame->qstride,
                    tf->mode, tf->context, PP_FORMAT_420);

    TF_END(tf, "PostProcess: ");
    return 0;
}

static void cleanup(VideoFilter *filter)
{
    pp_free_context(((ThisFilter*)filter)->context);
    pp_free_mode(((ThisFilter*)filter)->mode);
}

static VideoFilter *new_filter(VideoFrameType inpixfmt,
                               VideoFrameType outpixfmt,
                               int *width, int *height, char *options,
                               int threads)
{
    (void) threads;
    ThisFilter *filter;

    if ( inpixfmt != FMT_YV12 || outpixfmt != FMT_YV12 )
        return NULL;

    filter = (ThisFilter*) malloc(sizeof(ThisFilter));
    if (filter == NULL)
    {
        fprintf(stderr,"Couldn't allocate memory for filter\n");
        return NULL;
    }

    filter->context = pp_get_context(*width, *height,
                            PP_CPU_CAPS_MMX|PP_CPU_CAPS_MMX2|PP_CPU_CAPS_3DNOW);
    if (filter->context == NULL)
    {
        fprintf(stderr,"PostProc: failed to get PP context\n");
        free(filter);
        return NULL;
    }

    printf("Filteroptions: %s\n", options);
    filter->mode = pp_get_mode_by_name_and_quality(options, PP_QUALITY_MAX);
    if (filter->mode == NULL)
    {
        printf("%s", pp_help);
        free(filter);
        return NULL;
    }

    filter->eprint = 0;

    filter->vf.filter = &pp;
    filter->vf.cleanup = &cleanup;
    TF_INIT(filter);
    return (VideoFilter *)filter;
}

FmtConv FmtList[] = 
{
    { FMT_YV12, FMT_YV12 },
    FMT_NULL
};

const FilterInfo filter_table[] =
{
    {
        .filter_init= &new_filter,
        .name=       (char*)"postprocess",
        .descript=   (char*)"FFMPEG's postprocessing filters",
        .formats=    FmtList,
        .libname=    NULL
    },
    FILT_NULL
};
