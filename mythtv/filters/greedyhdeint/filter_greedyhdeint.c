/* Rewrite of neuron2's GreedyHDeint filter for Avisynth
 *
 * converted for myth by Markus Schulz <msc@antzsystem.de>
 * */

#include <stdlib.h>
#include <stdio.h>

#include "config.h"

#if HAVE_STDINT_H
#include <stdint.h>
#endif

#include <string.h>
#include <math.h>

#include "filter.h"
#include "frame.h"

#include "../mm_arch.h"

#include "color.h"

#if ARCH_X86

#include "greedyhmacros.h"

#define MAXCOMB_DEFAULT          5
#define MOTIONTHRESHOLD_DEFAULT 25
#define MOTIONSENSE_DEFAULT     30

static unsigned int GreedyMaxComb = MAXCOMB_DEFAULT;
static unsigned int GreedyMotionThreshold = MOTIONTHRESHOLD_DEFAULT;
static unsigned int GreedyMotionSense = MOTIONSENSE_DEFAULT;

#define IS_MMX
#define SSE_TYPE MMXT
#define FUNCT_NAME greedyh_filter_mmx
#include "greedyh.asm"
#undef SSE_TYPE
#undef IS_MMX
#undef FUNCT_NAME

#define IS_SSE
#define SSE_TYPE SSE
#define FUNCT_NAME greedyh_filter_sse
#include "greedyh.asm"
#undef SSE_TYPE
#undef IS_SSE
#undef FUNCT_NAME

#define IS_3DNOW
#define FUNCT_NAME greedyh_filter_3dnow
#define SSE_TYPE 3DNOW
#include "greedyh.asm"
#undef SSE_TYPE
#undef IS_3DNOW
#undef FUNCT_NAME


#endif



#ifdef MMX
#include "ffmpeg-mmx.h"

static const mmx_t mm_cpool[] =
{
    { 0x0000000000000000LL },
};

#else
#define mmx_t int
#endif

typedef struct ThisFilter
{
    VideoFilter vf;

    long long frames_nr[2];
    int8_t got_frames[2];
    unsigned char* frames[2];
    unsigned char* deint_frame;
    long long last_framenr;

    int width;
    int height;

    int mm_flags;
    TF_STRUCT;
} ThisFilter;

static void AllocFilter(ThisFilter* filter, int width, int height)
{
    if ((width != filter->width) || height != filter->height)
    {
        printf("greedyhdeint: size changed from %d x %d -> %d x %d\n", filter->width, filter->height, width, height);
        if (filter->frames[0])
        {
            free(filter->frames[0]);
            free(filter->frames[1]);
            free(filter->deint_frame);
        }
        filter->frames[0] = malloc(width * height * 2);
        filter->frames[1] = malloc(width * height * 2);
        memset(filter->frames[0], 0, width * height * 2);
        memset(filter->frames[1], 0, width * height * 2);
        filter->deint_frame = malloc(width * height * 2);
        filter->width = width;
        filter->height = height;
        memset(filter->got_frames, 0, sizeof(filter->got_frames));
        memset(filter->frames_nr, 0, sizeof(filter->frames_nr));
    }
}

#include <sys/time.h>
#include <time.h>

