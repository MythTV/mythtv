/*
 * filter.h
 */

#ifndef _FILTER_H
#define _FILTER_H

#include "frame.h"

#define MYTHTV_FILTER_PATH "/usr/local/lib/mythtv/filters"

typedef struct  VideoFilter_
{
    int (*filter)(Frame *);
    void (*cleanup)(struct VideoFilter_ *);

    char *name;
    void *handle; // Library handle;
} VideoFilter;


VideoFilter *load_videoFilter(char *filter_name, char *options);
int process_video_filters(Frame *frame, VideoFilter **filters, 
                          int numberFilters);
void filters_cleanup(VideoFilter **filters, int numberFilters);

#endif // #ifndef _FILTER_H
