/*
 *  filter_invert
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "filter.h"
#include "frame.h"

static const char FILTER_NAME[] = "Invert";

typedef struct ThisFilter
{
    int (*filter)(VideoFilter *, VideoFrame *);
    void (*cleanup)(VideoFilter *);

    char *name;
    void *handle; // Library handle;

    /* functions and variables below here considered "private" */

} ThisFilter;


int invert(VideoFilter *vf, VideoFrame *frame)
{  
    int size;
    unsigned char *buf = frame->buf;
    vf = vf;

    if (frame->codec == FMT_RGB24)
        size = frame->width * 3 * frame->height;
    else if (frame->codec == FMT_YV12)
        size = frame->width * 3 / 2 * frame->height;
    else
        return -1;

    while (size--)
    {
        *buf = 255 - (*buf);
        buf++;
    }

    return 0;
}

void cleanup(VideoFilter *filter)
{
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
    options = options;
    filter->filter = &invert;
    filter->cleanup = &cleanup;
    filter->name = (char *)FILTER_NAME;
    return (VideoFilter *)filter;
}

