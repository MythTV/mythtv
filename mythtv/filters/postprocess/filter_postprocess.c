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
#include "mythframe.h"
#include "libpostproc/postprocess.h"

//static const char FILTER_NAME[] = "PostProcess";

typedef struct ThisFilter
{
    VideoFilter m_vf;

    pp_mode       *m_mode;
    pp_context    *m_context;
    int            m_width;
    int            m_height;
    int            m_ySize;
    int            m_cSize;
    unsigned char *m_src[3];
    unsigned char *m_dst[3];
    int            m_srcStride[3];
    int            m_dstStride[3];
    int            m_eprint;
    TF_STRUCT;
} ThisFilter;


static int pp(VideoFilter *vf, VideoFrame *frame, int field)
{
    (void)field;
    ThisFilter* tf = (ThisFilter*)vf;
    TF_VARS;

    TF_START;

    tf->m_src[0] = tf->m_dst[0] = frame->buf;
    tf->m_src[1] = tf->m_dst[1] = frame->buf + tf->m_ySize;
    tf->m_src[2] = tf->m_dst[2] = frame->buf + tf->m_ySize + tf->m_cSize;

    if (frame->qscale_table == NULL)
        frame->qstride = 0;

    tf->m_ySize = (frame->width) * (frame->height);
    tf->m_cSize = tf->m_ySize / 4;

    tf->m_width = frame->width;
    tf->m_height = frame->height;

    tf->m_srcStride[0] = tf->m_ySize / (tf->m_height);
    tf->m_srcStride[1] = tf->m_cSize / (tf->m_height) * 2;
    tf->m_srcStride[2] = tf->m_cSize / (tf->m_height) * 2;

    tf->m_dstStride[0] = tf->m_ySize / (tf->m_height);
    tf->m_dstStride[1] = tf->m_cSize / (tf->m_height) * 2;
    tf->m_dstStride[2] = tf->m_cSize / (tf->m_height) * 2;

    pp_postprocess( (const uint8_t**)tf->m_src, tf->m_srcStride,
                    tf->m_dst, tf->m_dstStride,
                    frame->width, frame->height,
                    (signed char *)(frame->qscale_table), frame->qstride,
                    tf->m_mode, tf->m_context, PP_FORMAT_420);

    TF_END(tf, "PostProcess: ");
    return 0;
}

static void cleanup(VideoFilter *filter)
{
    pp_free_context(((ThisFilter*)filter)->m_context);
    pp_free_mode(((ThisFilter*)filter)->m_mode);
}

static VideoFilter *new_filter(VideoFrameType inpixfmt,
                               VideoFrameType outpixfmt,
                               const int *width, const int *height, const char *options,
                               int threads)
{
    (void) threads;

    if ( inpixfmt != FMT_YV12 || outpixfmt != FMT_YV12 )
        return NULL;

    ThisFilter *filter = (ThisFilter*) malloc(sizeof(ThisFilter));
    if (filter == NULL)
    {
        fprintf(stderr,"Couldn't allocate memory for filter\n");
        return NULL;
    }

    filter->m_context = pp_get_context(*width, *height,
                            PP_CPU_CAPS_MMX|PP_CPU_CAPS_MMX2|PP_CPU_CAPS_3DNOW);
    if (filter->m_context == NULL)
    {
        fprintf(stderr,"PostProc: failed to get PP context\n");
        free(filter);
        return NULL;
    }

    printf("Filteroptions: %s\n", options);
    filter->m_mode = pp_get_mode_by_name_and_quality(options, PP_QUALITY_MAX);
    if (filter->m_mode == NULL)
    {
        printf("%s", pp_help);
        free(filter);
        return NULL;
    }

    filter->m_eprint = 0;

    filter->m_vf.filter = &pp;
    filter->m_vf.cleanup = &cleanup;
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
