/*
 * filter.h
 */

#ifndef _FILTER_H
#define _FILTER_H

#include "frame.h"

#define MYTHTV_FILTER_PATH PREFIX"/lib/mythtv/filters"

typedef struct FmtConv_
{
    VideoFrameType in;
    VideoFrameType out;
} FmtConv;

#define FMT_NULL {FMT_NONE,FMT_NONE}

typedef struct FilterInfo_
{
    char *symbol;
    char *name;
    char *descript;
    FmtConv *formats;
    char *libname;
} FilterInfo;

typedef struct  VideoFilter_
{
    int (*filter)(struct VideoFilter_ *, VideoFrame *);
    void (*cleanup)(struct VideoFilter_ *);

    void *handle; // Library handle;
    VideoFrameType inpixfmt;
    VideoFrameType outpixfmt;
    char *opts;
    FilterInfo *info;
} VideoFilter;

#define FILT_NULL {NULL,NULL,NULL,NULL,NULL}

#endif // #ifndef _FILTER_H
