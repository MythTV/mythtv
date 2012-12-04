/*
 * filter.h
 */

#ifndef _FILTER_H
#define _FILTER_H

#include "frame.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FmtConv_
{
    VideoFrameType in;
    VideoFrameType out;
} FmtConv;

#define FMT_NULL {FMT_NONE,FMT_NONE}

typedef struct VideoFilter_ VideoFilter;

typedef VideoFilter*(*init_filter)(int, int, int *, int *, char *, int);

typedef struct FilterInfo_
{
    init_filter filter_init;
    char *name;
    char *descript;
    FmtConv *formats;
    char *libname;
} FilterInfo;

struct VideoFilter_
{
    int (*filter)(struct VideoFilter_ *, VideoFrame *, int);
    void (*cleanup)(struct VideoFilter_ *);

    void *handle; /* Library handle */
    VideoFrameType inpixfmt;
    VideoFrameType outpixfmt;
    char *opts;
    FilterInfo *info;
};

#define FILT_NULL {NULL,NULL,NULL,NULL,NULL}

#ifdef TIME_FILTER

#ifndef TF_INTERVAL
#define TF_INTERVAL 300
#endif /* TF_INTERVAL */

#ifdef TF_TYPE_TSC
#include <stdio.h>

#define TF_STRUCT int tf_frames; unsigned long long tf_ticks;
#define TF_INIT(filter) (filter)->tf_frames=0; (filter)->tf_ticks=0;
#define TF_VARS unsigned int t1l,t1h,t2l,t2h;
#define TF_START __asm__ __volatile__ ("rdtsc" :"=a" (t1l), "=d" (t1h));

#ifdef TF_TSC_TICKS
#define TF_END(filter, prefix) \
    __asm__ __volatile__ ("rdtsc" :"=a" (t2l), "=d" (t2h)); \
    (filter)->tf_ticks += (((unsigned long long)t2h<<32)+t2l)-(((unsigned long long)t1h<<32)+t1l); \
    (filter)->tf_frames = ( (filter)->tf_frames + 1 ) % TF_INTERVAL; \
    if (!(filter)->tf_frames) \
    { \
        fprintf(stderr, prefix \
                "filter timed at %0.2f frames per second\n", \
                TF_INTERVAL / ((double)(filter)->tf_ticks / TF_TSC_TICKS)); \
        (filter)->tf_ticks = 0; \
    }
#else /* TF_TSC_TICKS */
#define TF_END(filter, prefix) \
    __asm__ __volatile__ ("rdtsc" :"=a" (t2l), "=d" (t2h)); \
    (filter)->tf_ticks += (((unsigned long long)t2h<<32)+t2l)-(((unsigned long long)t1h<<32)+t1l); \
    (filter)->tf_frames = ( (filter)->tf_frames + 1 ) % TF_INTERVAL; \
    if (!(filter)->tf_frames) \
    { \
        fprintf(stderr, prefix \
                "filter timed at %lld ticks per frame\n", \
                (filter)->tf_ticks / TF_INTERVAL); \
        (filter)->tf_ticks = 0; \
    }
#endif /* TF_TSC_TICKS */
#else /* TF_TYPE_TSC */
#include <sys/time.h>
#include <stdio.h>
#define TF_STRUCT int tf_frames; double tf_seconds;
#define TF_INIT(filter) (filter)->tf_frames=0; (filter)->tf_seconds=0;
#define TF_VARS struct timeval tf_1, tf_2;
#define TF_START gettimeofday(&tf_1, NULL);
#define TF_END(filter,prefix) \
    gettimeofday(&tf_2, NULL); \
    (filter)->tf_seconds += (tf_2.tv_sec - tf_1.tv_sec) \
        + (tf_2.tv_usec - tf_1.tv_usec) * .000001; \
    (filter)->tf_frames = ( (filter)->tf_frames + 1 ) % TF_INTERVAL; \
    if (!(filter)->tf_frames) \
    { \
        fprintf(stderr, prefix \
                "filter timed at %0.2f frames per second\n", \
                TF_INTERVAL / (filter)->tf_seconds); \
        (filter)->tf_seconds = 0; \
    }
#endif /* TF_TYPE_TSC */

#else /* TIME_FILTER */
#define TF_INIT(filter)
#define TF_STRUCT
#define TF_VARS
#define TF_START
#define TF_END(filter,prefix)
#endif /* TIME_FILTER */

#ifdef __cplusplus
}
#endif

#endif /* #ifndef _FILTER_H */
