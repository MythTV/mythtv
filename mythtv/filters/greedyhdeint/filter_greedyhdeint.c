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
#include "mythframe.h"

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
    VideoFilter    m_vf;

    long long      m_framesNr[2];
    int8_t         m_gotFrames[2];
    unsigned char* m_frames[2];
    unsigned char* m_deintFrame;
    long long      m_lastFrameNr;

    int            m_width;
    int            m_height;

    int            m_mmFlags;
    TF_STRUCT;
} ThisFilter;

static void AllocFilter(ThisFilter* filter, int width, int height)
{
    if ((width != filter->m_width) || height != filter->m_height)
    {
        printf("greedyhdeint: size changed from %d x %d -> %d x %d\n", filter->m_width, filter->m_height, width, height);
        if (filter->m_frames[0])
        {
            free(filter->m_frames[0]);
            free(filter->m_frames[1]);
            free(filter->m_deintFrame);
        }
        filter->m_frames[0] = malloc(width * height * 2);
        filter->m_frames[1] = malloc(width * height * 2);
        memset(filter->m_frames[0], 0, width * height * 2);
        memset(filter->m_frames[1], 0, width * height * 2);
        filter->m_deintFrame = malloc(width * height * 2);
        filter->m_width = width;
        filter->m_height = height;
        memset(filter->m_gotFrames, 0, sizeof(filter->m_gotFrames));
        memset(filter->m_framesNr, 0, sizeof(filter->m_framesNr));
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

    if (filter->m_lastFrameNr != frame->frameNumber)
    {
        //this is no double call, really a new frame
        cur_frame = (filter->m_lastFrameNr + 1) & 1;
        last_frame = (filter->m_lastFrameNr) & 1;
        //check if really the previous frame (cause mythtv AutoDeInt behauviour)
        if (filter->m_lastFrameNr != (frame->frameNumber - 1))
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
                        filter->m_frames[cur_frame], 2 * frame->width,
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
        cur_frame = (filter->m_lastFrameNr) & 1;
        last_frame = (filter->m_lastFrameNr + 1) & 1;
        bottom_field = frame->top_field_first? 1 : 0;
    }
    filter->m_gotFrames[cur_frame] = 1;
    filter->m_framesNr[cur_frame] = frame->frameNumber;

    //must be done for first frame or deinterlacing would use an "empty" memory block/frame
    if (!filter->m_gotFrames[last_frame])
        last_frame = cur_frame;

    int valid = 1;
#ifdef MMX
    /* SSE Version has best quality. 3DNOW and MMX a litte bit impure */
    if (filter->m_mmFlags & AV_CPU_FLAG_SSE)
    {
        greedyh_filter_sse(
            filter->m_deintFrame, 2 * frame->width,
            filter->m_frames[cur_frame], filter->m_frames[last_frame],
            bottom_field, field, frame->width, frame->height);
    }
    else if (filter->m_mmFlags & AV_CPU_FLAG_3DNOW)
    {
        greedyh_filter_3dnow(
            filter->m_deintFrame, 2 * frame->width,
            filter->m_frames[cur_frame], filter->m_frames[last_frame],
            bottom_field, field, frame->width, frame->height);
    }
    else if (filter->m_mmFlags & AV_CPU_FLAG_MMX)
    {
        greedyh_filter_mmx(
            filter->m_deintFrame, 2 * frame->width,
            filter->m_frames[cur_frame], filter->m_frames[last_frame],
            bottom_field, field, frame->width, frame->height);
    }
    else
#else
#       warning Greedy HighMotion deinterlace filter requires MMX
        (void)field;
#endif
    {
        /* TODO plain old C implementation */
        (void) bottom_field;
        valid = 0;
    }

#if 0
      apply_chroma_filter(filter->m_deintFrame, frame->width * 2,
                          frame->width, frame->height );
#endif

    /* convert back to yv12, cause myth only works with this format */
    if ( valid) yuy2_to_yv12(
        filter->m_deintFrame, 2 * frame->width,
        frame->buf + frame->offsets[0], frame->pitches[0],
        frame->buf + frame->offsets[1], frame->pitches[1],
        frame->buf + frame->offsets[2], frame->pitches[2],
        frame->width, frame->height);

    filter->m_lastFrameNr = frame->frameNumber;

    return 0;
}

static void CleanupGreedyHDeintFilter(VideoFilter * filter)
{
    ThisFilter* f = (ThisFilter*)filter;
    free(f->m_deintFrame);
    free(f->m_frames[0]);
    free(f->m_frames[1]);
}

static VideoFilter* GreedyHDeintFilter(VideoFrameType inpixfmt,
                                       VideoFrameType outpixfmt,
                                       const int *width, const int *height, const char *options,
                                       int threads)
{
    (void) inpixfmt;
    (void) outpixfmt;
    (void) options;
    (void) threads;

    ThisFilter *filter = (ThisFilter *) malloc (sizeof(ThisFilter));
    if (filter == NULL)
    {
        fprintf (stderr, "GreedyHDeint: failed to allocate memory for filter.\n");
        return NULL;
    }

    filter->m_width = 0;
    filter->m_height = 0;
    memset(filter->m_frames, 0, sizeof(filter->m_frames));
    filter->m_deintFrame = 0;

    AllocFilter(filter, *width, *height);

    init_yuv_conversion();
#ifdef MMX
    filter->m_mmFlags = av_get_cpu_flags();
    TF_INIT(filter);
#else
    filter->m_mmFlags = 0;
#endif
    if (!(filter->m_mmFlags & (AV_CPU_FLAG_SSE|AV_CPU_FLAG_3DNOW|AV_CPU_FLAG_MMX)))
    {
        /* TODO plain old C implementation */
        fprintf (stderr, "GreedyHDeint: Requires MMX extensions.\n");
        CleanupGreedyHDeintFilter(&filter->m_vf);
        free(filter);
        return NULL;
    }

    filter->m_vf.filter = &GreedyHDeint;
    filter->m_vf.cleanup = &CleanupGreedyHDeintFilter;
    return (VideoFilter *) filter;
}

#ifdef MMX
static FmtConv FmtList[] =
{
    { FMT_YV12, FMT_YV12 } ,
    FMT_NULL
};
#endif

const FilterInfo filter_table[] =
{
    {
            .filter_init= &GreedyHDeintFilter,
            .name=       (char*)"greedyhdeint",
            .descript=   (char*)"combines data from several fields to deinterlace with less motion blur",
#ifdef MMX
            .formats=    FmtList,
#endif
            .libname=    NULL
    },
    {
            .filter_init= &GreedyHDeintFilter,
            .name=       (char*)"greedyhdoubleprocessdeint",
            .descript=   (char*)"combines data from several fields to deinterlace with less motion blur",
#ifdef MMX
            .formats=    FmtList,
#endif
            .libname=    NULL
    },FILT_NULL
};

/* vim: set expandtab tabstop=4 shiftwidth=4: */
