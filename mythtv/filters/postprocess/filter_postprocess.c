/*
 *  filter_postprocess
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

#include "avcodec.h"
#include "filter.h"
#include "frame.h"
#include "libpostproc/postprocess.h"

static const char FILTER_NAME[] = "PostProcess";

typedef struct ThisFilter
{
    int (*filter)(VideoFilter *, VideoFrame *);
    void (*cleanup)(VideoFilter *);

    char *name;
    void *handle; // Library handle;

    pp_mode_t* mode;
    pp_context_t* context;
    int width;
    int height;
    int ysize;
    int csize;
    unsigned char* src[3];
    unsigned char* dst[3];
    int srcStride[3];
    int dstStride[3];
    int eprint;
} ThisFilter;


int pp(VideoFilter *vf, VideoFrame *frame)
{
    ThisFilter* tf = (ThisFilter*)vf;

    if (frame->codec != FMT_YV12)
    {
        if (tf->eprint)
            return 0;
        printf("ERROR: PostProcess filter only works on YV12 for now!\n");
        tf->eprint = 1;
    }

    if (tf->width != frame->width || tf->height != frame->height)
    {
        printf("Reinitializing filter (%ux%u -> %ux%u)\n",
            tf->width, tf->height, frame->width, frame->height);

        if (tf->context)
            pp_free_context(tf->context);
        tf->context = NULL;

        tf->context = pp_get_context(frame->width, frame->height,
                            PP_CPU_CAPS_MMX|PP_CPU_CAPS_MMX2|PP_CPU_CAPS_3DNOW);

        tf->ysize = frame->size/3*2;
        tf->csize = (frame->size - tf->ysize) / 2;

        tf->width = frame->width;
        tf->height = frame->height;

        tf->srcStride[0] = tf->ysize / frame->height;
        tf->srcStride[1] = tf->csize / frame->height * 2;
        tf->srcStride[2] = tf->csize / frame->height * 2;

        tf->dstStride[0] = tf->ysize / frame->height;
        tf->dstStride[1] = tf->csize / frame->height * 2;
        tf->dstStride[2] = tf->csize / frame->height * 2;
    }

    tf->src[0] = tf->dst[0] = frame->buf;
    tf->src[1] = tf->dst[1] = frame->buf + tf->ysize;
    tf->src[2] = tf->dst[2] = frame->buf + tf->ysize + tf->csize;

    if (frame->qscale_table == NULL)
        frame->qstride = 0;

    pp_postprocess( tf->src, tf->srcStride,
                    tf->dst, tf->dstStride,
                    frame->width, frame->height,
                    frame->qscale_table, frame->qstride,
                    tf->mode, tf->context, PP_FORMAT_420);

    return 0;
}

void cleanup(VideoFilter *filter)
{
    pp_free_mode(((ThisFilter*)filter)->mode);
    free(filter);
}

VideoFilter *new_filter(char *options)
{
    ThisFilter *filter = malloc(sizeof(ThisFilter));

    if (filter == NULL)
    {
        fprintf(stderr,"Couldn't allocate memory for filter\n");
        return NULL;
    }

    printf("Filteroptions: %s\n", options);
    filter->mode = pp_get_mode_by_name_and_quality(options, PP_QUALITY_MAX);
    if (filter->mode == NULL)
    {
        printf("%s", pp_help);
        return NULL;
    }

    filter->eprint = 0;
    filter->context = NULL;
    filter->height = 0;
    filter->width = 0;

    filter->filter = &pp;
    filter->cleanup = &cleanup;
    filter->name = (char *)FILTER_NAME;
    return (VideoFilter *)filter;
}