static int GreedyHDeint (VideoFilter * f, VideoFrame * frame, int field)
{
    ThisFilter *filter = (ThisFilter *) f;

    int last_frame = 0;
    int cur_frame = 0;
    int bottom_field = 0;

    AllocFilter((ThisFilter*)f, frame->width, frame->height);

    if (filter->last_framenr != frame->frameNumber)
    {
        //this is no double call, really a new frame
        cur_frame = (filter->last_framenr + 1) & 1;
        last_frame = (filter->last_framenr) & 1;
        //check if really the previous frame (cause mythtv AutoDeInt behauviour)
        if (filter->last_framenr != (frame->frameNumber - 1))
        {
            cur_frame = frame->frameNumber & 1;
            last_frame = cur_frame;
        }
        bottom_field = frame->top_field_first? 0 : 1;
        switch(frame->codec)
        {
            case FMT_YV12:
                //must convert from yv12 planar to yuv422 packed
                //only needed on first call for this frame
                yv12_to_yuy2(
                        frame->buf + frame->offsets[0], frame->pitches[0],
                        frame->buf + frame->offsets[1], frame->pitches[1],
                        frame->buf + frame->offsets[2], frame->pitches[2],
                        filter->frames[cur_frame], 2 * frame->width,
                        frame->width, frame->height,
                        1 - frame->interlaced_frame);
                break;
            default:
                fprintf(stderr, "Unsupported pixel format.\n");
                return 0;
        }

    }
    else
    {
        //double call
        cur_frame = (filter->last_framenr) & 1;
        last_frame = (filter->last_framenr + 1) & 1;
        bottom_field = frame->top_field_first? 1 : 0;
    }
    filter->got_frames[cur_frame] = 1;
    filter->frames_nr[cur_frame] = frame->frameNumber;

    //must be done for first frame or deinterlacing would use an "empty" memory block/frame
    if (!filter->got_frames[last_frame])
        last_frame = cur_frame;

#ifdef MMX
    /* SSE Version has best quality. 3DNOW and MMX a litte bit impure */
    if (filter->mm_flags & AV_CPU_FLAG_SSE)
    {
        greedyh_filter_sse(
            filter->deint_frame, 2 * frame->width,
            filter->frames[cur_frame], filter->frames[last_frame],
            bottom_field, field, frame->width, frame->height);
    }
    else if (filter->mm_flags & AV_CPU_FLAG_3DNOW)
    {
        greedyh_filter_3dnow(
            filter->deint_frame, 2 * frame->width,
            filter->frames[cur_frame], filter->frames[last_frame],
            bottom_field, field, frame->width, frame->height);
    }
    else if (filter->mm_flags & AV_CPU_FLAG_MMX)
    {
        greedyh_filter_mmx(
            filter->deint_frame, 2 * frame->width,
            filter->frames[cur_frame], filter->frames[last_frame],
            bottom_field, field, frame->width, frame->height);
    }
    else
#endif
    {
        /* TODO plain old C implementation */
        (void) bottom_field;
    }

#if 0
      apply_chroma_filter(filter->deint_frame, frame->width * 2,
                          frame->width, frame->height );
#endif

    /* convert back to yv12, cause myth only works with this format */
    yuy2_to_yv12(
        filter->deint_frame, 2 * frame->width,
        frame->buf + frame->offsets[0], frame->pitches[0],
        frame->buf + frame->offsets[1], frame->pitches[1],
        frame->buf + frame->offsets[2], frame->pitches[2],
        frame->width, frame->height);

    filter->last_framenr = frame->frameNumber;

    return 0;
}

static void CleanupGreedyHDeintFilter(VideoFilter * filter)
{
    ThisFilter* f = (ThisFilter*)filter;
    free(f->deint_frame);
    free(f->frames[0]);
    free(f->frames[1]);
}

static VideoFilter* GreedyHDeintFilter(VideoFrameType inpixfmt,
                                       VideoFrameType outpixfmt,
                                       int *width, int *height, char *options,
                                       int threads)
{
    ThisFilter *filter;
    (void) height;
    (void) options;
    (void) threads;

    filter = (ThisFilter *) malloc (sizeof(ThisFilter));
    if (filter == NULL)
    {
        fprintf (stderr, "GreedyHDeint: failed to allocate memory for filter.\n");
        return NULL;
    }

    filter->width = 0;
    filter->height = 0;
    memset(filter->frames, 0, sizeof(filter->frames));
    filter->deint_frame = 0;

    AllocFilter(filter, *width, *height);

    init_yuv_conversion();
#ifdef MMX
    filter->mm_flags = av_get_cpu_flags();
    TF_INIT(filter);
#else
    filter->mm_flags = 0;
#endif

    filter->vf.filter = &GreedyHDeint;
    filter->vf.cleanup = &CleanupGreedyHDeintFilter;
    return (VideoFilter *) filter;
}

static FmtConv FmtList[] =
{
    { FMT_YV12, FMT_YV12 } ,
    FMT_NULL
};

const FilterInfo filter_table[] =
{
    {
            .filter_init= &GreedyHDeintFilter,
            .name=       (char*)"greedyhdeint",
            .descript=   (char*)"combines data from several fields to deinterlace with less motion blur",
            .formats=    FmtList,
            .libname=    NULL
    },
    {
            .filter_init= &GreedyHDeintFilter,
            .name=       (char*)"greedyhdoubleprocessdeint",
            .descript=   (char*)"combines data from several fields to deinterlace with less motion blur",
            .formats=    FmtList,
            .libname=    NULL
    },FILT_NULL
};

/* vim: set expandtab tabstop=4 shiftwidth=4: */
